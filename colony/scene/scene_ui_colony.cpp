#include "colony/scene/scene_ui_colony.h"

#include <cstdio>

#include "moksha/client/utils/json_utils.h"

// In points. Landscape: the frame rate at the top centre, clear of the debug overlay's rows at the top left.
static constexpr float FPS_OFFSET_Y = 16.0f;
static constexpr float MARGIN = 12.0f;

static constexpr uint16_t STATS_GLYPH = 10;
static constexpr float STATS_ROW_HEIGHT = 13.0f;
static constexpr float STATS_WIDTH = STATS_GLYPH * Samples::SceneUIColony::STATS_CHARS + 2.0f * MARGIN;

static constexpr uint16_t LOAD_GLYPH = 12;
static constexpr float LOAD_ROW_HEIGHT = 16.0f;
static constexpr float LOAD_WIDTH = LOAD_GLYPH * Samples::SceneUIColony::LOAD_CHARS + 2.0f * MARGIN;

static constexpr float BUTTON_WIDTH = 52.0f;
static constexpr float BUTTON_HEIGHT = 44.0f;
static constexpr float BUTTON_STRIDE = BUTTON_WIDTH + 8.0f;

static constexpr uint16_t BUTTON_ATLAS_TILE = 660;
static constexpr uint16_t PANEL_ATLAS_TILE = 48;
static constexpr ME::Color PANEL_COLOR{0.0f, 0.0f, 0.0f, 0.6f};
static constexpr float SMOOTH_FPS = 55.0f;  // at or above: green
static constexpr float SLOW_FPS = 28.0f;    // at or above: yellow; below: red

Samples::SceneUIColony::~SceneUIColony() {
    StepButton* const buttons[] = {&upButton, &downButton, &nextButton, &previousButton};
    for (StepButton* step : buttons) {
        delete step->label;
        delete step->panel;
        delete step->button;
    }
    for (ME::Label* row : loadRows) {
        delete row;
    }
    delete loadPanel;
    for (ME::Label* row : statsRows) {
        delete row;
    }
    delete statsPanel;
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

    // Stats: a dark panel at the top right with a row of text per line; the rows are its children, so hiding the
    // panel hides them.
    statsPanel = new ME::Panel(0, 0, 0, 0, PANEL_ATLAS_TILE, PANEL_COLOR);
    statsPanel->SetAnchor(ME::UIAnchor::TopRight);
    statsPanel->SetPivot(ME::UIPivot::TopRight);
    statsPanel->SetOffset(ME::Vec2{-MARGIN, MARGIN});
    statsPanel->SetSize(ME::Vec2{STATS_WIDTH, STATS_ROW_HEIGHT * STATS_ROWS + 2.0f * MARGIN});
    statsPanel->Init();
    AddUIElement(statsPanel);
    for (uint32_t i = 0; i < STATS_ROWS; ++i) {
        statsRows[i] = new ME::Label("", 0, 1, 0, ME::Color::White(), STATS_GLYPH, STATS_GLYPH, 0, 2, STATS_CHARS,
                                     ME::TextAlignment::Left);
        statsRows[i]->SetAnchor(ME::UIAnchor::TopLeft);
        statsRows[i]->SetPivot(ME::UIPivot::TopLeft);
        statsRows[i]->SetOffset(ME::Vec2{MARGIN, MARGIN + STATS_ROW_HEIGHT * static_cast<float>(i)});
        statsRows[i]->SetSize(ME::Vec2{STATS_WIDTH - 2.0f * MARGIN, STATS_ROW_HEIGHT});
        statsRows[i]->SetParent(statsPanel);
        statsRows[i]->Init();
        AddUIElement(statsRows[i]);
    }

    // Load settings: a dark panel at the bottom left, above nothing; rows from the top.
    loadPanel = new ME::Panel(0, 0, 0, 0, PANEL_ATLAS_TILE, PANEL_COLOR);
    loadPanel->SetAnchor(ME::UIAnchor::BottomLeft);
    loadPanel->SetPivot(ME::UIPivot::BottomLeft);
    loadPanel->SetOffset(ME::Vec2{MARGIN, -MARGIN});
    loadPanel->SetSize(ME::Vec2{LOAD_WIDTH, LOAD_ROW_HEIGHT * LOAD_ROWS + 2.0f * MARGIN});
    loadPanel->Init();
    AddUIElement(loadPanel);
    for (uint32_t i = 0; i < LOAD_ROWS; ++i) {
        loadRows[i] = new ME::Label("", 0, 1, 0, ME::Color::White(), LOAD_GLYPH, LOAD_GLYPH, 0, 2, LOAD_CHARS,
                                    ME::TextAlignment::Left);
        loadRows[i]->SetAnchor(ME::UIAnchor::TopLeft);
        loadRows[i]->SetPivot(ME::UIPivot::TopLeft);
        loadRows[i]->SetOffset(ME::Vec2{MARGIN, MARGIN + LOAD_ROW_HEIGHT * static_cast<float>(i)});
        loadRows[i]->SetSize(ME::Vec2{LOAD_WIDTH - 2.0f * MARGIN, LOAD_ROW_HEIGHT});
        loadRows[i]->SetParent(loadPanel);
        loadRows[i]->Init();
        AddUIElement(loadRows[i]);
    }

    BuildStepButton(previousButton, "^", -MARGIN - 3.0f * BUTTON_STRIDE);
    BuildStepButton(nextButton, "v", -MARGIN - 2.0f * BUTTON_STRIDE);
    BuildStepButton(downButton, "-", -MARGIN - BUTTON_STRIDE);
    BuildStepButton(upButton, "+", -MARGIN);
}

void Samples::SceneUIColony::SetFps(double fps) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.0f FPS  %.1f ms", fps, fps > 0.0 ? 1000.0 / fps : 0.0);
    fpsLabel->SetText(buffer);
    fpsLabel->SetColor(fps >= SMOOTH_FPS ? ME::Color::Green()
                       : fps >= SLOW_FPS ? ME::Color::Yellow()
                                         : ME::Color::Red());
}

void Samples::SceneUIColony::SetStatsVisible(bool bVisible) {
    statsPanel->SetVisible(bVisible);
}

bool Samples::SceneUIColony::IsStatsVisible() const {
    return statsPanel->IsVisible();
}

void Samples::SceneUIColony::SetStatsRow(uint32_t row, const char* text, const ME::Color& color) {
    if (row >= STATS_ROWS) {
        return;
    }
    statsRows[row]->SetText(text);
    statsRows[row]->SetColor(color);
}

void Samples::SceneUIColony::SetLoadRow(uint32_t row, const char* text, bool bSelected) {
    if (row >= LOAD_ROWS) {
        return;
    }
    loadRows[row]->SetText(text);
    loadRows[row]->SetColor(bSelected ? ME::Color::Yellow() : ME::Color::White());
}

void Samples::SceneUIColony::BuildStepButton(StepButton& out, const char* text, float offsetX) {
    out.button = new ME::Button();
    out.button->SetAnchor(ME::UIAnchor::BottomRight);
    out.button->SetPivot(ME::UIPivot::BottomRight);
    out.button->SetOffset(ME::Vec2{offsetX, -MARGIN});
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

    out.label = new ME::Label(text, 0, 1, 0, ME::Color::Black(), 28, 28, 0, 2, 4, ME::TextAlignment::Center);
    out.label->SetAnchor(ME::UIAnchor::Center);
    out.label->SetPivot(ME::UIPivot::Center);
    out.label->SetSize(ME::Vec2{BUTTON_WIDTH, BUTTON_HEIGHT});
    out.label->SetParent(out.button);
    out.label->Init();
    AddUIElement(out.label);

    // Slightly see-through over the world. A finger stays "hovering" after it lifts, so hover keeps the normal colour.
    const ME::Color normal(1.0f, 1.0f, 1.0f, 0.75f);
    out.button->SetNormalColor(normal);
    out.button->SetHoverColor(normal);
    out.button->SetPanel(out.panel);
}
