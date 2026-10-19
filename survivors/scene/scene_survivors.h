/**
 * Survivors scene: a walled one-screen arena (instanced set 0), enemies then bullets (instanced set 1), the player
 * (the only plain sprite: each plain sprite costs a DX constant buffer), and one dynamic collider per
 * player/enemy/bullet slot. All sprites come from one atlas (Kenney's Tiny Dungeon), loaded into slots 0-2.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "moksha/client/scene/scene.h"
#include "moksha/shared/datastructure/pool.h"
#include "moksha/shared/math/transform.h"
#include "survivors/game/survivors_actors.h"

namespace Samples {

// Tile indices in the Tiny Dungeon atlas (row * 12 + column).
namespace SurvivorsTiles {
constexpr uint16_t FLOOR = 48;
constexpr uint16_t FLOOR_SPECKLED_A = 49;
constexpr uint16_t FLOOR_SPECKLED_B = 51;
constexpr uint16_t WALL = 40;
constexpr uint16_t PLAYER = 96;
constexpr uint16_t BULLET = 101;
}  // namespace SurvivorsTiles

// Sound ids: indices into sfxPaths.
namespace SurvivorsSfx {
constexpr uint8_t SHOOT = 0;
constexpr uint8_t HIT = 1;
}  // namespace SurvivorsSfx

class SceneSurvivors : public ME::Scene {
   public:
    virtual void Init() override;
    virtual void CreateResources() override;
    virtual void BuildCamera() override;
    virtual void BuildSpriteTransforms() override;
    virtual void BuildSpriteRenderers() override;
    virtual void BuildInstancedSpriteTransforms() override;
    virtual void BuildInstancedSpriteRenderers() override;

    virtual const char* GetDisplayName() const override;

    ME::Transform& GetPlayer() {
        return spriteTransforms[0];
    }

    // Moves the player's collider to the player sprite.
    void SyncPlayer();

    // Copies the pools' active slots to their sprites and colliders, and parks the slots freed since the last call.
    void SyncEnemies(ME::Pool<Enemy>& enemies);
    void SyncBullets(ME::Pool<Bullet>& bullets);

    // Half the size of the walkable floor (inside the walls) in world units, centred on the origin.
    float GetFloorHalfWidth() const;
    float GetFloorHalfHeight() const;

    static constexpr float TILE_SIZE = 8.0f;  // world units per tile; the camera shows 200 units vertically
    static constexpr size_t MAX_ENEMIES = 99'000;  // plus bullets, just under the engine's 100k instances/colliders
    static constexpr size_t MAX_BULLETS = 256;

    // Dynamic collider layout: the player, then one per enemy slot, then one per bullet slot.
    static constexpr size_t PLAYER_COLLIDER = 0;
    static constexpr size_t FIRST_ENEMY_COLLIDER = 1;
    static constexpr size_t FIRST_BULLET_COLLIDER = FIRST_ENEMY_COLLIDER + MAX_ENEMIES;

   private:
    static constexpr size_t ARENA_TILES_X = 40;
    static constexpr size_t ARENA_TILES_Y = 22;
    static constexpr size_t ARENA_TILE_COUNT = ARENA_TILES_X * ARENA_TILES_Y;
    static constexpr size_t FIRST_BULLET_INSTANCE = MAX_ENEMIES;  // bullets follow the enemies in instanced set 1

    // Builds every dynamic collider once; the physics system sorts them by layer when the scene is set.
    void BuildColliders();

    // World position of tile (x, y)'s centre; the arena is centred on the origin.
    float TileCenterX(size_t x) const;
    float TileCenterY(size_t y) const;

    ME::Vec3 parkPos{9000.0f, 9000.0f, 0.0f};
    size_t syncedEnemyCount = 0;
    size_t syncedBulletCount = 0;
};

}  // namespace Samples
