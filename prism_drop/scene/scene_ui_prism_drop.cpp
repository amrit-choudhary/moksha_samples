#include "prism_drop/scene/scene_ui_prism_drop.h"

#include <cinttypes>
#include <cmath>
#include <cstdio>

#include "moksha/client/utils/json_utils.h"
#include "prism_drop/game/prism_palette.h"
#include "prism_drop/scene/scene_prism_drop.h"

using Samples::PrismPalette::WithAlpha;

static constexpr float LABEL_WIDTH = 440.0f;  // world units; text is centred in it
static constexpr ME::Color DIM{0.70f, 0.72f, 0.90f, 1.0f};

Samples::SceneUIPrismDrop::SceneUIPrismDrop(float inPixelsPerUnit) : pixelsPerUnit(inPixelsPerUnit) {}

Samples::SceneUIPrismDrop::~SceneUIPrismDrop() {
    for (PopupLine& line : popups) {
        delete line.label;
    }
    delete controlsLabel2;
    delete controlsLabel;
    delete hintLabel;
    delete infoLabel;
    delete subtitleLabel;
    delete titleLabel;
    delete statsLabel;
    delete nextCaption;
    delete holdCaption;
    delete scoreLabel;
    delete scoreCaption;
}

void Samples::SceneUIPrismDrop::CreateResources() {
    SceneUI::CreateResources();

    spriteTexturePaths[0] = "textures/ui/ui_atlas.dds";
    spriteTexturePaths[1] = "textures/font/ascii_ibm_transparent.dds";
    spriteTexturePaths.count = 2;

    ME::JsonUtils::LoadTextureAtlasProps("texture_data/atlas_ui.json", textureAtlasProperties[0]);
    ME::JsonUtils::LoadTextureAtlasProps("texture_data/font_atlas_01.json", textureAtlasProperties[1]);
    textureAtlasProperties.count = 2;
}

void Samples::SceneUIPrismDrop::BuildUIElements() {
    SceneUI::BuildUIElements();

    using S = ScenePrismDrop;
    const float boxTop = S::BOX_Y + S::BOX_SIZE * 0.5f;
    const float boardTop = S::BOARD_BOTTOM + S::BOARD_HEIGHT_UNITS;

    scoreCaption = MakeLabel("SCORE", DIM, 12, 0.0f, boxTop - 14.0f);
    scoreLabel = MakeLabel("0", ME::Color::White(), 22, 0.0f, boxTop - 44.0f);
    holdCaption = MakeLabel("HOLD", DIM, 12, -S::BOX_X, boxTop - 14.0f);
    nextCaption = MakeLabel("NEXT", DIM, 12, S::BOX_X, boxTop - 14.0f);
    statsLabel = MakeLabel("LEVEL 1   LINES 0", DIM, 14, 0.0f, S::BOARD_BOTTOM - S::WELL_BORDER - 18.0f);

    titleLabel = MakeLabel("PRISM DROP", PrismPalette::CYAN, 36, 0.0f, boardTop - 150.0f);
    subtitleLabel = MakeLabel(" ", PrismPalette::PINK, 16, 0.0f, boardTop - 200.0f);
    infoLabel = MakeLabel(" ", ME::Color::White(), 16, 0.0f, boardTop - 235.0f);
    hintLabel = MakeLabel("TAP TO START", PrismPalette::GOLD, 18, 0.0f, boardTop - 320.0f);
    controlsLabel = MakeLabel("Drag: move   Tap: rotate", DIM, 12, 0.0f, boardTop - 400.0f);
    controlsLabel2 = MakeLabel("Swipe down: drop   Swipe up: hold", DIM, 12, 0.0f, boardTop - 422.0f);

    for (size_t i = 0; i < POPUP_LINES; ++i) {
        popups[i].label = MakeLabel(" ", ME::Color::White(), i == 0 ? 26 : 16, 0.0f, 0.0f);
        popups[i].label->SetVisible(false);
    }
}

void Samples::SceneUIPrismDrop::Tick(float deltaTime) {
    blinkTime += deltaTime;
    if (bHintBlinks && hintLabel->IsVisible()) {
        const float pulse = 0.55f + 0.45f * std::sin(blinkTime * 4.0f);
        hintLabel->SetColor(WithAlpha(PrismPalette::GOLD, pulse));
    }

    for (PopupLine& line : popups) {
        if (!line.bActive) {
            continue;
        }
        line.age += deltaTime;
        const float t = (line.age - line.delay) / POPUP_SECONDS;
        if (t < 0.0f) {
            line.label->SetVisible(false);
            continue;
        }
        if (t >= 1.0f) {
            line.bActive = false;
            line.label->SetVisible(false);
            continue;
        }
        // Pops in quickly, holds, then rises and fades.
        const float rise = 1.0f - (1.0f - t) * (1.0f - t);
        const float alpha = t < 0.08f ? t / 0.08f : (t > 0.6f ? (1.0f - t) / 0.4f : 1.0f);
        const float flash = t < 0.15f ? 1.0f - t / 0.15f : 0.0f;
        line.label->SetVisible(true);
        line.label->SetColor(WithAlpha(PrismPalette::Lighten(line.color, flash), alpha));
        PlaceLabel(line.label, 0.0f, line.baseY + rise * POPUP_RISE);
    }
}

void Samples::SceneUIPrismDrop::ShowTitle(uint64_t best) {
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "BEST %" PRIu64, best);
    titleLabel->SetText("PRISM DROP");
    titleLabel->SetColor(PrismPalette::CYAN);
    subtitleLabel->SetText("a falling block puzzle");
    infoLabel->SetText(buffer);
    hintLabel->SetText("TAP TO START");
    bHintBlinks = true;
    SetMessageVisible(true);
    controlsLabel->SetVisible(true);
    controlsLabel2->SetVisible(true);
}

void Samples::SceneUIPrismDrop::ShowPlaying() {
    SetMessageVisible(false);
    controlsLabel->SetVisible(false);
    controlsLabel2->SetVisible(false);
}

void Samples::SceneUIPrismDrop::ShowPaused(bool bPaused) {
    if (!bPaused) {
        ShowPlaying();
        return;
    }
    titleLabel->SetText("PAUSED");
    titleLabel->SetColor(ME::Color::White());
    subtitleLabel->SetText("");
    infoLabel->SetText("");
    hintLabel->SetText("TAP TO RESUME");
    bHintBlinks = true;
    SetMessageVisible(true);
}

void Samples::SceneUIPrismDrop::ShowGameOver(uint64_t score, uint64_t best, bool bNewBest) {
    char buffer[48];
    titleLabel->SetText("GAME OVER");
    titleLabel->SetColor(PrismPalette::DANGER);
    snprintf(buffer, sizeof(buffer), "SCORE %" PRIu64, score);
    subtitleLabel->SetText(buffer);
    if (bNewBest) {
        infoLabel->SetText("NEW BEST!");
        infoLabel->SetColor(PrismPalette::GOLD);
    } else {
        snprintf(buffer, sizeof(buffer), "BEST %" PRIu64, best);
        infoLabel->SetText(buffer);
        infoLabel->SetColor(ME::Color::White());
    }
    hintLabel->SetText("TAP TO PLAY AGAIN");
    bHintBlinks = true;
    SetMessageVisible(true);
}

void Samples::SceneUIPrismDrop::SetScore(uint64_t score) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%" PRIu64, score);
    scoreLabel->SetText(buffer);
}

void Samples::SceneUIPrismDrop::SetStats(uint32_t level, uint32_t lines) {
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "LEVEL %u   LINES %u", level, lines);
    statsLabel->SetText(buffer);
}

void Samples::SceneUIPrismDrop::Popup(const char* headline, const char* detail, uint64_t points, const ME::Color& color,
                                      float worldY) {
    char buffer[32];
    StartLine(popups[0], headline, color, worldY, 0.0f);
    StartLine(popups[1], detail != nullptr ? detail : "", ME::Color::White(), worldY - 34.0f, 0.06f);
    if (points > 0) {
        snprintf(buffer, sizeof(buffer), "+%" PRIu64, points);
        StartLine(popups[2], buffer, PrismPalette::GOLD, worldY - 60.0f, 0.12f);
    } else {
        StartLine(popups[2], "", ME::Color::White(), worldY - 60.0f, 0.12f);
    }
}

ME::Label* Samples::SceneUIPrismDrop::MakeLabel(const char* text, const ME::Color& color, uint16_t glyph, float worldX,
                                                float worldY) {
    const uint16_t size = static_cast<uint16_t>(static_cast<float>(glyph) * pixelsPerUnit + 0.5f);
    ME::Label* label = new ME::Label(text, 0, 1, 0, color, size, size, 0, 2, 40, ME::TextAlignment::Center);
    label->SetAnchor(ME::UIAnchor::TopCenter);
    label->SetPivot(ME::UIPivot::Center);
    label->SetSize(ME::Vec2{LABEL_WIDTH * pixelsPerUnit, static_cast<float>(size) + 6.0f});
    PlaceLabel(label, worldX, worldY);
    label->Init();
    AddUIElement(label);
    return label;
}

void Samples::SceneUIPrismDrop::PlaceLabel(ME::Label* label, float worldX, float worldY) {
    label->SetOffset(ME::Vec2{worldX * pixelsPerUnit, (ScenePrismDrop::VIEW_HALF_HEIGHT - worldY) * pixelsPerUnit});
}

void Samples::SceneUIPrismDrop::StartLine(PopupLine& line, const char* text, const ME::Color& color, float worldY,
                                          float delay) {
    line.label->SetText(text);
    line.color = color;
    line.baseY = worldY;
    line.age = 0.0f;
    line.delay = delay;
    line.bActive = text[0] != '\0';
    line.label->SetVisible(false);
}

void Samples::SceneUIPrismDrop::SetMessageVisible(bool bVisible) {
    titleLabel->SetVisible(bVisible);
    subtitleLabel->SetVisible(bVisible);
    infoLabel->SetVisible(bVisible);
    hintLabel->SetVisible(bVisible);
}
