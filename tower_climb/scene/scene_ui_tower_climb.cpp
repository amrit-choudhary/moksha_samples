#include "tower_climb/scene/scene_ui_tower_climb.h"

#include <cstdio>

#include "moksha/client/utils/json_utils.h"

// Builds a centred label anchored to the window's top centre (offsetY from the top) or centre.
static ME::Label* MakeLabel(const char* text, const ME::Color& color, uint16_t glyphSize, ME::UIAnchor anchor,
                            float offsetY) {
    ME::Label* label =
        new ME::Label(text, 0, 1, 0, color, glyphSize, glyphSize, 0, 2, 30, ME::TextAlignment::Center);
    label->SetAnchor(anchor);
    label->SetPivot(ME::UIPivot::Center);
    label->SetOffset(ME::Vec2{0.0f, offsetY});
    label->SetSize(ME::Vec2{420.0f, static_cast<float>(glyphSize) + 8.0f});
    label->Init();
    return label;
}

Samples::SceneUITowerClimb::SceneUITowerClimb() {}

Samples::SceneUITowerClimb::~SceneUITowerClimb() {
    delete hintLabel;
    delete bestLabel;
    delete titleLabel;
    delete heightLabel;
}

void Samples::SceneUITowerClimb::CreateResources() {
    SceneUI::CreateResources();

    spriteTexturePaths[0] = "textures/ui/ui_atlas.dds";
    spriteTexturePaths[1] = "textures/font/ascii_ibm_transparent.dds";
    spriteTexturePaths.count = 2;

    ME::JsonUtils::LoadTextureAtlasProps("texture_data/atlas_ui.json", textureAtlasProperties[0]);
    ME::JsonUtils::LoadTextureAtlasProps("texture_data/font_atlas_01.json", textureAtlasProperties[1]);
    textureAtlasProperties.count = 2;
}

void Samples::SceneUITowerClimb::BuildUIElements() {
    SceneUI::BuildUIElements();

    heightLabel = MakeLabel("0 m", ME::Color::White(), 32, ME::UIAnchor::TopCenter, 40.0f);
    AddUIElement(heightLabel);

    titleLabel = MakeLabel("TOWER CLIMB", ME::Color::Yellow(), 32, ME::UIAnchor::Center, -80.0f);
    AddUIElement(titleLabel);

    bestLabel = MakeLabel("Best: 0 m", ME::Color::White(), 20, ME::UIAnchor::Center, -30.0f);
    AddUIElement(bestLabel);

    hintLabel = MakeLabel("Tap left or right", ME::Color::Green(), 20, ME::UIAnchor::Center, 20.0f);
    AddUIElement(hintLabel);
}

void Samples::SceneUITowerClimb::ShowReady(uint32_t bestMeters) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Best: %u m", bestMeters);
    titleLabel->SetText("TOWER CLIMB");
    bestLabel->SetText(buffer);
    hintLabel->SetText("Tap left or right");
    SetMessageVisible(true);
    SetHeight(0);
}

void Samples::SceneUITowerClimb::ShowPlaying() {
    SetMessageVisible(false);
}

void Samples::SceneUITowerClimb::ShowGameOver(uint32_t meters, uint32_t bestMeters, bool bNewBest) {
    char buffer[32];
    if (bNewBest) {
        snprintf(buffer, sizeof(buffer), "New best: %u m", meters);
    } else {
        snprintf(buffer, sizeof(buffer), "Best: %u m", bestMeters);
    }
    titleLabel->SetText("GAME OVER");
    bestLabel->SetText(buffer);
    hintLabel->SetText("Tap to climb again");
    SetMessageVisible(true);
}

void Samples::SceneUITowerClimb::SetHeight(uint32_t meters) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%u m", meters);
    heightLabel->SetText(buffer);
}

void Samples::SceneUITowerClimb::SetMessageVisible(bool bVisible) {
    titleLabel->SetVisible(bVisible);
    bestLabel->SetVisible(bVisible);
    hintLabel->SetVisible(bVisible);
}
