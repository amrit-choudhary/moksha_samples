#include "wind_tunnel/scene/scene_wind_tunnel.h"

#include <algorithm>
#include <cmath>

#include "moksha/client/utils/json_utils.h"
#include "wind_tunnel/game/wind_tunnel_sim.h"

static constexpr const char* CELL_TEXTURE = "textures/wind_cell.dds";
static constexpr const char* CELL_ATLAS = "texture_data/atlas_cell.json";

static constexpr float CELL_Z = 50.0f;
static constexpr float STREAM_Z = 40.0f;  // additive, so depth only has to put it in front of the cells

// Streamlines: seeds every STREAM_SPACING domain units, each traced STREAM_STEPS times for STREAM_STEP_TIME seconds.
static constexpr float STREAM_SPACING = 1.0f / 26.0f;
static constexpr uint32_t STREAM_STEPS = 12;
static constexpr float STREAM_STEP_TIME = 0.011f;
static constexpr float STREAM_THICKNESS = 0.0028f;
static constexpr float STREAM_ALPHA = 0.42f;

static constexpr float RANGE_EASE = 0.06f;  // per frame, toward the frame's own range

// Colours.
static constexpr ME::Color BACKGROUND{0.015f, 0.02f, 0.05f, 1.0f};
static constexpr ME::Color SMOKE{0.82f, 0.94f, 1.0f, 1.0f};
// Green, which none of the views use, so the obstacle never blends into smoke or a tint; its rim is darker.
static constexpr ME::Color OBSTACLE{0.22f, 0.86f, 0.42f, 1.0f};
static constexpr ME::Color OBSTACLE_RIM{0.04f, 0.32f, 0.14f, 1.0f};
static constexpr ME::Color WALL{0.16f, 0.17f, 0.21f, 1.0f};
static constexpr ME::Color SUCTION{0.18f, 0.62f, 1.0f, 1.0f};   // smoke where the pressure is low
static constexpr ME::Color PUSH{1.0f, 0.46f, 0.12f, 1.0f};       // smoke where it is high
static constexpr ME::Color PRESSURE_MIDDLE{0.07f, 0.08f, 0.12f, 1.0f};  // free-stream pressure, pressure view
static constexpr float PRESSURE_UNDER_SMOKE = 0.10f;  // how much the pressure tints clear air
static constexpr float PRESSURE_SATURATION = 0.7f;    // the full tint at this fraction of the largest deviation

namespace {

ME::Color Lerp(const ME::Color& a, const ME::Color& b, float t) {
    return ME::Color(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 1.0f);
}

// Black, violet, orange, pale yellow: still air is dark, fast air bright.
ME::Color HeatColor(float t) {
    static constexpr ME::Color STOPS[] = {
        {0.01f, 0.01f, 0.04f, 1.0f},
        {0.30f, 0.06f, 0.45f, 1.0f},
        {0.85f, 0.25f, 0.30f, 1.0f},
        {0.99f, 0.62f, 0.10f, 1.0f},
        {0.99f, 0.97f, 0.75f, 1.0f},
    };
    constexpr int last = static_cast<int>(sizeof(STOPS) / sizeof(STOPS[0])) - 1;
    t = std::clamp(t, 0.0f, 0.9999f) * static_cast<float>(last);
    const int index = static_cast<int>(t);
    return Lerp(STOPS[index], STOPS[index + 1], t - static_cast<float>(index));
}

// Pressure deviation in [-1, 1] to a colour: blue below the free stream, `middle` at it, orange above.
ME::Color DivergingColor(float t, const ME::Color& middle) {
    t = std::clamp(t, -1.0f, 1.0f);
    return t < 0.0f ? Lerp(middle, SUCTION, -t) : Lerp(middle, PUSH, t);
}

float Ease(float current, float target) {
    return current + (target - current) * RANGE_EASE;
}

}  // namespace

void Samples::SceneWindTunnel::CreateResources() {
    ME::Scene::CreateResources();

    // Set 0 draws slot 1 and set 1 slot 2; slot 0 is filled too, as the slots below a set's must be.
    for (size_t i = 0; i < 3; ++i) {
        spriteTexturePaths[i] = CELL_TEXTURE;
        ME::JsonUtils::LoadTextureAtlasProps(CELL_ATLAS, textureAtlasProperties[i]);
    }
    spriteTexturePaths.count = 3;
    textureAtlasProperties.count = 3;
}

void Samples::SceneWindTunnel::BuildCamera() {
    ME::Scene::BuildCamera();
    spriteCamera->orthographicSize = VIEW_HALF_HEIGHT;
}

void Samples::SceneWindTunnel::BuildInstancedSpriteTransforms() {
    ME::Scene::BuildInstancedSpriteTransforms();

    instancedSpriteTransforms0.count = MAX_CELLS;
    for (uint32_t i = 0; i < MAX_CELLS; ++i) {
        instancedSpriteTransforms0[i].SetPosition(0.0f, 0.0f, CELL_Z);
        instancedSpriteTransforms0[i].SetScale(0.0f, 0.0f);
    }
    instancedSpriteTransforms1.count = MAX_SEGMENTS;
    for (uint32_t i = 0; i < MAX_SEGMENTS; ++i) {
        instancedSpriteTransforms1[i].SetPosition(0.0f, 0.0f, STREAM_Z);
        instancedSpriteTransforms1[i].SetScale(0.0f, 0.0f);
    }
}

void Samples::SceneWindTunnel::BuildInstancedSpriteRenderers() {
    ME::Scene::BuildInstancedSpriteRenderers();

    instancedSpriteRenderers0.count = MAX_CELLS;
    for (uint32_t i = 0; i < MAX_CELLS; ++i) {
        instancedSpriteRenderers0[i] = ME::SpriteRenderer(0, 0, 1, 1, 0);
    }
    instancedSpriteDrawCount0 = 0;

    instancedSpriteRenderers1.count = MAX_SEGMENTS;
    for (uint32_t i = 0; i < MAX_SEGMENTS; ++i) {
        instancedSpriteRenderers1[i] = ME::SpriteRenderer(0, 0, 2, 2, 0);
    }
    instancedSpriteBlend1 = ME::SpriteBlend::Additive;
    instancedSpriteDrawCount1 = 0;
}

const char* Samples::SceneWindTunnel::GetDisplayName() const {
    return "Wind Tunnel";
}

void Samples::SceneWindTunnel::Layout(const WindTunnelSim& sim) {
    const uint32_t nx = sim.GetCountX();
    const uint32_t ny = sim.GetCountY();
    const float h = sim.GetCellSize();
    const float left = -0.5f * sim.GetWidth();
    const float bottom = -VIEW_HALF_HEIGHT;
    const uint32_t cells = std::min(nx * ny, MAX_CELLS);

    // Index i * ny + j, the simulation's own order, so Sync walks both arrays together.
    for (uint32_t c = 0; c < cells; ++c) {
        const uint32_t i = c / ny;
        const uint32_t j = c % ny;
        ME::Transform& transform = instancedSpriteTransforms0[c];
        transform.SetPosition(left + (static_cast<float>(i) + 0.5f) * h, bottom + (static_cast<float>(j) + 0.5f) * h,
                              CELL_Z);
        transform.SetScale(h, h);
        instancedSpriteRenderers0[c].bDirty = true;
    }
    instancedSpriteDrawCount0 = cells;
    bRangeValid = false;
}

void Samples::SceneWindTunnel::Sync(const WindTunnelSim& sim, View view, bool bStreamlines) {
    const uint32_t nx = sim.GetCountX();
    const uint32_t ny = sim.GetCountY();
    const uint32_t cells = std::min(nx * ny, MAX_CELLS);

    // This frame's ranges, over the fluid away from the inlet (its pressure spikes when the inflow changes). The free
    // stream's pressure is taken as the mean.
    float pLow = 0.0f;
    float pHigh = 0.0f;
    float sHigh = 0.0f;
    double pSum = 0.0;
    uint32_t fluidCells = 0;
    for (uint32_t i = 2; i < nx; ++i) {
        for (uint32_t j = 1; j + 1 < ny; ++j) {
            if (sim.IsSolid(i, j)) {
                continue;
            }
            const float p = sim.GetPressure(i, j);
            pLow = std::min(pLow, p);
            pHigh = std::max(pHigh, p);
            pSum += p;
            ++fluidCells;
            sHigh = std::max(sHigh, sim.GetSpeed(i, j));
        }
    }
    const float pMean = fluidCells > 0 ? static_cast<float>(pSum / fluidCells) : 0.0f;
    const float pDeviation = std::max(pHigh - pMean, pMean - pLow);
    if (!bRangeValid) {
        pressureMid = pMean;
        pressureDeviation = pDeviation;
        speedHigh = sHigh;
        bRangeValid = true;
    } else {
        pressureMid = Ease(pressureMid, pMean);
        pressureDeviation = Ease(pressureDeviation, pDeviation);
        speedHigh = Ease(speedHigh, sHigh);
    }
    const float toDeviation = pressureDeviation > 1e-6f ? 1.0f / (PRESSURE_SATURATION * pressureDeviation) : 0.0f;
    const float toSpeed = speedHigh > 1e-6f ? 1.0f / speedHigh : 0.0f;

    for (uint32_t c = 0; c < cells; ++c) {
        const uint32_t i = c / ny;
        const uint32_t j = c % ny;
        ME::Color color;
        if (sim.IsSolid(i, j)) {
            if (sim.IsWall(i, j)) {
                color = WALL;
            } else {
                // A rim cell touches fluid on a side.
                const bool bRim = !sim.IsSolid(i - 1, j) || !sim.IsSolid(i + 1, j) || !sim.IsSolid(i, j - 1) ||
                                  !sim.IsSolid(i, j + 1);
                color = bRim ? OBSTACLE_RIM : OBSTACLE;
            }
        } else {
            const float smoke = std::clamp(sim.GetSmoke(i, j), 0.0f, 1.0f);
            switch (view) {
                case View::PressureSmoke: {
                    // Smoke tinted by pressure; clear air dark, with a faint tint.
                    const float deviation = (sim.GetPressure(i, j) - pressureMid) * toDeviation;
                    const ME::Color tint = DivergingColor(deviation, SMOKE);
                    const float k = PRESSURE_UNDER_SMOKE * std::min(std::fabs(deviation), 1.0f) +
                                    (1.0f - PRESSURE_UNDER_SMOKE) * smoke;
                    color = Lerp(BACKGROUND, tint, k);
                    break;
                }
                case View::Smoke:
                    color = Lerp(BACKGROUND, SMOKE, smoke);
                    break;
                case View::Pressure:
                    color = DivergingColor((sim.GetPressure(i, j) - pressureMid) * toDeviation, PRESSURE_MIDDLE);
                    break;
                case View::Speed:
                    color = HeatColor(sim.GetSpeed(i, j) * toSpeed);
                    break;
                case View::Count:
                    break;
            }
        }
        ME::SpriteRenderer& renderer = instancedSpriteRenderers0[c];
        renderer.color = color;
        renderer.bDirty = true;
    }

    if (bStreamlines) {
        TraceStreamlines(sim);
    } else {
        instancedSpriteDrawCount1 = 0;
    }
}

float Samples::SceneWindTunnel::ToSimX(const WindTunnelSim& sim, float viewX) const {
    return viewX + 0.5f * sim.GetWidth();
}

float Samples::SceneWindTunnel::ToSimY(float viewY) const {
    return viewY + VIEW_HALF_HEIGHT;
}

void Samples::SceneWindTunnel::TraceStreamlines(const WindTunnelSim& sim) {
    const float width = sim.GetWidth();
    const float h = sim.GetCellSize();
    const float left = -0.5f * width;
    const float bottom = -VIEW_HALF_HEIGHT;
    const uint32_t nx = sim.GetCountX();
    const uint32_t ny = sim.GetCountY();
    auto solidAt = [&](float x, float y) {
        if (x < 0.0f || y < 0.0f) {
            return true;
        }
        const uint32_t i = static_cast<uint32_t>(x / h);
        const uint32_t j = static_cast<uint32_t>(y / h);
        return i >= nx || j >= ny || sim.IsSolid(i, j);
    };

    uint32_t count = 0;
    for (float sy = 0.5f * STREAM_SPACING; sy < WindTunnelSim::DOMAIN_HEIGHT; sy += STREAM_SPACING) {
        for (float sx = 0.5f * STREAM_SPACING; sx < width; sx += STREAM_SPACING) {
            float x = sx;
            float y = sy;
            for (uint32_t step = 0; step < STREAM_STEPS && count < MAX_SEGMENTS; ++step) {
                if (solidAt(x, y)) {
                    break;
                }
                const float vx = sim.SampleU(x, y);
                const float vy = sim.SampleV(x, y);
                const float dx = vx * STREAM_STEP_TIME;
                const float dy = vy * STREAM_STEP_TIME;
                const float length = std::sqrt(dx * dx + dy * dy);
                if (length < 0.1f * STREAM_THICKNESS) {
                    break;
                }

                ME::Transform& transform = instancedSpriteTransforms1[count];
                transform.SetPosition(left + x + 0.5f * dx, bottom + y + 0.5f * dy, STREAM_Z);
                transform.SetScale(length + STREAM_THICKNESS, STREAM_THICKNESS);
                transform.SetRotation(0.0f, 0.0f, std::atan2(dy, dx));

                // Brightest at the seed, fading downstream.
                const float fade = 1.0f - static_cast<float>(step) / static_cast<float>(STREAM_STEPS);
                ME::SpriteRenderer& renderer = instancedSpriteRenderers1[count];
                renderer.color = ME::Color(1.0f, 1.0f, 1.0f, STREAM_ALPHA * fade);
                renderer.bDirty = true;
                ++count;

                x += dx;
                y += dy;
            }
        }
    }
    instancedSpriteDrawCount1 = count;
}
