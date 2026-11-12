/**
 * Prism Drop scene. Plain sprites: the backdrop, the well and the hold and next boxes, drawn first and farthest.
 * Instanced set 0 holds every block (the well's cells, ghost, falling piece, previews and row flashes) in fixed slots;
 * set 1 is the particle pool, with ambient motes first, placed behind the well, blended additively so effects glow.
 * Depth is Less with writes on for blocks, so each block gets its own depth, nearer with its slot, and a later slot
 * blends over an earlier one; the additive effects test depth but don't write it.
 * World units: the view is 800 tall (y -400 to 400) with a 30-unit cell; the width follows the window.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "moksha/client/scene/scene.h"

namespace Samples {

// Tiles of blocks.png (8 x 2): the seven piece blocks, then these.
namespace PrismTiles {
constexpr uint16_t WHITE = 7;
constexpr uint16_t GHOST = 8;
constexpr uint16_t GLOW = 9;
constexpr uint16_t SOLID = 10;
constexpr uint16_t BAR = 11;
}  // namespace PrismTiles

// Tiles of fx.png (8 x 1).
namespace PrismFxTiles {
constexpr uint16_t DOT = 0;
constexpr uint16_t SPARK = 1;
constexpr uint16_t TWINKLE = 2;
constexpr uint16_t RING = 3;
constexpr uint16_t SHARD = 4;
constexpr uint16_t STREAK = 5;
constexpr uint16_t GLOW = 6;
constexpr uint16_t SMOKE = 7;
}  // namespace PrismFxTiles

// Sound ids: indices into sfxPaths.
namespace PrismSfx {
constexpr uint8_t MOVE = 0;
constexpr uint8_t ROTATE = 1;
constexpr uint8_t HARD_DROP = 2;
constexpr uint8_t LOCK = 3;
constexpr uint8_t CLEAR1 = 4;
constexpr uint8_t CLEAR2 = 5;
constexpr uint8_t CLEAR3 = 6;
constexpr uint8_t QUAD = 7;
constexpr uint8_t HOLD = 8;
constexpr uint8_t LEVEL_UP = 9;
constexpr uint8_t GAME_OVER = 10;
constexpr uint8_t START = 11;
constexpr uint8_t PAUSE = 12;
constexpr uint8_t COUNT = 13;
}  // namespace PrismSfx

class ScenePrismDrop : public ME::Scene {
   public:
    virtual void CreateResources() override;
    virtual void BuildCamera() override;
    virtual void BuildSpriteTransforms() override;
    virtual void BuildSpriteRenderers() override;
    virtual void BuildInstancedSpriteTransforms() override;
    virtual void BuildInstancedSpriteRenderers() override;

    virtual const char* GetDisplayName() const override;

    // Fits the camera and the backdrop to the window's aspect ratio (width / height).
    void SetAspect(float aspect);

    // Offsets the camera, for screen shake.
    void SetCameraOffset(float x, float y);

    // Tints the backdrop, the well and the boxes (mood, level and danger).
    void SetBackdropTint(const ME::Color& tint);
    void SetFrameTint(const ME::Color& tint);

    // Places block slot at (x, y) with size, angle (radians) and tint; Hide parks it. Both mark the slot dirty.
    void SetBlock(size_t slot, float x, float y, float size, float angle, uint16_t tile, const ME::Color& color);
    void SetBlockStretched(size_t slot, float x, float y, float width, float height, uint16_t tile,
                           const ME::Color& color);
    void HideBlock(size_t slot);

    // The same for particle slots, which can stretch.
    void SetFx(size_t slot, float x, float y, float width, float height, float angle, uint16_t tile,
               const ME::Color& color);
    void HideFx(size_t slot);

    float GetViewHalfWidth() const {
        return viewHalfWidth;
    }

    // Layout, in world units.
    static constexpr float VIEW_HALF_HEIGHT = 400.0f;
    static constexpr float CELL = 30.0f;
    static constexpr float BOARD_LEFT = -150.0f;    // 10 cells wide, centred
    static constexpr float BOARD_BOTTOM = -345.0f;  // 20 visible rows up to y = 255
    static constexpr float BOARD_WIDTH_UNITS = CELL * 10.0f;
    static constexpr float BOARD_HEIGHT_UNITS = CELL * 20.0f;
    static constexpr float WELL_BORDER = 15.0f;  // the well texture's 16 px border at 30 units per 32 px cell
    static constexpr float BOX_SIZE = 84.0f;
    static constexpr float BOX_X = 112.0f;  // hold box at -x, next box at +x
    static constexpr float BOX_Y = 318.0f;
    static constexpr float PREVIEW_CELL = 17.0f;

    // Block slots (set 0), in draw order.
    static constexpr size_t BOARD_SLOT = 0;  // row * 10 + column, the 20 visible rows
    static constexpr size_t BOARD_SLOTS = 200;
    static constexpr size_t GHOST_SLOT = BOARD_SLOT + BOARD_SLOTS;
    static constexpr size_t PIECE_SLOT = GHOST_SLOT + 4;
    static constexpr size_t HOLD_SLOT = PIECE_SLOT + 4;
    static constexpr size_t NEXT_SLOT = HOLD_SLOT + 4;
    static constexpr size_t LOCK_GLOW_SLOT = NEXT_SLOT + 4;
    static constexpr size_t ROW_FLASH_SLOT = LOCK_GLOW_SLOT + 4;
    static constexpr size_t BLOCK_SLOTS = ROW_FLASH_SLOT + 4;

    // Particle slots (set 1).
    static constexpr size_t AMBIENT_SLOTS = 90;
    static constexpr size_t EFFECT_SLOTS = 1200;
    static constexpr size_t FX_SLOTS = AMBIENT_SLOTS + EFFECT_SLOTS;

   private:
    // Plain sprites, in draw order.
    static constexpr size_t BACKDROP_SPRITE = 0;
    static constexpr size_t WELL_SPRITE = 1;
    static constexpr size_t HOLD_SPRITE = 2;
    static constexpr size_t NEXT_SPRITE = 3;
    static constexpr size_t SPRITE_COUNT = 4;

    static float BlockDepth(size_t slot);
    static float FxDepth(size_t slot);

    float viewHalfWidth = 225.0f;
};

}  // namespace Samples
