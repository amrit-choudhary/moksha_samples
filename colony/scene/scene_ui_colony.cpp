#include "colony/scene/scene_ui_colony.h"

#include <cstdio>

#include "moksha/client/utils/json_utils.h"

// In points. Landscape: top centre, clear of the debug overlay's rows at the top left.
static constexpr float FPS_OFFSET_Y = 16.0f;

static constexpr float SMOOTH_FPS = 55.0f;  // at or above: green
static constexpr float SLOW_FPS = 28.0f;    // at or above: yellow; below: red

Samples::SceneUIColony::~SceneUIColony() {
    delete fpsLabel;
}

void Samples::SceneUIColony::CreateResources() {
    SceneUI::CreateResources();

    spriteTexturePaths[0] = "textures/ui/ui_atlas.dds";
    spriteTexturePaths[1] = "textures/font/ascii_ibm_transparent.dds";
    spriteTexturePaths.count = 2;

    ME::JsonUtils::LoadTextureAtlasProps("texture_data/atlas_ui.json", textureAtlasProperties[0]);
    ME::JsonUtils::LoadTextureAtlasProps("texture_data/font_atlas_01.json", textureAtlasProperties[1]);
    textureAtlasProperties.count = 2;
}

void Samples::SceneUIColony::BuildUIElements() {
    SceneUI::BuildUIElements();

    fpsLabel = new ME::Label("-- FPS", 0, 1, 0, ME::Color::Green(), 20, 20, 0, 2, 30, ME::TextAlignment::Center);
    fpsLabel->SetAnchor(ME::UIAnchor::TopCenter);
    fpsLabel->SetPivot(ME::UIPivot::TopCenter);
    fpsLabel->SetOffset(ME::Vec2{0.0f, FPS_OFFSET_Y});
    fpsLabel->SetSize(ME::Vec2{360.0f, 26.0f});
    fpsLabel->Init();
    AddUIElement(fpsLabel);
}

void Samples::SceneUIColony::SetFps(double fps) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.0f FPS  %.1f ms", fps, fps > 0.0 ? 1000.0 / fps : 0.0);
    fpsLabel->SetText(buffer);
    fpsLabel->SetColor(fps >= SMOOTH_FPS ? ME::Color::Green()
                       : fps >= SLOW_FPS ? ME::Color::Yellow()
                                         : ME::Color::Red());
}
