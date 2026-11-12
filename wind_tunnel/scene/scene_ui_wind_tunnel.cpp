#include "wind_tunnel/scene/scene_ui_wind_tunnel.h"

#include <cstdio>

#include "moksha/client/utils/json_utils.h"

// In points. The labels sit top centre, clear of the debug overlay's rows at the top left.
static constexpr float FPS_OFFSET_Y = 12.0f;
static constexpr float INFO_OFFSET_Y = 38.0f;
static constexpr float FORCES_OFFSET_Y = 58.0f;
static constexpr float BAR_OFFSET_Y = -12.0f;
static constexpr float KEYS_OFFSET_Y = -62.0f;
static constexpr float BUTTON_WIDTH = 92.0f;
static constexpr float BUTTON_HEIGHT = 40.0f;
static constexpr float BUTTON_GAP = 8.0f;

static constexpr uint16_t BUTTON_ATLAS_TILE = 660;
static constexpr float SMOOTH_FPS = 55.0f;  // at or above: green
static constexpr float SLOW_FPS = 28.0f;    // at or above: yellow; below: red

static constexpr const char* BUTTON_TEXT[] = {"Shape", "Foil", "View", "Lines", "Smoke", "-", "+"};
static_assert(sizeof(BUTTON_TEXT) / sizeof(BUTTON_TEXT[0]) ==
              static_cast<size_t>(Samples::SceneUIWindTunnel::ButtonId::Count));

static constexpr const char* KEYS_TEXT =
    "drag: move  1-6 shape  N foil  Up/Down or Q/E turn  C view  S lines  T smoke  V swirl  H detail  -/+ speed  R reset";

Samples::SceneUIWindTunnel::~SceneUIWindTunnel() {
    for (BarButton& bar : buttons) {
        delete bar.label;
        delete bar.panel;
        delete bar.button;
    }
    delete keysLabel;
    delete forcesLabel;
    delete infoLabel;
    delete fpsLabel;
}

void Samples::SceneUIWindTunnel::CreateResources() {
    SceneUI::CreateResources();

    spriteTexturePaths[0] = "textures/ui/ui_atlas.dds";
    spriteTexturePaths[1] = "textures/font/ascii_ibm_transparent.dds";
    spriteTexturePaths.count = 2;

    ME::JsonUtils::LoadTextureAtlasProps("texture_data/atlas_ui.json", textureAtlasProperties[0]);
    ME::JsonUtils::LoadTextureAtlasProps("texture_data/font_atlas_01.json", textureAtlasProperties[1]);
    textureAtlasProperties.count = 2;
}

void Samples::SceneUIWindTunnel::BuildUIElements() {
    SceneUI::BuildUIElements();

    fpsLabel = new ME::Label("-- FPS", 0, 1, 0, ME::Color::Green(), 18, 18, 0, 2, 30, ME::TextAlignment::Center);
    fpsLabel->SetAnchor(ME::UIAnchor::TopCenter);
    fpsLabel->SetPivot(ME::UIPivot::TopCenter);
    fpsLabel->SetOffset(ME::Vec2{0.0f, FPS_OFFSET_Y});
    fpsLabel->SetSize(ME::Vec2{360.0f, 22.0f});
    fpsLabel->Init();
    AddUIElement(fpsLabel);

    infoLabel = new ME::Label("", 0, 1, 0, ME::Color::White(), 12, 12, 0, 2, 72, ME::TextAlignment::Center);
    infoLabel->SetAnchor(ME::UIAnchor::TopCenter);
    infoLabel->SetPivot(ME::UIPivot::TopCenter);
    infoLabel->SetOffset(ME::Vec2{0.0f, INFO_OFFSET_Y});
    infoLabel->SetSize(ME::Vec2{880.0f, 16.0f});
    infoLabel->Init();
    AddUIElement(infoLabel);

    forcesLabel = new ME::Label("", 0, 1, 0, ME::Color(0.7f, 1.0f, 0.75f, 1.0f), 12, 12, 0, 2, 60,
                                ME::TextAlignment::Center);
    forcesLabel->SetAnchor(ME::UIAnchor::TopCenter);
    forcesLabel->SetPivot(ME::UIPivot::TopCenter);
    forcesLabel->SetOffset(ME::Vec2{0.0f, FORCES_OFFSET_Y});
    forcesLabel->SetSize(ME::Vec2{740.0f, 16.0f});
    forcesLabel->Init();
    AddUIElement(forcesLabel);

    keysLabel = new ME::Label(KEYS_TEXT, 0, 1, 0, ME::Color(0.75f, 0.8f, 0.9f, 0.8f), 10, 8, 0, 2, 110,
                              ME::TextAlignment::Center);
    keysLabel->SetAnchor(ME::UIAnchor::BottomCenter);
    keysLabel->SetPivot(ME::UIPivot::BottomCenter);
    keysLabel->SetOffset(ME::Vec2{0.0f, KEYS_OFFSET_Y});
    keysLabel->SetSize(ME::Vec2{960.0f, 12.0f});
    keysLabel->Init();
    AddUIElement(keysLabel);

    // One row, centred at the bottom.
    constexpr size_t count = static_cast<size_t>(ButtonId::Count);
    const float rowWidth = count * BUTTON_WIDTH + (count - 1) * BUTTON_GAP;
    const ME::Color normal(1.0f, 1.0f, 1.0f, 0.7f);
    for (size_t b = 0; b < count; ++b) {
        BarButton& bar = buttons[b];
        const float centreX = -0.5f * rowWidth + 0.5f * BUTTON_WIDTH + static_cast<float>(b) * (BUTTON_WIDTH + BUTTON_GAP);

        bar.button = new ME::Button();
        bar.button->SetAnchor(ME::UIAnchor::BottomCenter);
        bar.button->SetPivot(ME::UIPivot::BottomCenter);
        bar.button->SetOffset(ME::Vec2{centreX, BAR_OFFSET_Y});
        bar.button->SetSize(ME::Vec2{BUTTON_WIDTH, BUTTON_HEIGHT});
        bar.button->Init();
        AddUIElement(bar.button);

        bar.panel = new ME::Panel(0, 0, 0, 0, BUTTON_ATLAS_TILE);
        bar.panel->SetAnchor(ME::UIAnchor::Center);
        bar.panel->SetPivot(ME::UIPivot::Center);
        bar.panel->SetSize(ME::Vec2{BUTTON_WIDTH, BUTTON_HEIGHT});
        bar.panel->SetParent(bar.button);
        bar.panel->Init();
        AddUIElement(bar.panel);

        bar.label = new ME::Label(BUTTON_TEXT[b], 0, 1, 0, ME::Color::Black(), 16, 15, 0, 2, 8,
                                  ME::TextAlignment::Center);
        bar.label->SetAnchor(ME::UIAnchor::Center);
        bar.label->SetPivot(ME::UIPivot::Center);
        bar.label->SetSize(ME::Vec2{BUTTON_WIDTH, BUTTON_HEIGHT});
        bar.label->SetParent(bar.button);
        bar.label->Init();
        AddUIElement(bar.label);

        // See-through over the flow. A finger stays "hovering" after it lifts, so hover keeps the normal colour.
        bar.button->SetNormalColor(normal);
        bar.button->SetHoverColor(normal);
        bar.button->SetPanel(bar.panel);
    }
}

void Samples::SceneUIWindTunnel::SetFps(double fps) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.0f FPS  %.1f ms", fps, fps > 0.0 ? 1000.0 / fps : 0.0);
    fpsLabel->SetText(buffer);
    fpsLabel->SetColor(fps >= SMOOTH_FPS ? ME::Color::Green()
                       : fps >= SLOW_FPS ? ME::Color::Yellow()
                                         : ME::Color::Red());
}

void Samples::SceneUIWindTunnel::SetInfo(const char* text) {
    infoLabel->SetText(text);
}

void Samples::SceneUIWindTunnel::SetForces(const char* text) {
    forcesLabel->SetText(text);
}

bool Samples::SceneUIWindTunnel::IsOverButton(float x, float y) const {
    const ME::Vec2 point{x, y};
    for (const BarButton& bar : buttons) {
        if (bar.button != nullptr && bar.button->GetRect()->Contains(point)) {
            return true;
        }
    }
    return false;
}
