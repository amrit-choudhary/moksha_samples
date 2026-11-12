#include "particle_life/scene/scene_ui_particle_life.h"

#include <cstdio>

#include "moksha/client/utils/json_utils.h"

// In points. Landscape: the labels sit top centre, clear of the debug overlay's rows at the top left.
static constexpr float FPS_OFFSET_Y = 16.0f;
static constexpr float INFO_OFFSET_Y = 44.0f;
static constexpr float BUTTON_ROW_OFFSET_Y = -40.0f;
static constexpr float BUTTON_MARGIN = 16.0f;
static constexpr float BUTTON_WIDTH = 64.0f;
static constexpr float BUTTON_HEIGHT = 56.0f;

static constexpr uint16_t BUTTON_ATLAS_TILE = 660;
static constexpr float SMOOTH_FPS = 55.0f;  // at or above: green
static constexpr float SLOW_FPS = 28.0f;    // at or above: yellow; below: red

Samples::SceneUIParticleLife::~SceneUIParticleLife() {
    StepButton* const buttons[] = {&upButton, &downButton};
    for (StepButton* step : buttons) {
        delete step->label;
        delete step->panel;
        delete step->button;
    }
    delete infoLabel;
    delete fpsLabel;
}

void Samples::SceneUIParticleLife::CreateResources() {
    SceneUI::CreateResources();

    spriteTexturePaths[0] = "textures/ui/ui_atlas.dds";
    spriteTexturePaths[1] = "textures/font/ascii_ibm_transparent.dds";
    spriteTexturePaths.count = 2;

    ME::JsonUtils::LoadTextureAtlasProps("texture_data/atlas_ui.json", textureAtlasProperties[0]);
    ME::JsonUtils::LoadTextureAtlasProps("texture_data/font_atlas_01.json", textureAtlasProperties[1]);
    textureAtlasProperties.count = 2;
}

void Samples::SceneUIParticleLife::BuildUIElements() {
    SceneUI::BuildUIElements();

    fpsLabel = new ME::Label("-- FPS", 0, 1, 0, ME::Color::Green(), 20, 20, 0, 2, 30, ME::TextAlignment::Center);
    fpsLabel->SetAnchor(ME::UIAnchor::TopCenter);
    fpsLabel->SetPivot(ME::UIPivot::TopCenter);
    fpsLabel->SetOffset(ME::Vec2{0.0f, FPS_OFFSET_Y});
    fpsLabel->SetSize(ME::Vec2{360.0f, 26.0f});
    fpsLabel->Init();
    AddUIElement(fpsLabel);

    infoLabel = new ME::Label("", 0, 1, 0, ME::Color::White(), 12, 12, 0, 2, 40, ME::TextAlignment::Center);
    infoLabel->SetAnchor(ME::UIAnchor::TopCenter);
    infoLabel->SetPivot(ME::UIPivot::TopCenter);
    infoLabel->SetOffset(ME::Vec2{0.0f, INFO_OFFSET_Y});
    infoLabel->SetSize(ME::Vec2{400.0f, 16.0f});
    infoLabel->Init();
    AddUIElement(infoLabel);

    BuildStepButton(downButton, "-", ME::UIAnchor::BottomLeft, BUTTON_MARGIN);
    BuildStepButton(upButton, "+", ME::UIAnchor::BottomRight, -BUTTON_MARGIN);
}

void Samples::SceneUIParticleLife::SetFps(double fps) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.0f FPS  %.1f ms", fps, fps > 0.0 ? 1000.0 / fps : 0.0);
    fpsLabel->SetText(buffer);
    fpsLabel->SetColor(fps >= SMOOTH_FPS ? ME::Color::Green()
                       : fps >= SLOW_FPS ? ME::Color::Yellow()
                                         : ME::Color::Red());
}

void Samples::SceneUIParticleLife::SetInfo(uint32_t particleCount, double simMilliseconds) {
    // Thousands separated, as 100,000.
    char count[16];
    if (particleCount >= 1000) {
        snprintf(count, sizeof(count), "%u,%03u", particleCount / 1000, particleCount % 1000);
    } else {
        snprintf(count, sizeof(count), "%u", particleCount);
    }
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "%s particles  sim %.1f ms", count, simMilliseconds);
    infoLabel->SetText(buffer);
}

void Samples::SceneUIParticleLife::BuildStepButton(StepButton& out, const char* text, ME::UIAnchor anchor,
                                                   float offsetX) {
    out.button = new ME::Button();
    out.button->SetAnchor(anchor);
    out.button->SetPivot(anchor == ME::UIAnchor::BottomLeft ? ME::UIPivot::BottomLeft : ME::UIPivot::BottomRight);
    out.button->SetOffset(ME::Vec2{offsetX, BUTTON_ROW_OFFSET_Y});
    out.button->SetSize(ME::Vec2{BUTTON_WIDTH, BUTTON_HEIGHT});
    out.button->Init();
    AddUIElement(out.button);

    out.panel = new ME::Panel(0, 0, 0, 0, BUTTON_ATLAS_TILE);
    out.panel->SetAnchor(ME::UIAnchor::Center);
    out.panel->SetPivot(ME::UIPivot::Center);
    out.panel->SetSize(ME::Vec2{BUTTON_WIDTH, BUTTON_HEIGHT});
    out.panel->SetParent(out.button);
    out.panel->Init();
    AddUIElement(out.panel);

    out.label = new ME::Label(text, 0, 1, 0, ME::Color::Black(), 32, 32, 0, 2, 4, ME::TextAlignment::Center);
    out.label->SetAnchor(ME::UIAnchor::Center);
    out.label->SetPivot(ME::UIPivot::Center);
    out.label->SetSize(ME::Vec2{BUTTON_WIDTH, BUTTON_HEIGHT});
    out.label->SetParent(out.button);
    out.label->Init();
    AddUIElement(out.label);

    // Slightly see-through over the particles. A finger stays "hovering" after it lifts, so hover keeps the normal
    // colour.
    const ME::Color normal(1.0f, 1.0f, 1.0f, 0.75f);
    out.button->SetNormalColor(normal);
    out.button->SetHoverColor(normal);
    out.button->SetPanel(out.panel);
}
