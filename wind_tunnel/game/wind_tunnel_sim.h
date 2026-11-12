/**
 * Wind tunnel simulation: an Eulerian fluid on a staggered (MAC) grid, after Matthias Müller's Ten Minute Physics #17
 * (MIT). Each step projects the velocity to be divergence free (Gauss-Seidel with overrelaxation), extrapolates the
 * border velocities, then advects the velocity and the smoke backwards along the flow (semi-Lagrangian). Optional
 * vorticity confinement adds back some of the swirl the advection smears out. Inviscid, and tuned to look good rather
 * than to be accurate.
 *
 * Units: the domain is 1 high and GetWidth() wide; cell (i, j) covers [i*h, (i+1)*h] x [j*h, (j+1)*h], with i = 0 at the
 * left and j = 0 at the bottom. u[i][j] is the x velocity on the cell's left face, v[i][j] the y velocity on its bottom
 * face. The left column blows air in at the inflow speed with smoke, the top and bottom rows are walls, and the right
 * side is open.
 *
 * The obstacle is an outline: one or more polygons around a pivot, unioned. It is moved and turned (positive angles
 * counter-clockwise) and filled into the grid each step by scanlines: a cell is solid when its centre is inside. Its
 * cells carry its velocity, which is what stirs the wake when it is dragged. After each step the pressure on its faces
 * gives the force on it, as lift and drag coefficients against its reference length.
 */

#pragma once

#include <cstdint>
#include <vector>

namespace Samples {

class WindTunnelSim {
   public:
    struct Point {
        float x = 0.0f;
        float y = 0.0f;
    };
    // Polygons in the obstacle's own frame, pivot at the origin, domain units.
    using Outline = std::vector<std::vector<Point>>;

    enum class SmokeSource : uint8_t { Band, Rake, Count };

    static constexpr float DOMAIN_HEIGHT = 1.0f;
    static constexpr uint32_t MIN_ROWS = 8;
    static constexpr uint32_t PRESSURE_ITERATIONS = 40;
    static constexpr float OVER_RELAXATION = 1.9f;
    static constexpr float DENSITY = 1000.0f;

    // Builds a grid `rows` cells high and as wide as `aspect` (width / height) allows, then resets the flow. False if
    // rows < MIN_ROWS or the aspect isn't positive.
    bool Init(uint32_t rows, float aspect);

    // Still air everywhere, inflow at the left, obstacle where it was.
    void ResetFlow();

    // Advances by dt seconds.
    void Step(float dt);

    // The pressure solve alone (part of Step): afterwards the flow is close to divergence free.
    void Project(float dt);

    // The obstacle's shape, and the length its coefficients are measured against (an airfoil's chord).
    void SetOutline(const Outline& newOutline, float referenceLength);

    // Moves the obstacle's pivot to (x, y), turned by angle radians, with its cells carrying velocity (vx, vy).
    void SetObstacle(float x, float y, float angle, float vx, float vy);

    void SetSmokeSource(SmokeSource source);
    void SetInflowSpeed(float speed);
    void SetVorticity(float strength) {
        vorticity = strength;
    }

    // Samples at a point in domain units; values outside clamp to the border.
    float SampleU(float x, float y) const;
    float SampleV(float x, float y) const;

    uint32_t GetCountX() const {
        return numX;
    }
    uint32_t GetCountY() const {
        return numY;
    }
    float GetCellSize() const {
        return h;
    }
    float GetWidth() const {
        return h * static_cast<float>(numX);
    }
    float GetInflowSpeed() const {
        return inflowSpeed;
    }
    SmokeSource GetSmokeSource() const {
        return smokeSource;
    }
    float GetObstacleX() const {
        return obstacleX;
    }
    float GetObstacleY() const {
        return obstacleY;
    }
    float GetObstacleAngle() const {
        return obstacleAngle;
    }

    // Lift (up) and drag (downstream) coefficients from the last step: force / (0.5 density speed^2 length).
    float GetLift() const {
        return lift;
    }
    float GetDrag() const {
        return drag;
    }

    // Per cell.
    bool IsSolid(uint32_t i, uint32_t j) const {
        return s[i * numY + j] == 0.0f;
    }
    bool IsWall(uint32_t i, uint32_t j) const {
        return j == 0 || j + 1 == numY || i == 0;
    }
    float GetPressure(uint32_t i, uint32_t j) const {
        return p[i * numY + j];
    }
    float GetSmoke(uint32_t i, uint32_t j) const {
        return m[i * numY + j];
    }
    // The speed at the cell's centre.
    float GetSpeed(uint32_t i, uint32_t j) const;

    // How many cells the obstacle fills.
    uint32_t CountObstacleCells() const;

    // The largest |divergence| over the fluid cells (for tests: near zero right after Project).
    float MaxDivergence() const;

    // A hash of the velocity and smoke fields (for tests and determinism checks).
    uint32_t Checksum() const;

   private:
    enum class Field : uint8_t { U, V, Cell };

    void ApplyVorticity();
    void SolveIncompressibility(float dt);
    void Extrapolate();
    void AdvectVelocity(float dt);
    void AdvectSmoke(float dt);
    void FeedInflow();
    void MarkObstacle();
    void FillCell(uint32_t i, uint32_t j);
    void MeasureForce();
    float Sample(float x, float y, const std::vector<float>& field, Field kind) const;
    float AverageU(uint32_t i, uint32_t j) const;
    float AverageV(uint32_t i, uint32_t j) const;

    uint32_t numX = 0;
    uint32_t numY = 0;
    float h = 0.0f;

    std::vector<float> u, v, newU, newV;
    std::vector<float> p;     // pressure, for drawing and forces
    std::vector<float> s;     // 1 fluid, 0 solid
    std::vector<float> m;     // smoke, 0 clear to 1 dense
    std::vector<float> newM;
    std::vector<float> curl;   // vorticity confinement scratch
    std::vector<float> relax;  // pressure solve: overrelaxation over open faces, per cell

    Outline outline;
    float referenceLength = 0.25f;
    std::vector<Point> placed;     // scratch: one polygon in domain units
    std::vector<float> crossings;  // scratch: a scanline's edge crossings

    float inflowSpeed = 2.0f;
    float vorticity = 0.0f;
    SmokeSource smokeSource = SmokeSource::Rake;
    float obstacleX = 0.4f;
    float obstacleY = 0.5f;
    float obstacleAngle = 0.0f;
    float obstacleVX = 0.0f;
    float obstacleVY = 0.0f;
    float lift = 0.0f;
    float drag = 0.0f;
};

}  // namespace Samples
