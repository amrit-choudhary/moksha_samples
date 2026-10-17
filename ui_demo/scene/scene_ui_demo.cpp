#include "scene_ui_demo.h"

#include "moksha/client/utils/json_utils.h"

Samples::SceneUIDemo::SceneUIDemo() {}

Samples::SceneUIDemo::~SceneUIDemo() {
    delete scoreButtonLabel;
    delete scoreButtonPanel;
    delete scoreButton;
    delete healthLabel;
    delete scoreLabel;
    delete centerImage;
    delete titleLabel;
    delete topBarPanel;
}

void Samples::SceneUIDemo::CreateResources() {
    SceneUI::CreateResources();

    spriteTexturePaths[0] = "textures/ui/ui_atlas.dds";
    spriteTexturePaths[1] = "textures/font/ascii_ibm_transparent.dds";
    spriteTexturePaths.count = 2;

    ME::JsonUtils::LoadTextureAtlasProps("texture_data/atlas_ui.json", textureAtlasProperties[0]);
    ME::JsonUtils::LoadTextureAtlasProps("texture_data/font_atlas_01.json", textureAtlasProperties[1]);
    textureAtlasProperties.count = 2;
}

void Samples::SceneUIDemo::BuildUIElements() {
    SceneUI::BuildUIElements();

    topBarPanel = new ME::Panel(0, 0, 0, 0, 48);
    topBarPanel->SetAnchor(ME::UIAnchor::TopCenter);
    topBarPanel->SetPivot(ME::UIPivot::TopCenter);
    topBarPanel->SetOffset(ME::Vec2{0.0f, 10.0f});
    topBarPanel->SetSize(ME::Vec2{600.0f, 60.0f});
    topBarPanel->Init();
    AddUIElement(topBarPanel);

    titleLabel = new ME::Label("UI Test", 0, 1, 0, ME::Color::White(), 32, 32, 2, 2, 80, ME::TextAlignment::Center);
    titleLabel->SetAnchor(ME::UIAnchor::Center);
    titleLabel->SetPivot(ME::UIPivot::Center);
    titleLabel->SetOffset(ME::Vec2{0.0f, 0.0f});
    titleLabel->SetSize(ME::Vec2{300.0f, 40.0f});
    titleLabel->SetParent(topBarPanel);
    titleLabel->Init();
    AddUIElement(titleLabel);

    centerImage = new ME::Image(0, 0, 0, 0, 399);
    centerImage->SetAnchor(ME::UIAnchor::Center);
    centerImage->SetPivot(ME::UIPivot::Center);
    centerImage->SetOffset(ME::Vec2{0.0f, 0.0f});
    centerImage->SetSize(ME::Vec2{300.0f, 150.0f});
    centerImage->Init();
    AddUIElement(centerImage);

    healthLabel =
        new ME::Label("Health: 100", 0, 1, 0, ME::Color::Yellow(), 24, 24, 0, 2, 80, ME::TextAlignment::Right);
    healthLabel->SetAnchor(ME::UIAnchor::BottomRight);
    healthLabel->SetPivot(ME::UIPivot::BottomRight);
    healthLabel->SetOffset(ME::Vec2{-20.0f, -20.0f});
    healthLabel->SetSize(ME::Vec2{260.0f, 40.0f});
    healthLabel->Init();
    AddUIElement(healthLabel);

    scoreLabel = new ME::Label("Score: 0", 0, 1, 0, ME::Color::Green(), 24, 24, 0, 2, 80, ME::TextAlignment::Left);
    scoreLabel->SetAnchor(ME::UIAnchor::BottomLeft);
    scoreLabel->SetPivot(ME::UIPivot::BottomLeft);
    scoreLabel->SetOffset(ME::Vec2{20.0f, -20.0f});
    scoreLabel->SetSize(ME::Vec2{260.0f, 40.0f});
    scoreLabel->Init();
    AddUIElement(scoreLabel);

    scoreButton = new ME::Button();
    scoreButton->SetAnchor(ME::UIAnchor::BottomCenter);
    scoreButton->SetPivot(ME::UIPivot::BottomCenter);
    scoreButton->SetOffset(ME::Vec2{0.0f, -20.0f});
    scoreButton->SetSize(ME::Vec2{200.0f, 50.0f});
    scoreButton->Init();
    AddUIElement(scoreButton);

    scoreButtonPanel = new ME::Panel(0, 0, 0, 0, 660);
    scoreButtonPanel->SetAnchor(ME::UIAnchor::Center);
    scoreButtonPanel->SetPivot(ME::UIPivot::Center);
    scoreButtonPanel->SetSize(ME::Vec2{200.0f, 50.0f});
    scoreButtonPanel->SetParent(scoreButton);
    scoreButtonPanel->Init();
    AddUIElement(scoreButtonPanel);

    scoreButtonLabel =
        new ME::Label("Button", 0, 1, 0, ME::Color::Black(), 24, 24, 0, 2, 40, ME::TextAlignment::Center);
    scoreButtonLabel->SetAnchor(ME::UIAnchor::Center);
    scoreButtonLabel->SetPivot(ME::UIPivot::Center);
    scoreButtonLabel->SetSize(ME::Vec2{200.0f, 50.0f});
    scoreButtonLabel->SetParent(scoreButton);
    scoreButtonLabel->Init();
    AddUIElement(scoreButtonLabel);

    scoreButton->SetPanel(scoreButtonPanel);
}

ME::Label* Samples::SceneUIDemo::GetHealthLabel() const {
    return healthLabel;
}

ME::Label* Samples::SceneUIDemo::GetScoreLabel() const {
    return scoreLabel;
}

ME::Image* Samples::SceneUIDemo::GetCenterImage() const {
    return centerImage;
}

ME::Button* Samples::SceneUIDemo::GetScoreButton() const {
    return scoreButton;
}
