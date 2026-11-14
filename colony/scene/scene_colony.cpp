#include "colony/scene/scene_colony.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "colony/game/colony_ants.h"
#include "colony/game/colony_world.h"
#include "moksha/client/utils/json_utils.h"
#include "moksha/shared/misc/engine_limits.h"

static constexpr const char* CELL_TEXTURE = "textures/colony_cell.dds";
static constexpr const char* CELL_ATLAS = "texture_data/atlas_cell.json";

static constexpr float CELL_Z = 50.0f;
static constexpr float ANT_Z = 40.0f;  // in front of the cells
static constexpr float ANT_LENGTH = 4.0f;
static constexpr float ANT_WIDTH = 2.0f;
static constexpr float SOLDIER_LENGTH = 6.0f;
static constexpr float SOLDIER_WIDTH = 3.0f;

// Colours.
static constexpr ME::Color GROUND{0.11f, 0.09f, 0.07f, 1.0f};
static constexpr ME::Color WALL{0.42f, 0.42f, 0.46f, 1.0f};
static constexpr ME::Color FOOD{0.30f, 0.86f, 0.32f, 1.0f};
static constexpr ME::Color NEST{0.92f, 0.62f, 0.18f, 1.0f};
static constexpr ME::Color TO_HOME{0.25f, 0.45f, 1.0f, 1.0f};  // added to the ground at full strength
static constexpr ME::Color TO_FOOD{1.0f, 0.30f, 0.25f, 1.0f};
static constexpr ME::Color ANT_SEARCHING{0.88f, 0.86f, 0.80f, 1.0f};
static constexpr ME::Color ANT_CARRYING{0.55f, 1.0f, 0.45f, 1.0f};
static constexpr ME::Color ANT_EXPLORING{0.50f, 0.72f, 1.0f, 1.0f};  // an explorer, searching
static constexpr ME::Color ANT_STUCK{1.0f, 0.42f, 0.30f, 1.0f};      // wandering and wiping markers
static constexpr ME::Color ANT_HIT{1.0f, 1.0f, 1.0f, 1.0f};          // just attacked
static constexpr ME::Color TO_ENEMY{1.0f, 0.15f, 0.55f, 1.0f};       // any colony's toEnemy markers
// Per colony: its nest, rally cross, soldiers and (colonies 1-3) workers and trails. Colony 0's workers and trails keep
// the v1 colours above.
static constexpr ME::Color COLONY_COLORS[Samples::ColonyWorld::MAX_COLONIES] = {
    {0.92f, 0.62f, 0.18f, 1.0f},  // amber
    {0.70f, 0.42f, 1.00f, 1.0f},  // violet
    {0.20f, 0.85f, 0.85f, 1.0f},  // cyan
    {1.00f, 0.45f, 0.70f, 1.0f},  // pink
};
static constexpr float MARKER_TINT = 0.7f;
static constexpr float RIVAL_MARKER_TINT = 0.35f;  // colonies 1-3: toHome and toFood together, in the colony's colour
static constexpr float ENEMY_TINT = 0.6f;
static constexpr float HIT_FLASH = 0.15f;  // seconds an ant shows ANT_HIT after it attacks
static constexpr int32_t RALLY_ARM = 3;    // cells from the rally cross's centre to its ends
static constexpr float FOOD_FULL = 20.0f;  // a cell with this much food or more is drawn at full colour
static constexpr float FOOD_MIN_TINT = 0.35f;

static_assert(Samples::ColonyWorld::CELL_COUNT <= ME::Constants::MaxInstancedSpriteTransformCount);
static_assert(Samples::ColonyAnts::MAX_ANTS <= ME::Constants::MaxInstancedSpriteTransformCount);

namespace {

ME::Color Lerp(const ME::Color& a, const ME::Color& b, float t) {
    return ME::Color(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 1.0f);
}

float Saturate(float value) {
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

}  // namespace

void Samples::SceneColony::CreateResources() {
    ME::Scene::CreateResources();

    // Set 0 draws slot 1 and set 1 slot 2; slot 0 is filled too, as the slots below a set's must be.
    for (size_t i = 0; i < 3; ++i) {
        spriteTexturePaths[i] = CELL_TEXTURE;
        ME::JsonUtils::LoadTextureAtlasProps(CELL_ATLAS, textureAtlasProperties[i]);
    }
    spriteTexturePaths.count = 3;
    textureAtlasProperties.count = 3;
}

void Samples::SceneColony::BuildCamera() {
    ME::Scene::BuildCamera();
    spriteCamera->orthographicSize = VIEW_HALF_HEIGHT;
}

void Samples::SceneColony::BuildInstancedSpriteTransforms() {
    ME::Scene::BuildInstancedSpriteTransforms();

    // Index row * GRID_WIDTH + column, the world's own order. The world's origin is its corner; the view's its centre.
    const float size = ColonyWorld::CELL_SIZE;
    const float left = -0.5f * ColonyWorld::WIDTH;
    const float bottom = -0.5f * ColonyWorld::HEIGHT;
    instancedSpriteTransforms0.count = ColonyWorld::CELL_COUNT;
    for (uint32_t row = 0; row < ColonyWorld::GRID_HEIGHT; ++row) {
        for (uint32_t column = 0; column < ColonyWorld::GRID_WIDTH; ++column) {
            ME::Transform& transform = instancedSpriteTransforms0[row * ColonyWorld::GRID_WIDTH + column];
            transform.SetPosition(left + (static_cast<float>(column) + 0.5f) * size,
                                  bottom + (static_cast<float>(row) + 0.5f) * size, CELL_Z);
            transform.SetScale(size, size);
        }
    }

    instancedSpriteTransforms1.count = ColonyAnts::MAX_ANTS;
    for (uint32_t i = 0; i < ColonyAnts::MAX_ANTS; ++i) {
        instancedSpriteTransforms1[i].SetPosition(0.0f, 0.0f, ANT_Z);
        instancedSpriteTransforms1[i].SetScale(0.0f, 0.0f);
    }
}

void Samples::SceneColony::BuildInstancedSpriteRenderers() {
    ME::Scene::BuildInstancedSpriteRenderers();

    instancedSpriteRenderers0.count = ColonyWorld::CELL_COUNT;
    for (uint32_t i = 0; i < ColonyWorld::CELL_COUNT; ++i) {
        instancedSpriteRenderers0[i] = ME::SpriteRenderer(0, 0, 1, 1, 0);
        instancedSpriteRenderers0[i].color = GROUND;
    }
    instancedSpriteDrawCount0 = ColonyWorld::CELL_COUNT;

    instancedSpriteRenderers1.count = ColonyAnts::MAX_ANTS;
    for (uint32_t i = 0; i < ColonyAnts::MAX_ANTS; ++i) {
        instancedSpriteRenderers1[i] = ME::SpriteRenderer(0, 0, 2, 2, 0);
        instancedSpriteRenderers1[i].color = ANT_SEARCHING;
    }
    instancedSpriteDrawCount1 = 0;
}

const char* Samples::SceneColony::GetDisplayName() const {
    return "Colony";
}

void Samples::SceneColony::SyncCells(const ColonyWorld& world) {
    instancedSpriteDrawCount0 = ColonyWorld::CELL_COUNT;
    const uint8_t* walls = world.GetWalls();
    const uint16_t* food = world.GetFood();
    const uint32_t colonyCount = world.GetColonyCount();
    const float* toHome = world.GetToHome(0);
    const float* toFood = world.GetToFood(0);

    for (uint32_t cell = 0; cell < ColonyWorld::CELL_COUNT; ++cell) {
        ME::Color color;
        if (walls[cell] != 0) {
            color = WALL;
        } else if (food[cell] != 0) {
            const float amount = Saturate(static_cast<float>(food[cell]) / FOOD_FULL);
            color = Lerp(GROUND, FOOD, FOOD_MIN_TINT + (1.0f - FOOD_MIN_TINT) * amount);
        } else {
            const float home = toHome[cell] * MARKER_TINT;
            const float found = toFood[cell] * MARKER_TINT;
            float r = GROUND.r + TO_HOME.r * home + TO_FOOD.r * found;
            float g = GROUND.g + TO_HOME.g * home + TO_FOOD.g * found;
            float b = GROUND.b + TO_HOME.b * home + TO_FOOD.b * found;
            for (uint32_t colony = 0; colony < colonyCount; ++colony) {
                const float enemy = world.GetToEnemy(colony)[cell] * ENEMY_TINT;
                r += TO_ENEMY.r * enemy;
                g += TO_ENEMY.g * enemy;
                b += TO_ENEMY.b * enemy;
                if (colony == 0) {
                    continue;
                }
                const float trail = (world.GetToHome(colony)[cell] + world.GetToFood(colony)[cell]) * RIVAL_MARKER_TINT;
                r += COLONY_COLORS[colony].r * trail;
                g += COLONY_COLORS[colony].g * trail;
                b += COLONY_COLORS[colony].b * trail;
            }
            color = ME::Color(Saturate(r), Saturate(g), Saturate(b), 1.0f);
        }
        ME::SpriteRenderer& renderer = instancedSpriteRenderers0[cell];
        renderer.color = color;
        renderer.bDirty = true;
    }

    // Nests over the ground: every cell whose centre lies inside one.
    for (uint32_t colony = 0; colony < world.GetColonyCount(); ++colony) {
        const ColonyNest& nest = world.GetNest(colony);
        const uint32_t first = ColonyWorld::CellAt(nest.x - nest.radius, nest.y - nest.radius);
        const uint32_t last = ColonyWorld::CellAt(nest.x + nest.radius, nest.y + nest.radius);
        const uint32_t firstColumn = first % ColonyWorld::GRID_WIDTH;
        const uint32_t lastColumn = last % ColonyWorld::GRID_WIDTH;
        for (uint32_t row = first / ColonyWorld::GRID_WIDTH; row <= last / ColonyWorld::GRID_WIDTH; ++row) {
            for (uint32_t column = firstColumn; column <= lastColumn; ++column) {
                const float dx = (static_cast<float>(column) + 0.5f) * ColonyWorld::CELL_SIZE - nest.x;
                const float dy = (static_cast<float>(row) + 0.5f) * ColonyWorld::CELL_SIZE - nest.y;
                if (dx * dx + dy * dy <= nest.radius * nest.radius) {
                    instancedSpriteRenderers0[row * ColonyWorld::GRID_WIDTH + column].color =
                        colonyCount > 1 ? COLONY_COLORS[colony] : NEST;
                }
            }
        }
    }

    // Rally points: a cross of cells in the colony's colour, drawn over the open ground.
    for (uint32_t colony = 0; colony < colonyCount; ++colony) {
        const ColonyNest& nest = world.GetNest(colony);
        if (!nest.bRally) {
            continue;
        }
        const int32_t centreColumn = ColonyWorld::ColumnOf(nest.rallyX);
        const int32_t centreRow = ColonyWorld::RowOf(nest.rallyY);
        for (int32_t step = -RALLY_ARM; step <= RALLY_ARM; ++step) {
            const int32_t cells[2][2] = {{centreColumn + step, centreRow}, {centreColumn, centreRow + step}};
            for (const auto& at : cells) {
                if (at[0] < 0 || at[0] >= static_cast<int32_t>(ColonyWorld::GRID_WIDTH) || at[1] < 0 ||
                    at[1] >= static_cast<int32_t>(ColonyWorld::GRID_HEIGHT)) {
                    continue;
                }
                const uint32_t cell =
                    static_cast<uint32_t>(at[1]) * ColonyWorld::GRID_WIDTH + static_cast<uint32_t>(at[0]);
                if (walls[cell] == 0) {
                    instancedSpriteRenderers0[cell].color = COLONY_COLORS[colony];
                }
            }
        }
    }
}

void Samples::SceneColony::SyncAnts(const ColonyAnts& ants) {
    const float left = -0.5f * ColonyWorld::WIDTH;
    const float bottom = -0.5f * ColonyWorld::HEIGHT;
    const uint32_t slotEnd = ants.GetSlotEnd();
    for (uint32_t slot = 0; slot < slotEnd; ++slot) {
        ME::Transform& transform = instancedSpriteTransforms1[slot];
        if (ants.alive[slot] == 0) {
            transform.SetScale(0.0f, 0.0f);
            continue;
        }
        const bool bSoldier = ants.caste[slot] == AntCaste::Soldier;
        transform.SetPosition(left + ants.x[slot], bottom + ants.y[slot], ANT_Z);
        transform.SetRotation(0.0f, 0.0f, ants.heading[slot]);
        transform.SetScale(bSoldier ? SOLDIER_LENGTH : ANT_LENGTH, bSoldier ? SOLDIER_WIDTH : ANT_WIDTH);

        ME::SpriteRenderer& renderer = instancedSpriteRenderers1[slot];
        const uint32_t colony = ants.colony[slot];
        const bool bExplorer = ants.role[slot] == AntRole::Explorer;
        if (ants.stuckTime[slot] > 0.0f) {
            renderer.color = ANT_STUCK;
        } else if (ants.fightTime[slot] < HIT_FLASH) {
            renderer.color = ANT_HIT;
        } else if (bSoldier) {
            renderer.color = COLONY_COLORS[colony];
        } else if (ants.task[slot] == AntTask::ToHome) {
            renderer.color = ANT_CARRYING;
        } else if (colony == 0) {
            renderer.color = bExplorer ? ANT_EXPLORING : ANT_SEARCHING;
        } else {
            renderer.color = Lerp(COLONY_COLORS[colony], ME::Color::White(), bExplorer ? 0.6f : 0.3f);
        }
        renderer.bDirty = true;
    }
    instancedSpriteDrawCount1 = slotEnd;
}

void Samples::SceneColony::Zoom(float notches, float pointerX, float pointerY) {
    const float aspect = spriteCamera->aspectRatio;
    const float oldHalf = spriteCamera->orthographicSize;
    float half = oldHalf * std::pow(ZOOM_PER_NOTCH, -notches);
    const float minHalf = VIEW_HALF_HEIGHT / MAX_ZOOM;
    half = half < minHalf ? minHalf : (half > VIEW_HALF_HEIGHT ? VIEW_HALF_HEIGHT : half);

    // The pointer's offset from the view's centre, in half heights; the world point there stays under it.
    const float offsetX = (pointerX - 0.5f) * 2.0f * aspect;
    const float offsetY = (pointerY - 0.5f) * 2.0f;
    PlaceView(spriteCamera->position.x + offsetX * (oldHalf - half),
              spriteCamera->position.y + offsetY * (oldHalf - half), half);
}

void Samples::SceneColony::Pan(float viewsX, float viewsY) {
    const float half = spriteCamera->orthographicSize;
    PlaceView(spriteCamera->position.x + viewsX * 2.0f * half, spriteCamera->position.y + viewsY * 2.0f * half, half);
}

void Samples::SceneColony::PlaceView(float x, float y, float half) {
    // Keep the view inside the world (the camera is at the world's centre when zoomed out).
    const float aspect = spriteCamera->aspectRatio;
    const float limitX = std::fmax(0.0f, 0.5f * ColonyWorld::WIDTH - half * aspect);
    const float limitY = std::fmax(0.0f, 0.5f * ColonyWorld::HEIGHT - half);
    x = x < -limitX ? -limitX : (x > limitX ? limitX : x);
    y = y < -limitY ? -limitY : (y > limitY ? limitY : y);

    spriteCamera->orthographicSize = half;
    spriteCamera->position.x = x;
    spriteCamera->position.y = y;
    spriteCamera->viewPosition.x = x;
    spriteCamera->viewPosition.y = y;
}

void Samples::SceneColony::HideAll() {
    instancedSpriteDrawCount0 = 0;
    instancedSpriteDrawCount1 = 0;
}
