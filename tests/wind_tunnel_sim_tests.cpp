/**
 * Tests for Wind Tunnel's simulation (WindTunnelSim) and obstacle outlines (WindTunnelShapes): setup, the pressure
 * solve, stability, repeatability, filling outlines into the grid, NACA codes, and the sign of lift and drag.
 */

#include <cmath>

#include "moksha/test_framework/test_framework.h"
#include "wind_tunnel/game/wind_tunnel_shapes.h"
#include "wind_tunnel/game/wind_tunnel_sim.h"

using Samples::WindTunnelSim;
namespace Shapes = Samples::WindTunnelShapes;

static constexpr uint32_t ROWS = 40;
static constexpr float ASPECT = 16.0f / 9.0f;
static constexpr float STEP = 1.0f / 60.0f;

static bool AllFinite(const WindTunnelSim& sim) {
    for (uint32_t i = 0; i < sim.GetCountX(); ++i) {
        for (uint32_t j = 0; j < sim.GetCountY(); ++j) {
            if (!std::isfinite(sim.GetPressure(i, j)) || !std::isfinite(sim.GetSpeed(i, j)) ||
                !std::isfinite(sim.GetSmoke(i, j))) {
                return false;
            }
        }
    }
    return true;
}

TEST(WindTunnelSim, InitRejectsTinyGrids) {
    WindTunnelSim sim;
    EXPECT(!sim.Init(WindTunnelSim::MIN_ROWS - 1, ASPECT));
    EXPECT(!sim.Init(ROWS, 0.0f));
    EXPECT(sim.Init(ROWS, ASPECT));
    EXPECT(sim.GetCountY() == ROWS);
    EXPECT(sim.GetCountX() == 71);  // 40 * 16 / 9 = 71.1
    EXPECT_NEAR(sim.GetCellSize() * ROWS, WindTunnelSim::DOMAIN_HEIGHT, 1e-6f);
}

TEST(WindTunnelSim, WallsAreSolid) {
    WindTunnelSim sim;
    ASSERT(sim.Init(ROWS, ASPECT));
    EXPECT(sim.IsSolid(5, 0));
    EXPECT(sim.IsSolid(5, ROWS - 1));
    EXPECT(sim.IsSolid(0, ROWS / 2));
    EXPECT(!sim.IsSolid(5, ROWS / 2));
    EXPECT(!sim.IsSolid(sim.GetCountX() - 1, ROWS / 2));  // the outlet is open
}

TEST(WindTunnelSim, ProjectionRemovesDivergence) {
    WindTunnelSim sim;
    ASSERT(sim.Init(ROWS, ASPECT));
    sim.SetOutline(Shapes::Circle(0.12f), 0.24f);
    sim.SetObstacle(0.4f, 0.5f, 0.0f, 0.0f, 0.0f);
    for (int step = 0; step < 30; ++step) {
        sim.Step(STEP);
    }
    // Advection leaves the flow a little divergent; the solve takes most of that out.
    const float before = sim.MaxDivergence();
    sim.Project(STEP);
    const float after = sim.MaxDivergence();
    EXPECT(after < before);
    EXPECT(after < 0.05f * sim.GetInflowSpeed() * sim.GetCellSize() * ROWS);
}

TEST(WindTunnelSim, StaysFiniteWhileDragged) {
    WindTunnelSim sim;
    ASSERT(sim.Init(ROWS, ASPECT));
    sim.SetVorticity(1.5f);
    sim.SetInflowSpeed(4.0f);
    sim.SetOutline(Shapes::Aircraft(0.75f), 0.75f);
    for (int step = 0; step < 240; ++step) {
        const float t = static_cast<float>(step) * STEP;
        // Fast and jerky, so the obstacle's cells carry big speeds.
        sim.SetObstacle(0.6f + 0.4f * std::sin(t * 5.0f), 0.5f + 0.3f * std::sin(t * 7.0f), t, 6.0f, -4.0f);
        sim.Step(STEP);
    }
    EXPECT(AllFinite(sim));
}

TEST(WindTunnelSim, SameInputSameResult) {
    WindTunnelSim a;
    WindTunnelSim b;
    ASSERT(a.Init(ROWS, ASPECT));
    ASSERT(b.Init(ROWS, ASPECT));
    a.SetOutline(Shapes::Square(0.1f), 0.2f);
    b.SetOutline(Shapes::Square(0.1f), 0.2f);
    for (int step = 0; step < 60; ++step) {
        a.Step(STEP);
        b.Step(STEP);
    }
    EXPECT(a.Checksum() == b.Checksum());
    b.Step(STEP);
    EXPECT(a.Checksum() != b.Checksum());
}

TEST(WindTunnelSim, SmokeFlowsDownstream) {
    WindTunnelSim sim;
    ASSERT(sim.Init(ROWS, ASPECT));
    sim.SetOutline({}, 1.0f);
    sim.SetSmokeSource(WindTunnelSim::SmokeSource::Band);
    for (int step = 0; step < 30; ++step) {
        sim.Step(STEP);
    }
    // At speed 2, half a second carries the band a whole domain height downstream, along the middle.
    EXPECT(sim.GetSmoke(ROWS / 2, ROWS / 2) > 0.5f);
    EXPECT(sim.GetSmoke(ROWS / 2, 3) < 0.01f);
}

TEST(WindTunnelSim, OutlineFillsItsArea) {
    WindTunnelSim sim;
    ASSERT(sim.Init(100, ASPECT));
    const float radius = 0.2f;
    sim.SetOutline(Shapes::Circle(radius), 2.0f * radius);
    sim.SetObstacle(0.6f, 0.5f, 0.0f, 0.0f, 0.0f);
    const float cells = 3.14159f * radius * radius / (sim.GetCellSize() * sim.GetCellSize());
    EXPECT_NEAR(static_cast<float>(sim.CountObstacleCells()), cells, 0.03f * cells);
    EXPECT(sim.IsSolid(60, 50));
    EXPECT(!sim.IsSolid(60, 75));

    // Turning a square by 45 degrees keeps its area.
    sim.SetOutline(Shapes::Square(0.15f), 0.3f);
    const uint32_t straight = sim.CountObstacleCells();
    sim.SetObstacle(0.6f, 0.5f, 0.785398f, 0.0f, 0.0f);
    const uint32_t turned = sim.CountObstacleCells();
    EXPECT_NEAR(static_cast<float>(turned), static_cast<float>(straight), 0.03f * static_cast<float>(straight));
}

TEST(WindTunnelShapes, NacaCodes) {
    Shapes::Outline foil;
    EXPECT(!Shapes::Naca("241", 1.0f, foil));
    EXPECT(!Shapes::Naca("24a2", 1.0f, foil));
    EXPECT(!Shapes::Naca("23112", 1.0f, foil));  // reflexed 5-digit
    EXPECT(!Shapes::Naca("26012", 1.0f, foil));  // no such camber position
    EXPECT(!Shapes::Naca("2400", 1.0f, foil));   // no thickness
    EXPECT(foil.empty());

    ASSERT(Shapes::Naca("0012", 1.0f, foil));
    ASSERT(foil.size() == 1);
    float minX = 1e9f, maxX = -1e9f, maxY = -1e9f, minY = 1e9f;
    for (const WindTunnelSim::Point& point : foil[0]) {
        minX = std::fmin(minX, point.x);
        maxX = std::fmax(maxX, point.x);
        minY = std::fmin(minY, point.y);
        maxY = std::fmax(maxY, point.y);
    }
    // Pivot at the quarter chord; 12% thick, symmetric.
    EXPECT_NEAR(minX, -0.25f, 1e-4f);
    EXPECT_NEAR(maxX, 0.75f, 1e-4f);
    EXPECT_NEAR(maxY - minY, 0.12f, 0.002f);
    EXPECT_NEAR(maxY, -minY, 1e-4f);

    ASSERT(Shapes::Naca("23012", 1.0f, foil));
    ASSERT(Shapes::Naca("4412", 1.0f, foil));
    float top = -1e9f, bottom = 1e9f;
    for (const WindTunnelSim::Point& point : foil[0]) {
        top = std::fmax(top, point.y);
        bottom = std::fmin(bottom, point.y);
    }
    EXPECT(top > -bottom);  // cambered: more above the chord than below
}

TEST(WindTunnelSim, CamberedFoilLiftsAndPlateDrags) {
    WindTunnelSim sim;
    ASSERT(sim.Init(60, ASPECT));
    Shapes::Outline foil;
    ASSERT(Shapes::Naca("4412", 0.5f, foil));
    sim.SetOutline(foil, 0.5f);
    sim.SetObstacle(0.5f, 0.5f, -0.14f, 0.0f, 0.0f);  // 8 degrees nose up
    float lift = 0.0f;
    for (int step = 0; step < 180; ++step) {
        sim.Step(STEP);
        if (step >= 120) {
            lift += sim.GetLift();
        }
    }
    EXPECT(lift > 0.0f);

    sim.SetOutline(Shapes::Plate(0.15f, 0.02f), 0.3f);
    sim.ResetFlow();
    float drag = 0.0f;
    for (int step = 0; step < 180; ++step) {
        sim.Step(STEP);
        if (step >= 120) {
            drag += sim.GetDrag();
        }
    }
    EXPECT(drag > 0.0f);
}
