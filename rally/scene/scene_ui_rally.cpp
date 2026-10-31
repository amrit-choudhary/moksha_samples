#include "rally/scene/scene_ui_rally.h"

#include <cstdio>

#include "moksha/client/utils/json_utils.h"

// Builds a centred label anchored to the window's top centre (offsetY from the top) or centre.
static ME::Label* MakeLabel(const char* text, const ME::Color& color, uint16_t glyphSize, ME::UIAnchor anchor,
                            float offsetY) {
    ME::Label* label = new ME::Label(text, 0, 1, 0, color, glyphSize, glyphSize, 0, 2, 30, ME::TextAlignment::Center);
    label->SetAnchor(anchor);
    label->SetPivot(ME::UIPivot::Center);
    label->SetOffset(ME::Vec2{0.0f, offsetY});
    label->SetSize(ME::Vec2{420.0f, static_cast<float>(glyphSize) + 8.0f});
    label->Init();
    return label;
}

Samples::SceneUIRally::SceneUIRally() {}

Samples::SceneUIRally::~SceneUIRally() {
    delete hintLabel;
    delete messageLabel;
    delete scoreLabel;
}

void Samples::SceneUIRally::CreateResources() {
    SceneUI::CreateResources();

    spriteTexturePaths[0] = "textures/ui/ui_atlas.dds";
    spriteTexturePaths[1] = "textures/font/ascii_ibm_transparent.dds";
    spriteTexturePaths.count = 2;

    ME::JsonUtils::LoadTextureAtlasProps("texture_data/atlas_ui.json", textureAtlasProperties[0]);
    ME::JsonUtils::LoadTextureAtlasProps("texture_data/font_atlas_01.json", textureAtlasProperties[1]);
    textureAtlasProperties.count = 2;
}

void Samples::SceneUIRally::BuildUIElements() {
    SceneUI::BuildUIElements();

    scoreLabel = MakeLabel("You 0 : 0 Them", ME::Color::White(), 24, ME::UIAnchor::TopCenter, 40.0f);
    AddUIElement(scoreLabel);

    messageLabel = MakeLabel("Connecting...", ME::Color::Yellow(), 28, ME::UIAnchor::Center, -60.0f);
    AddUIElement(messageLabel);

    hintLabel = MakeLabel(" ", ME::Color::Green(), 18, ME::UIAnchor::Center, 60.0f);
    AddUIElement(hintLabel);
}

void Samples::SceneUIRally::SetScore(uint8_t ownScore, uint8_t otherScore) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "You %u : %u Them", static_cast<unsigned>(ownScore),
             static_cast<unsigned>(otherScore));
    scoreLabel->SetText(buffer);
}

void Samples::SceneUIRally::SetMessage(const char* message) {
    const bool bVisible = message[0] != '\0';
    if (bVisible) {
        messageLabel->SetText(message);
    }
    messageLabel->SetVisible(bVisible);
}

void Samples::SceneUIRally::SetHint(const char* hint) {
    const bool bVisible = hint[0] != '\0';
    if (bVisible) {
        hintLabel->SetText(hint);
    }
    hintLabel->SetVisible(bVisible);
}
