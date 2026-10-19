#include "survivors/scene/scene_survivors.h"

#include "moksha/client/utils/json_utils.h"
#include "moksha/shared/physics/physics_layer.h"
#include "moksha/shared/random/random_engine.h"

static constexpr const char* ATLAS_TEXTURE = "textures/world/tiny_dungeon.dds";
static constexpr const char* ATLAS_PROPS = "texture_data/atlas_tiny_dungeon.json";

// Hitboxes are smaller than the 16 px tiles, whose art doesn't fill the tile.
static constexpr float PLAYER_HITBOX = 0.6f;
static constexpr float ENEMY_HITBOX = 0.7f;
static constexpr float BULLET_HITBOX = 0.5f;

void Samples::SceneSurvivors::Init() {
    ME::Scene::Init();
    BuildColliders();
}

void Samples::SceneSurvivors::CreateResources() {
    ME::Scene::CreateResources();

    // Slot 0: plain sprites. Slot 1: instanced set 0 (arena tiles). Slot 2: instanced set 1 (enemies).
    for (size_t i = 0; i < 3; ++i) {
        spriteTexturePaths[i] = ATLAS_TEXTURE;
        ME::JsonUtils::LoadTextureAtlasProps(ATLAS_PROPS, textureAtlasProperties[i]);
    }
    spriteTexturePaths.count = 3;
    textureAtlasProperties.count = 3;

    sfxPaths[SurvivorsSfx::SHOOT] = "audio/shoot.wav";
    sfxPaths[SurvivorsSfx::HIT] = "audio/hit.wav";
    sfxPaths.count = 2;
}

void Samples::SceneSurvivors::BuildCamera() {
    ME::Scene::BuildCamera();
    spriteCamera->orthographicSize = 100.0f;
}

void Samples::SceneSurvivors::BuildSpriteTransforms() {
    ME::Scene::BuildSpriteTransforms();

    spriteTransforms[0].SetPosition(0.0f, 0.0f, 0.0f);
    spriteTransforms[0].SetScale(TILE_SIZE, TILE_SIZE);
    spriteTransforms.count = 1;
}

void Samples::SceneSurvivors::BuildSpriteRenderers() {
    ME::Scene::BuildSpriteRenderers();

    spriteRenderers[0] = ME::SpriteRenderer(0, 0, 0, 0, SurvivorsTiles::PLAYER);
    spriteRenderers.count = 1;
}

void Samples::SceneSurvivors::BuildInstancedSpriteTransforms() {
    ME::Scene::BuildInstancedSpriteTransforms();

    instancedSpriteTransforms0.count = ARENA_TILE_COUNT;
    for (size_t i = 0; i < ARENA_TILE_COUNT; ++i) {
        // z = 1 keeps the floor behind the player and enemies.
        instancedSpriteTransforms0[i].SetPosition(TileCenterX(i % ARENA_TILES_X), TileCenterY(i / ARENA_TILES_X), 1.0f);
        instancedSpriteTransforms0[i].SetScale(TILE_SIZE, TILE_SIZE);
    }

    // Sized for both full pools up front; slots are parked until an enemy or bullet uses them.
    instancedSpriteTransforms1.count = MAX_ENEMIES + MAX_BULLETS;
    for (size_t i = 0; i < MAX_ENEMIES; ++i) {
        instancedSpriteTransforms1[i].SetPosition(parkPos);
        instancedSpriteTransforms1[i].SetScale(TILE_SIZE, TILE_SIZE);
    }
    for (size_t i = 0; i < MAX_BULLETS; ++i) {
        instancedSpriteTransforms1[FIRST_BULLET_INSTANCE + i].SetPosition(parkPos);
        instancedSpriteTransforms1[FIRST_BULLET_INSTANCE + i].SetScale(TILE_SIZE * 0.5f, TILE_SIZE * 0.5f);
    }
}

void Samples::SceneSurvivors::BuildInstancedSpriteRenderers() {
    ME::Scene::BuildInstancedSpriteRenderers();

    // Fixed seed: the floor looks the same every run.
    ME::Random floorRandom{1234u};
    instancedSpriteRenderers0.count = ARENA_TILE_COUNT;
    for (size_t i = 0; i < ARENA_TILE_COUNT; ++i) {
        const size_t x = i % ARENA_TILES_X;
        const size_t y = i / ARENA_TILES_X;
        const bool isWall = x == 0 || y == 0 || x == ARENA_TILES_X - 1 || y == ARENA_TILES_Y - 1;

        uint16_t tile = SurvivorsTiles::WALL;
        if (!isWall) {
            const uint32_t roll = floorRandom.NextRange(0, 99);
            tile = roll < 5    ? SurvivorsTiles::FLOOR_SPECKLED_A
                   : roll < 10 ? SurvivorsTiles::FLOOR_SPECKLED_B
                               : SurvivorsTiles::FLOOR;
        }
        instancedSpriteRenderers0[i] = ME::SpriteRenderer(0, 0, 1, 1, tile);
    }

    instancedSpriteRenderers1.count = MAX_ENEMIES + MAX_BULLETS;
    for (size_t i = 0; i < MAX_ENEMIES; ++i) {
        instancedSpriteRenderers1[i] = ME::SpriteRenderer(0, 0, 2, 2, ENEMY_TYPES[0].tile);
    }
    for (size_t i = 0; i < MAX_BULLETS; ++i) {
        instancedSpriteRenderers1[FIRST_BULLET_INSTANCE + i] = ME::SpriteRenderer(0, 0, 2, 2, SurvivorsTiles::BULLET);
    }
}

void Samples::SceneSurvivors::BuildColliders() {
    using ME::PhysicsLayer;

    dynamicColliders[PLAYER_COLLIDER] = ME::ColliderAABB(PLAYER_COLLIDER, true, false, PhysicsLayer::Player,
                                                         PhysicsLayer::Enemy, spriteTransforms[0], PLAYER_HITBOX);
    for (size_t i = 0; i < MAX_ENEMIES; ++i) {
        const size_t id = FIRST_ENEMY_COLLIDER + i;
        dynamicColliders[id] =
            ME::ColliderAABB(id, false, false, PhysicsLayer::Enemy, PhysicsLayer::Player | PhysicsLayer::Projectile,
                             instancedSpriteTransforms1[i], ENEMY_HITBOX);
    }
    for (size_t i = 0; i < MAX_BULLETS; ++i) {
        const size_t id = FIRST_BULLET_COLLIDER + i;
        dynamicColliders[id] = ME::ColliderAABB(id, false, false, PhysicsLayer::Projectile, PhysicsLayer::Enemy,
                                                instancedSpriteTransforms1[FIRST_BULLET_INSTANCE + i], BULLET_HITBOX);
    }
    dynamicColliders.count = FIRST_BULLET_COLLIDER + MAX_BULLETS;
}

void Samples::SceneSurvivors::SyncPlayer() {
    dynamicColliders[PLAYER_COLLIDER].UpdateTransform(spriteTransforms[0], PLAYER_HITBOX);
}

void Samples::SceneSurvivors::SyncEnemies(ME::Pool<Enemy>& enemies) {
    const size_t activeCount = enemies.GetActiveCount();
    for (size_t i = 0; i < activeCount; ++i) {
        const Enemy& enemy = enemies[i];
        ME::Transform& transform = instancedSpriteTransforms1[i];
        transform.SetPosition(enemy.position.x, enemy.position.y, 0.0f);
        instancedSpriteRenderers1[i].atlasIndex = ENEMY_TYPES[enemy.type].tile;
        instancedSpriteRenderers1[i].bDirty = true;

        ME::ColliderAABB& collider = dynamicColliders[FIRST_ENEMY_COLLIDER + i];
        collider.UpdateTransform(transform, ENEMY_HITBOX);
        collider.isEnabled = !enemy.bDead;
    }

    for (size_t i = activeCount; i < syncedEnemyCount; ++i) {
        instancedSpriteTransforms1[i].SetPosition(parkPos);
        instancedSpriteRenderers1[i].bDirty = true;
        dynamicColliders[FIRST_ENEMY_COLLIDER + i].isEnabled = false;
    }
    syncedEnemyCount = activeCount;
}

void Samples::SceneSurvivors::SyncBullets(ME::Pool<Bullet>& bullets) {
    const size_t activeCount = bullets.GetActiveCount();
    for (size_t i = 0; i < activeCount; ++i) {
        const Bullet& bullet = bullets[i];
        ME::Transform& transform = instancedSpriteTransforms1[FIRST_BULLET_INSTANCE + i];
        transform.SetPosition(bullet.position.x, bullet.position.y, 0.0f);
        instancedSpriteRenderers1[FIRST_BULLET_INSTANCE + i].bDirty = true;

        ME::ColliderAABB& collider = dynamicColliders[FIRST_BULLET_COLLIDER + i];
        collider.UpdateTransform(transform, BULLET_HITBOX);
        collider.isEnabled = !bullet.bDead;
    }

    for (size_t i = activeCount; i < syncedBulletCount; ++i) {
        instancedSpriteTransforms1[FIRST_BULLET_INSTANCE + i].SetPosition(parkPos);
        instancedSpriteRenderers1[FIRST_BULLET_INSTANCE + i].bDirty = true;
        dynamicColliders[FIRST_BULLET_COLLIDER + i].isEnabled = false;
    }
    syncedBulletCount = activeCount;
}

const char* Samples::SceneSurvivors::GetDisplayName() const {
    return "Survivors Scene";
}

float Samples::SceneSurvivors::GetFloorHalfWidth() const {
    return static_cast<float>(ARENA_TILES_X - 2) * TILE_SIZE * 0.5f;
}

float Samples::SceneSurvivors::GetFloorHalfHeight() const {
    return static_cast<float>(ARENA_TILES_Y - 2) * TILE_SIZE * 0.5f;
}

float Samples::SceneSurvivors::TileCenterX(size_t x) const {
    return (static_cast<float>(x) - static_cast<float>(ARENA_TILES_X - 1) * 0.5f) * TILE_SIZE;
}

float Samples::SceneSurvivors::TileCenterY(size_t y) const {
    return (static_cast<float>(y) - static_cast<float>(ARENA_TILES_Y - 1) * 0.5f) * TILE_SIZE;
}
