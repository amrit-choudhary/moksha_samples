#include "graphics_stress/scene/scene_ui_graphics_stress.h"

#include <cstdio>

#include "moksha/client/utils/json_utils.h"
#include "moksha/shared/random/random_engine.h"

// In points, above an iPhone's home indicator and below its Dynamic Island and the debug overlay's rows.
static constexpr float FPS_OFFSET_Y = 110.0f;
static constexpr float BUTTON_ROW_OFFSET_Y = -40.0f;
static constexpr float BUTTON_MARGIN = 16.0f;
static constexpr float BUTTON_WIDTH = 64.0f;
static constexpr float BUTTON_HEIGHT = 56.0f;
static constexpr float BUTTON_STRIDE = BUTTON_WIDTH + 8.0f;
static constexpr float FIRST_ROW_OFFSET_Y = -106.0f;  // the last row of the list; earlier rows stack above it
static constexpr float ROW_HEIGHT = 22.0f;
static constexpr uint16_t ROW_GLYPH_SIZE = 16;

// Stress widgets scatter over this area, between the frame rate and the controls on a portrait phone.
static constexpr float STRESS_AREA_TOP = 150.0f;
static constexpr float STRESS_AREA_WIDTH = 400.0f;
static constexpr float STRESS_AREA_HEIGHT = 390.0f;
static constexpr float STRESS_PANEL_SIZE = 48.0f;
static constexpr uint16_t STRESS_GLYPH_SIZE = 10;
// 240 of these stay within the engine's 10,000 text glyphs.
static constexpr const char* STRESS_TEXT = "MOKSHA RENDERER 0123456789 ABCD";

static constexpr uint16_t BUTTON_ATLAS_TILE = 660;
static constexpr uint16_t PANEL_ATLAS_TILE = 48;
static constexpr float SMOOTH_FPS = 55.0f;  // at or above: green
static constexpr float SLOW_FPS = 28.0f;    // at or above: yellow; below: red

static float Random01(ME::Random& random) {
    return static_cast<float>(random.NextDouble());
}

Samples::SceneUIGraphicsStress::SceneUIGraphicsStress() {}

Samples::SceneUIGraphicsStress::~SceneUIGraphicsStress() {
    StepButton* const buttons[] = {&upButton, &downButton, &nextButton, &previousButton};
    for (StepButton* step : buttons) {
        delete step->label;
        delete step->panel;
        delete step->button;
    }
    for (ME::Label* label : rowLabels) {
        delete label;
    }
    delete fpsLabel;
    delete controlsPanel;
    for (ME::Label* label : stressLabels) {
        delete label;
    }
    for (ME::Panel* panel : stressPanels) {
        delete panel;
    }
}

void Samples::SceneUIGraphicsStress::CreateResources() {
    SceneUI::CreateResources();

    spriteTexturePaths[0] = "textures/ui/ui_atlas.dds";
    spriteTexturePaths[1] = "textures/font/ascii_ibm_transparent.dds";
    spriteTexturePaths.count = 2;

    ME::JsonUtils::LoadTextureAtlasProps("texture_data/atlas_ui.json", textureAtlasProperties[0]);
    ME::JsonUtils::LoadTextureAtlasProps("texture_data/font_atlas_01.json", textureAtlasProperties[1]);
    textureAtlasProperties.count = 2;
}

void Samples::SceneUIGraphicsStress::BuildUIElements() {
    SceneUI::BuildUIElements();

    BuildStressWidgets();

    // Wider than any portrait screen, so it spans the width without knowing it.
    controlsPanel = new ME::Panel(0, 0, 0, 0, PANEL_ATLAS_TILE, ME::Color(0.0f, 0.0f, 0.0f, 0.6f));
    controlsPanel->SetAnchor(ME::UIAnchor::BottomCenter);
    controlsPanel->SetPivot(ME::UIPivot::BottomCenter);
    controlsPanel->SetSize(ME::Vec2{1200.0f, -FIRST_ROW_OFFSET_Y + ROW_HEIGHT * ROW_COUNT + 8.0f});
    controlsPanel->Init();
    AddUIElement(controlsPanel);

    fpsLabel = new ME::Label("-- FPS", 0, 1, 0, ME::Color::Green(), 22, 22, 0, 2, 30, ME::TextAlignment::Center);
    fpsLabel->SetAnchor(ME::UIAnchor::TopCenter);
    fpsLabel->SetPivot(ME::UIPivot::TopCenter);
    fpsLabel->SetOffset(ME::Vec2{0.0f, FPS_OFFSET_Y});
    fpsLabel->SetSize(ME::Vec2{360.0f, 30.0f});
    fpsLabel->Init();
    AddUIElement(fpsLabel);

    for (size_t i = 0; i < ROW_COUNT; ++i) {
        const float rowsBelow = static_cast<float>(ROW_COUNT - 1 - i);
        rowLabels[i] = new ME::Label("", 0, 1, 0, ME::Color::White(), ROW_GLYPH_SIZE, ROW_GLYPH_SIZE, 0, 2, 30,
                                     ME::TextAlignment::Left);
        rowLabels[i]->SetAnchor(ME::UIAnchor::BottomCenter);
        rowLabels[i]->SetPivot(ME::UIPivot::BottomCenter);
        rowLabels[i]->SetOffset(ME::Vec2{0.0f, FIRST_ROW_OFFSET_Y - rowsBelow * ROW_HEIGHT});
        rowLabels[i]->SetSize(ME::Vec2{340.0f, ROW_HEIGHT});
        rowLabels[i]->Init();
        AddUIElement(rowLabels[i]);
    }

    BuildStepButton(previousButton, "^", ME::UIAnchor::BottomLeft, BUTTON_MARGIN);
    BuildStepButton(nextButton, "v", ME::UIAnchor::BottomLeft, BUTTON_MARGIN + BUTTON_STRIDE);
    BuildStepButton(downButton, "-", ME::UIAnchor::BottomRight, -BUTTON_MARGIN - BUTTON_STRIDE);
    BuildStepButton(upButton, "+", ME::UIAnchor::BottomRight, -BUTTON_MARGIN);
}

void Samples::SceneUIGraphicsStress::SetFps(double fps) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.0f FPS  %.1f ms", fps, fps > 0.0 ? 1000.0 / fps : 0.0);
    fpsLabel->SetText(buffer);
    fpsLabel->SetColor(fps >= SMOOTH_FPS ? ME::Color::Green()
                       : fps >= SLOW_FPS ? ME::Color::Yellow()
                                         : ME::Color::Red());
}

void Samples::SceneUIGraphicsStress::SetRow(size_t index, const char* text, bool bSelected) {
    if (index >= ROW_COUNT) {
        return;
    }
    rowLabels[index]->SetText(text);
    rowLabels[index]->SetColor(bSelected ? ME::Color::Yellow() : ME::Color::White());
}

void Samples::SceneUIGraphicsStress::SetStressPanelCount(uint32_t count) {
    for (uint32_t i = 0; i < MAX_STRESS_PANELS; ++i) {
        stressPanels[i]->SetVisible(i < count);
    }
}

void Samples::SceneUIGraphicsStress::SetStressLabelCount(uint32_t count) {
    for (uint32_t i = 0; i < MAX_STRESS_LABELS; ++i) {
        stressLabels[i]->SetVisible(i < count);
    }
}

void Samples::SceneUIGraphicsStress::BuildStressWidgets() {
    // Fixed seed: the same layout, and so the same load, on every run.
    ME::Random random{1717u};

    for (uint32_t i = 0; i < MAX_STRESS_PANELS; ++i) {
        const ME::Color tint(Random01(random), Random01(random), Random01(random), 0.35f);
        ME::Panel* panel = new ME::Panel(0, 0, 0, 0, BUTTON_ATLAS_TILE, tint);
        panel->SetAnchor(ME::UIAnchor::TopLeft);
        panel->SetPivot(ME::UIPivot::TopLeft);
        panel->SetOffset(
            ME::Vec2{Random01(random) * STRESS_AREA_WIDTH, STRESS_AREA_TOP + Random01(random) * STRESS_AREA_HEIGHT});
        panel->SetSize(ME::Vec2{STRESS_PANEL_SIZE, STRESS_PANEL_SIZE});
        panel->Init();
        panel->SetVisible(false);
        AddUIElement(panel);
        stressPanels[i] = panel;
    }

    for (uint32_t i = 0; i < MAX_STRESS_LABELS; ++i) {
        const ME::Color tint(0.5f + 0.5f * Random01(random), 0.5f + 0.5f * Random01(random),
                             0.5f + 0.5f * Random01(random), 1.0f);
        ME::Label* label = new ME::Label(STRESS_TEXT, 0, 1, 0, tint, STRESS_GLYPH_SIZE, STRESS_GLYPH_SIZE, 0, 2, 40,
                                         ME::TextAlignment::Left);
        label->SetAnchor(ME::UIAnchor::TopLeft);
        label->SetPivot(ME::UIPivot::TopLeft);
        label->SetOffset(ME::Vec2{Random01(random) * STRESS_AREA_WIDTH * 0.5f,
                                  STRESS_AREA_TOP + Random01(random) * STRESS_AREA_HEIGHT});
        label->SetSize(ME::Vec2{320.0f, static_cast<float>(STRESS_GLYPH_SIZE) + 2.0f});
        label->Init();
        label->SetVisible(false);
        AddUIElement(label);
        stressLabels[i] = label;
    }
}

void Samples::SceneUIGraphicsStress::BuildStepButton(StepButton& out, const char* text, ME::UIAnchor anchor,
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

    // A finger stays "hovering" after it lifts, so hover keeps the normal colour.
    out.button->SetHoverColor(ME::Color::White());
    out.button->SetPanel(out.panel);
}
