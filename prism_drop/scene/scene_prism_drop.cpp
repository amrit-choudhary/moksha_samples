#include "prism_drop/scene/scene_prism_drop.h"

#include "moksha/client/utils/json_utils.h"

// Texture slots. Set 0 always draws slot 1 and set 1 slot 2; plain sprites pick their own.
static constexpr uint8_t WELL_TEXTURE = 0;
static constexpr uint8_t BLOCKS_TEXTURE = 1;
static constexpr uint8_t FX_TEXTURE = 2;
static constexpr uint8_t BACKDROP_TEXTURE = 3;
static constexpr uint8_t BOX_TEXTURE = 4;

// Depths: plain sprites draw first, so they are farthest. Ambient motes sit between the backdrop and the well, so the
// well hides them; blocks and effects are nearer than both.
static constexpr float BACKDROP_Z = 98.0f;
static constexpr float AMBIENT_Z = 97.0f;
static constexpr float FRAME_Z = 96.0f;
static constexpr float BLOCK_Z = 50.0f;
static constexpr float EFFECT_Z = 20.0f;
static constexpr float SLOT_Z_STEP = 0.004f;

// The backdrop image's aspect ratio, kept when it is stretched to cover the view.
static constexpr float BACKDROP_ASPECT = 360.0f / 640.0f;

void Samples::ScenePrismDrop::CreateResources() {
    ME::Scene::CreateResources();

    spriteTexturePaths[WELL_TEXTURE] = "textures/prism/well.dds";
    spriteTexturePaths[BLOCKS_TEXTURE] = "textures/prism/blocks.dds";
    spriteTexturePaths[FX_TEXTURE] = "textures/prism/fx.dds";
    spriteTexturePaths[BACKDROP_TEXTURE] = "textures/prism/backdrop.dds";
    spriteTexturePaths[BOX_TEXTURE] = "textures/prism/box.dds";
    spriteTexturePaths.count = 5;

    ME::JsonUtils::LoadTextureAtlasProps("texture_data/atlas_well.json", textureAtlasProperties[WELL_TEXTURE]);
    ME::JsonUtils::LoadTextureAtlasProps("texture_data/atlas_blocks.json", textureAtlasProperties[BLOCKS_TEXTURE]);
    ME::JsonUtils::LoadTextureAtlasProps("texture_data/atlas_fx.json", textureAtlasProperties[FX_TEXTURE]);
    ME::JsonUtils::LoadTextureAtlasProps("texture_data/atlas_backdrop.json", textureAtlasProperties[BACKDROP_TEXTURE]);
    ME::JsonUtils::LoadTextureAtlasProps("texture_data/atlas_box.json", textureAtlasProperties[BOX_TEXTURE]);
    textureAtlasProperties.count = 5;

    sfxPaths[PrismSfx::MOVE] = "audio/move.wav";
    sfxPaths[PrismSfx::ROTATE] = "audio/rotate.wav";
    sfxPaths[PrismSfx::HARD_DROP] = "audio/hard_drop.wav";
    sfxPaths[PrismSfx::LOCK] = "audio/lock.wav";
    sfxPaths[PrismSfx::CLEAR1] = "audio/clear1.wav";
    sfxPaths[PrismSfx::CLEAR2] = "audio/clear2.wav";
    sfxPaths[PrismSfx::CLEAR3] = "audio/clear3.wav";
    sfxPaths[PrismSfx::QUAD] = "audio/quad.wav";
    sfxPaths[PrismSfx::HOLD] = "audio/hold.wav";
    sfxPaths[PrismSfx::LEVEL_UP] = "audio/level_up.wav";
    sfxPaths[PrismSfx::GAME_OVER] = "audio/game_over.wav";
    sfxPaths[PrismSfx::START] = "audio/start.wav";
    sfxPaths[PrismSfx::PAUSE] = "audio/pause.wav";
    sfxPaths.count = PrismSfx::COUNT;

    musicPaths[0] = "audio/music.wav";
    musicPaths.count = 1;
}

void Samples::ScenePrismDrop::BuildCamera() {
    ME::Scene::BuildCamera();
    spriteCamera->orthographicSize = VIEW_HALF_HEIGHT;
}

void Samples::ScenePrismDrop::BuildSpriteTransforms() {
    ME::Scene::BuildSpriteTransforms();

    const float wellWidth = BOARD_WIDTH_UNITS + WELL_BORDER * 2.0f;
    const float wellHeight = BOARD_HEIGHT_UNITS + WELL_BORDER * 2.0f;
    spriteTransforms[WELL_SPRITE].SetPosition(BOARD_LEFT + BOARD_WIDTH_UNITS * 0.5f,
                                              BOARD_BOTTOM + BOARD_HEIGHT_UNITS * 0.5f, FRAME_Z);
    spriteTransforms[WELL_SPRITE].SetScale(wellWidth, wellHeight);
    spriteTransforms[HOLD_SPRITE].SetPosition(-BOX_X, BOX_Y, FRAME_Z - SLOT_Z_STEP);
    spriteTransforms[HOLD_SPRITE].SetScale(BOX_SIZE, BOX_SIZE);
    spriteTransforms[NEXT_SPRITE].SetPosition(BOX_X, BOX_Y, FRAME_Z - SLOT_Z_STEP * 2.0f);
    spriteTransforms[NEXT_SPRITE].SetScale(BOX_SIZE, BOX_SIZE);
    spriteTransforms.count = SPRITE_COUNT;
    SetAspect(0.5625f);
}

void Samples::ScenePrismDrop::BuildSpriteRenderers() {
    ME::Scene::BuildSpriteRenderers();

    spriteRenderers[BACKDROP_SPRITE] = ME::SpriteRenderer(0, 0, BACKDROP_TEXTURE, BACKDROP_TEXTURE, 0);
    spriteRenderers[WELL_SPRITE] = ME::SpriteRenderer(0, 0, WELL_TEXTURE, WELL_TEXTURE, 0);
    spriteRenderers[HOLD_SPRITE] = ME::SpriteRenderer(0, 0, BOX_TEXTURE, BOX_TEXTURE, 0);
    spriteRenderers[NEXT_SPRITE] = ME::SpriteRenderer(0, 0, BOX_TEXTURE, BOX_TEXTURE, 0);
    spriteRenderers.count = SPRITE_COUNT;
}

void Samples::ScenePrismDrop::BuildInstancedSpriteTransforms() {
    ME::Scene::BuildInstancedSpriteTransforms();

    // Every slot starts hidden: zero scale draws no pixels.
    instancedSpriteTransforms0.count = BLOCK_SLOTS;
    for (size_t i = 0; i < BLOCK_SLOTS; ++i) {
        instancedSpriteTransforms0[i].SetPosition(0.0f, 0.0f, BlockDepth(i));
        instancedSpriteTransforms0[i].SetScale(0.0f, 0.0f);
    }
    instancedSpriteTransforms1.count = FX_SLOTS;
    for (size_t i = 0; i < FX_SLOTS; ++i) {
        instancedSpriteTransforms1[i].SetPosition(0.0f, 0.0f, FxDepth(i));
        instancedSpriteTransforms1[i].SetScale(0.0f, 0.0f);
    }
}

void Samples::ScenePrismDrop::BuildInstancedSpriteRenderers() {
    ME::Scene::BuildInstancedSpriteRenderers();

    instancedSpriteRenderers0.count = BLOCK_SLOTS;
    for (size_t i = 0; i < BLOCK_SLOTS; ++i) {
        instancedSpriteRenderers0[i] = ME::SpriteRenderer(0, 0, BLOCKS_TEXTURE, BLOCKS_TEXTURE, 0);
    }
    instancedSpriteRenderers1.count = FX_SLOTS;
    for (size_t i = 0; i < FX_SLOTS; ++i) {
        instancedSpriteRenderers1[i] = ME::SpriteRenderer(0, 0, FX_TEXTURE, FX_TEXTURE, 0);
    }
}

const char* Samples::ScenePrismDrop::GetDisplayName() const {
    return "Prism Drop Scene";
}

void Samples::ScenePrismDrop::SetAspect(float aspect) {
    if (spriteCamera != nullptr) {
        spriteCamera->aspectRatio = aspect;
    }
    viewHalfWidth = VIEW_HALF_HEIGHT * aspect;

    // Cover the view, with a margin for screen shake.
    float height = VIEW_HALF_HEIGHT * 2.0f * 1.06f;
    float width = height * BACKDROP_ASPECT;
    if (width < viewHalfWidth * 2.0f * 1.06f) {
        width = viewHalfWidth * 2.0f * 1.06f;
        height = width / BACKDROP_ASPECT;
    }
    spriteTransforms[BACKDROP_SPRITE].SetPosition(0.0f, 0.0f, BACKDROP_Z);
    spriteTransforms[BACKDROP_SPRITE].SetScale(width, height);
}

void Samples::ScenePrismDrop::SetCameraOffset(float x, float y) {
    spriteCamera->position = ME::Vec3(x, y, 0.0f);
    spriteCamera->viewPosition = ME::Vec3(x, y, 1.0f);
}

void Samples::ScenePrismDrop::SetBackdropTint(const ME::Color& tint) {
    spriteRenderers[BACKDROP_SPRITE].color = tint;
}

void Samples::ScenePrismDrop::SetFrameTint(const ME::Color& tint) {
    spriteRenderers[WELL_SPRITE].color = tint;
    spriteRenderers[HOLD_SPRITE].color = tint;
    spriteRenderers[NEXT_SPRITE].color = tint;
}

void Samples::ScenePrismDrop::SetBlock(size_t slot, float x, float y, float size, float angle, uint16_t tile,
                                       const ME::Color& color) {
    if (slot >= BLOCK_SLOTS) {
        return;
    }
    ME::Transform& transform = instancedSpriteTransforms0[slot];
    transform.SetPosition(x, y, BlockDepth(slot));
    transform.SetScale(size, size);
    transform.SetRotation(0.0f, 0.0f, angle);
    ME::SpriteRenderer& renderer = instancedSpriteRenderers0[slot];
    renderer.atlasIndex = tile;
    renderer.color = color;
    renderer.bDirty = true;
}

void Samples::ScenePrismDrop::SetBlockStretched(size_t slot, float x, float y, float width, float height, uint16_t tile,
                                                const ME::Color& color) {
    SetBlock(slot, x, y, 1.0f, 0.0f, tile, color);
    if (slot < BLOCK_SLOTS) {
        instancedSpriteTransforms0[slot].SetScale(width, height);
    }
}

void Samples::ScenePrismDrop::HideBlock(size_t slot) {
    if (slot >= BLOCK_SLOTS) {
        return;
    }
    if (instancedSpriteTransforms0[slot].GetScale().x != 0.0f) {
        instancedSpriteTransforms0[slot].SetScale(0.0f, 0.0f);
        instancedSpriteRenderers0[slot].bDirty = true;
    }
}

void Samples::ScenePrismDrop::SetFx(size_t slot, float x, float y, float width, float height, float angle,
                                    uint16_t tile, const ME::Color& color) {
    if (slot >= FX_SLOTS) {
        return;
    }
    ME::Transform& transform = instancedSpriteTransforms1[slot];
    transform.SetPosition(x, y, FxDepth(slot));
    transform.SetScale(width, height);
    transform.SetRotation(0.0f, 0.0f, angle);
    ME::SpriteRenderer& renderer = instancedSpriteRenderers1[slot];
    renderer.atlasIndex = tile;
    renderer.color = color;
    renderer.bDirty = true;
}

void Samples::ScenePrismDrop::HideFx(size_t slot) {
    if (slot >= FX_SLOTS) {
        return;
    }
    if (instancedSpriteTransforms1[slot].GetScale().x != 0.0f) {
        instancedSpriteTransforms1[slot].SetScale(0.0f, 0.0f);
        instancedSpriteRenderers1[slot].bDirty = true;
    }
}

float Samples::ScenePrismDrop::BlockDepth(size_t slot) {
    return BLOCK_Z - static_cast<float>(slot) * SLOT_Z_STEP;
}

float Samples::ScenePrismDrop::FxDepth(size_t slot) {
    if (slot < AMBIENT_SLOTS) {
        return AMBIENT_Z + 0.5f - static_cast<float>(slot) * SLOT_Z_STEP;
    }
    return EFFECT_Z - static_cast<float>(slot - AMBIENT_SLOTS) * SLOT_Z_STEP;
}
