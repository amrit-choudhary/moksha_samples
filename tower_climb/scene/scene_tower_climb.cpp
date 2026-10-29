#include "tower_climb/scene/scene_tower_climb.h"

#include "moksha/client/utils/json_utils.h"
#include "moksha/shared/physics/physics_layer.h"

static constexpr const char* ATLAS_TEXTURE = "textures/world/tiny_dungeon.dds";
static constexpr const char* ATLAS_PROPS = "texture_data/atlas_tiny_dungeon.json";

// Hitboxes are smaller than the 16 px tiles, whose art doesn't fill the tile.
static constexpr float CLIMBER_HITBOX = 0.6f;
static constexpr float HAZARD_HITBOX = 0.6f;

// Tints darken the bricks behind the walls so the shaft reads at a glance.
static constexpr ME::Color WALL_TINT{1.0f, 1.0f, 1.0f, 1.0f};
static constexpr ME::Color SHAFT_TINT{0.35f, 0.35f, 0.45f, 1.0f};
static constexpr ME::Color OUTSIDE_TINT{0.18f, 0.18f, 0.24f, 1.0f};

void Samples::SceneTowerClimb::Init() {
    ME::Scene::Init();
    BuildColliders();
}

void Samples::SceneTowerClimb::CreateResources() {
    ME::Scene::CreateResources();

    // Slot 0: plain sprites. Slot 1: instanced set 0 (shaft). Slot 2: instanced set 1 (hazards).
    for (size_t i = 0; i < 3; ++i) {
        spriteTexturePaths[i] = ATLAS_TEXTURE;
        ME::JsonUtils::LoadTextureAtlasProps(ATLAS_PROPS, textureAtlasProperties[i]);
    }
    spriteTexturePaths.count = 3;
    textureAtlasProperties.count = 3;

    sfxPaths[TowerSfx::JUMP] = "audio/jump.wav";
    sfxPaths[TowerSfx::HIT] = "audio/hit.wav";
    sfxPaths.count = 2;
}

void Samples::SceneTowerClimb::BuildCamera() {
    ME::Scene::BuildCamera();
    spriteCamera->orthographicSize = VIEW_HALF_HEIGHT;
}

void Samples::SceneTowerClimb::BuildSpriteTransforms() {
    ME::Scene::BuildSpriteTransforms();

    spriteTransforms[0].SetPosition(-CLING_X, CLIMBER_Y, 0.0f);
    spriteTransforms[0].SetScale(TILE_SIZE, TILE_SIZE);
    spriteTransforms.count = 1;
}

void Samples::SceneTowerClimb::BuildSpriteRenderers() {
    ME::Scene::BuildSpriteRenderers();

    spriteRenderers[0] = ME::SpriteRenderer(0, 0, 0, 0, TowerTiles::CLIMBER);
    spriteRenderers.count = 1;
}

void Samples::SceneTowerClimb::BuildInstancedSpriteTransforms() {
    ME::Scene::BuildInstancedSpriteTransforms();

    // z = 1 keeps the shaft behind the climber and hazards.
    instancedSpriteTransforms0.count = SHAFT_TILE_COUNT;
    for (size_t i = 0; i < SHAFT_TILE_COUNT; ++i) {
        const size_t row = i / SHAFT_COLUMNS;
        const float y = (static_cast<float>(row) - static_cast<float>(SHAFT_ROWS - 1) * 0.5f) * TILE_SIZE;
        instancedSpriteTransforms0[i].SetPosition(ColumnCenterX(i % SHAFT_COLUMNS), y, 1.0f);
        instancedSpriteTransforms0[i].SetScale(TILE_SIZE, TILE_SIZE);
    }

    instancedSpriteTransforms1.count = MAX_HAZARDS;
    for (size_t i = 0; i < MAX_HAZARDS; ++i) {
        instancedSpriteTransforms1[i].SetPosition(parkPos);
        instancedSpriteTransforms1[i].SetScale(TILE_SIZE, TILE_SIZE);
    }
}

void Samples::SceneTowerClimb::BuildInstancedSpriteRenderers() {
    ME::Scene::BuildInstancedSpriteRenderers();

    instancedSpriteRenderers0.count = SHAFT_TILE_COUNT;
    for (size_t row = 0; row < SHAFT_ROWS; ++row) {
        RollRow(row);
    }

    instancedSpriteRenderers1.count = MAX_HAZARDS;
    for (size_t i = 0; i < MAX_HAZARDS; ++i) {
        instancedSpriteRenderers1[i] = ME::SpriteRenderer(0, 0, 2, 2, HAZARD_TYPES[0].tile);
    }
}

void Samples::SceneTowerClimb::BuildColliders() {
    using ME::PhysicsLayer;

    dynamicColliders[CLIMBER_COLLIDER] = ME::ColliderAABB(CLIMBER_COLLIDER, true, false, PhysicsLayer::Player,
                                                          PhysicsLayer::Enemy, spriteTransforms[0], CLIMBER_HITBOX);
    for (size_t i = 0; i < MAX_HAZARDS; ++i) {
        const size_t id = FIRST_HAZARD_COLLIDER + i;
        dynamicColliders[id] = ME::ColliderAABB(id, false, false, PhysicsLayer::Enemy, PhysicsLayer::Player,
                                                instancedSpriteTransforms1[i], HAZARD_HITBOX);
    }
    dynamicColliders.count = FIRST_HAZARD_COLLIDER + MAX_HAZARDS;
}

void Samples::SceneTowerClimb::ScrollShaft(float distance) {
    const float shaftHeight = static_cast<float>(SHAFT_ROWS) * TILE_SIZE;
    const float bottom = -VIEW_HALF_HEIGHT - TILE_SIZE;

    for (size_t row = 0; row < SHAFT_ROWS; ++row) {
        const size_t first = row * SHAFT_COLUMNS;
        float y = instancedSpriteTransforms0[first].GetPosition().y - distance;
        const bool bWrapped = y < bottom;
        if (bWrapped) {
            y += shaftHeight;
        }

        for (size_t column = 0; column < SHAFT_COLUMNS; ++column) {
            instancedSpriteTransforms0[first + column].SetPosition(ColumnCenterX(column), y, 1.0f);
            instancedSpriteRenderers0[first + column].bDirty = true;
        }
        if (bWrapped) {
            RollRow(row);
        }
    }
}

void Samples::SceneTowerClimb::SyncClimber() {
    dynamicColliders[CLIMBER_COLLIDER].UpdateTransform(spriteTransforms[0], CLIMBER_HITBOX);
}

void Samples::SceneTowerClimb::SyncHazards(const Hazard* hazards, size_t count) {
    for (size_t i = 0; i < MAX_HAZARDS; ++i) {
        ME::Transform& transform = instancedSpriteTransforms1[i];
        ME::ColliderAABB& collider = dynamicColliders[FIRST_HAZARD_COLLIDER + i];
        const bool bActive = i < count && hazards[i].bActive;

        if (bActive) {
            transform.SetPosition(hazards[i].x, hazards[i].y, 0.0f);
            instancedSpriteRenderers1[i].atlasIndex = HAZARD_TYPES[hazards[i].type].tile;
            // Crawlers face away from their wall.
            instancedSpriteRenderers1[i].ToggleFlipHorizontal(hazards[i].x > 0.0f);
        } else {
            transform.SetPosition(parkPos);
        }
        instancedSpriteRenderers1[i].bDirty = true;
        collider.UpdateTransform(transform, HAZARD_HITBOX);
        collider.isEnabled = bActive;
    }
}

const char* Samples::SceneTowerClimb::GetDisplayName() const {
    return "Tower Climb Scene";
}

void Samples::SceneTowerClimb::RollRow(size_t row) {
    const size_t first = row * SHAFT_COLUMNS;
    for (size_t column = 0; column < SHAFT_COLUMNS; ++column) {
        const bool bWall = column == LEFT_WALL_COLUMN || column == RIGHT_WALL_COLUMN;
        const bool bInside = column > LEFT_WALL_COLUMN && column < RIGHT_WALL_COLUMN;
        const uint32_t roll = shaftRandom.NextRange(0, 99);

        uint16_t tile = TowerTiles::BRICK;
        ME::Color tint = bWall ? WALL_TINT : (bInside ? SHAFT_TINT : OUTSIDE_TINT);
        if (bWall) {
            tile = roll < 15 ? TowerTiles::BRICK_FRAMED_A : (roll < 30 ? TowerTiles::BRICK_FRAMED_B : TowerTiles::BRICK);
        } else if (bInside && roll < 2) {
            tile = TowerTiles::BANNER;
            tint = WALL_TINT;
        }

        instancedSpriteRenderers0[first + column] = ME::SpriteRenderer(0, 0, 1, 1, tile, tint);
    }
}

float Samples::SceneTowerClimb::ColumnCenterX(size_t column) const {
    return (static_cast<float>(column) - static_cast<float>(SHAFT_COLUMNS - 1) * 0.5f) * TILE_SIZE;
}
