/**
 * Tower Climb scene: the tower shaft (instanced set 0, rows recycled from the bottom to the top as they scroll), the
 * hazards (instanced set 1, a fixed pool parked off-screen when unused) and the climber (the only plain sprite).
 * The camera never moves: the world scrolls down past it, so coordinates stay small however high the climb goes.
 * All sprites come from one atlas (Kenney's Tiny Dungeon), loaded into slots 0-2.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "moksha/client/scene/scene.h"
#include "moksha/shared/math/transform.h"
#include "moksha/shared/random/random_engine.h"
#include "tower_climb/game/tower_climb_actors.h"

namespace Samples {

// Tile indices in the Tiny Dungeon atlas (row * 12 + column).
namespace TowerTiles {
constexpr uint16_t BRICK = 40;
constexpr uint16_t BRICK_FRAMED_A = 57;
constexpr uint16_t BRICK_FRAMED_B = 58;
constexpr uint16_t BANNER = 29;
constexpr uint16_t CLIMBER = 96;
}  // namespace TowerTiles

// Sound ids: indices into sfxPaths.
namespace TowerSfx {
constexpr uint8_t JUMP = 0;
constexpr uint8_t HIT = 1;
}  // namespace TowerSfx

class SceneTowerClimb : public ME::Scene {
   public:
    virtual void Init() override;
    virtual void CreateResources() override;
    virtual void BuildCamera() override;
    virtual void BuildSpriteTransforms() override;
    virtual void BuildSpriteRenderers() override;
    virtual void BuildInstancedSpriteTransforms() override;
    virtual void BuildInstancedSpriteRenderers() override;

    virtual const char* GetDisplayName() const override;

    ME::Transform& GetClimber() {
        return spriteTransforms[0];
    }
    ME::SpriteRenderer& GetClimberRenderer() {
        return spriteRenderers[0];
    }

    // Moves every tile row down by distance; a row that leaves the bottom wraps to the top with new tiles.
    void ScrollShaft(float distance);

    // Moves the climber's collider to the climber sprite.
    void SyncClimber();

    // Copies the active hazards to their sprites and colliders, and parks the inactive ones.
    void SyncHazards(const Hazard* hazards, size_t count);

    static constexpr float TILE_SIZE = 8.0f;           // world units per tile; one tile climbed is one metre
    static constexpr float VIEW_HALF_HEIGHT = 80.0f;   // the camera shows 20 tiles vertically
    static constexpr float WALL_X = 28.0f;             // centre of each wall column, mirrored on the left
    static constexpr float CLING_X = WALL_X - TILE_SIZE;  // centre of the climber or a crawler against a wall
    static constexpr float CLIMBER_Y = -40.0f;
    static constexpr size_t MAX_HAZARDS = 16;

    // Dynamic collider layout: the climber, then one per hazard slot.
    static constexpr size_t CLIMBER_COLLIDER = 0;
    static constexpr size_t FIRST_HAZARD_COLLIDER = 1;

   private:
    // 16 columns cover aspect ratios up to 0.8 (portrait phones and tablets); 22 rows cover the view plus a margin.
    static constexpr size_t SHAFT_COLUMNS = 16;
    static constexpr size_t SHAFT_ROWS = 22;
    static constexpr size_t SHAFT_TILE_COUNT = SHAFT_COLUMNS * SHAFT_ROWS;
    static constexpr size_t LEFT_WALL_COLUMN = 4;
    static constexpr size_t RIGHT_WALL_COLUMN = 11;

    void BuildColliders();

    // Picks the tile and tint for every column of row, for a freshly built or wrapped row.
    void RollRow(size_t row);

    float ColumnCenterX(size_t column) const;

    ME::Random shaftRandom{"towerShaft", true};
    ME::Vec3 parkPos{9000.0f, 9000.0f, 0.0f};
};

}  // namespace Samples
