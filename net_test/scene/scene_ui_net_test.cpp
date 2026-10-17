#include "scene_ui_net_test.h"

#include "moksha/client/utils/json_utils.h"

Samples::SceneUINetTest::SceneUINetTest() {}

Samples::SceneUINetTest::~SceneUINetTest() {
    for (int i = 0; i < ANSWER_COUNT; ++i) {
        delete answerButtonLabels[i];
        delete answerButtonPanels[i];
        delete answerButtons[i];
    }
    delete quizQuestionLabel;
    delete scoreButtonLabel;
    delete scoreButtonPanel;
    delete scoreButton;
    delete highScoreLabel;
    delete opponentScoreLabel;
    delete yourScoreLabel;
    delete pingLabel;
    delete statusLabel;
    delete titleLabel;
    delete topBarPanel;
}

void Samples::SceneUINetTest::CreateResources() {
    SceneUI::CreateResources();

    spriteTexturePaths[0] = "textures/ui/ui_atlas.dds";
    spriteTexturePaths[1] = "textures/font/ascii_ibm_transparent.dds";
    spriteTexturePaths.count = 2;

    ME::JsonUtils::LoadTextureAtlasProps("texture_data/atlas_ui.json", textureAtlasProperties[0]);
    ME::JsonUtils::LoadTextureAtlasProps("texture_data/font_atlas_01.json", textureAtlasProperties[1]);
    textureAtlasProperties.count = 2;
}

void Samples::SceneUINetTest::BuildUIElements() {
    SceneUI::BuildUIElements();

    topBarPanel = new ME::Panel(0, 0, 0, 0, 48);
    topBarPanel->SetAnchor(ME::UIAnchor::TopCenter);
    topBarPanel->SetPivot(ME::UIPivot::TopCenter);
    topBarPanel->SetOffset(ME::Vec2{0.0f, 10.0f});
    topBarPanel->SetSize(ME::Vec2{600.0f, 60.0f});
    topBarPanel->Init();
    AddUIElement(topBarPanel);

    titleLabel =
        new ME::Label("Network Test", 0, 1, 0, ME::Color::White(), 32, 32, 2, 2, 80, ME::TextAlignment::Center);
    titleLabel->SetAnchor(ME::UIAnchor::Center);
    titleLabel->SetPivot(ME::UIPivot::Center);
    titleLabel->SetOffset(ME::Vec2{0.0f, 0.0f});
    titleLabel->SetSize(ME::Vec2{300.0f, 40.0f});
    titleLabel->SetParent(topBarPanel);
    titleLabel->Init();
    AddUIElement(titleLabel);

    statusLabel =
        new ME::Label("Connecting...", 0, 1, 0, ME::Color::Yellow(), 24, 24, 0, 2, 80, ME::TextAlignment::Center);
    statusLabel->SetAnchor(ME::UIAnchor::Center);
    statusLabel->SetPivot(ME::UIPivot::Center);
    statusLabel->SetOffset(ME::Vec2{0.0f, -60.0f});
    statusLabel->SetSize(ME::Vec2{400.0f, 40.0f});
    statusLabel->Init();
    AddUIElement(statusLabel);

    pingLabel =
        new ME::Label("Pings: 0 / Pongs: 0", 0, 1, 0, ME::Color::White(), 24, 24, 0, 2, 80, ME::TextAlignment::Center);
    pingLabel->SetAnchor(ME::UIAnchor::Center);
    pingLabel->SetPivot(ME::UIPivot::Center);
    pingLabel->SetOffset(ME::Vec2{0.0f, -10.0f});
    pingLabel->SetSize(ME::Vec2{400.0f, 40.0f});
    pingLabel->Init();
    AddUIElement(pingLabel);

    highScoreLabel =
        new ME::Label("Global High: -", 0, 1, 0, ME::Color::Green(), 24, 24, 0, 2, 80, ME::TextAlignment::Center);
    highScoreLabel->SetAnchor(ME::UIAnchor::Center);
    highScoreLabel->SetPivot(ME::UIPivot::Center);
    highScoreLabel->SetOffset(ME::Vec2{0.0f, 40.0f});
    highScoreLabel->SetSize(ME::Vec2{400.0f, 40.0f});
    highScoreLabel->Init();
    AddUIElement(highScoreLabel);

    yourScoreLabel =
        new ME::Label("Your Score: 0", 0, 1, 0, ME::Color::Green(), 24, 24, 0, 2, 80, ME::TextAlignment::Left);
    yourScoreLabel->SetAnchor(ME::UIAnchor::BottomLeft);
    yourScoreLabel->SetPivot(ME::UIPivot::BottomLeft);
    yourScoreLabel->SetOffset(ME::Vec2{20.0f, -20.0f});
    yourScoreLabel->SetSize(ME::Vec2{260.0f, 40.0f});
    yourScoreLabel->Init();
    AddUIElement(yourScoreLabel);

    opponentScoreLabel =
        new ME::Label("Opponent Score: -", 0, 1, 0, ME::Color::Yellow(), 24, 24, 0, 2, 80, ME::TextAlignment::Right);
    opponentScoreLabel->SetAnchor(ME::UIAnchor::BottomRight);
    opponentScoreLabel->SetPivot(ME::UIPivot::BottomRight);
    opponentScoreLabel->SetOffset(ME::Vec2{-20.0f, -20.0f});
    opponentScoreLabel->SetSize(ME::Vec2{260.0f, 40.0f});
    opponentScoreLabel->Init();
    AddUIElement(opponentScoreLabel);

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
        new ME::Label("Score +1", 0, 1, 0, ME::Color::Black(), 24, 24, 0, 2, 40, ME::TextAlignment::Center);
    scoreButtonLabel->SetAnchor(ME::UIAnchor::Center);
    scoreButtonLabel->SetPivot(ME::UIPivot::Center);
    scoreButtonLabel->SetSize(ME::Vec2{200.0f, 50.0f});
    scoreButtonLabel->SetParent(scoreButton);
    scoreButtonLabel->Init();
    AddUIElement(scoreButtonLabel);

    scoreButton->SetPanel(scoreButtonPanel);

    quizQuestionLabel = new ME::Label("Quiz: waiting for question...", 0, 1, 0, ME::Color::White(), 24, 24, 0, 2, 80,
                                      ME::TextAlignment::Center);
    quizQuestionLabel->SetAnchor(ME::UIAnchor::Center);
    quizQuestionLabel->SetPivot(ME::UIPivot::Center);
    quizQuestionLabel->SetOffset(ME::Vec2{0.0f, 100.0f});
    quizQuestionLabel->SetSize(ME::Vec2{500.0f, 40.0f});
    quizQuestionLabel->Init();
    AddUIElement(quizQuestionLabel);

    const float answerOffsetsX[ANSWER_COUNT] = {-110.0f, 110.0f};
    for (int i = 0; i < ANSWER_COUNT; ++i) {
        answerButtons[i] = new ME::Button();
        answerButtons[i]->SetAnchor(ME::UIAnchor::Center);
        answerButtons[i]->SetPivot(ME::UIPivot::Center);
        answerButtons[i]->SetOffset(ME::Vec2{answerOffsetsX[i], 160.0f});
        answerButtons[i]->SetSize(ME::Vec2{180.0f, 50.0f});
        answerButtons[i]->Init();
        AddUIElement(answerButtons[i]);

        answerButtonPanels[i] = new ME::Panel(0, 0, 0, 0, 660);
        answerButtonPanels[i]->SetAnchor(ME::UIAnchor::Center);
        answerButtonPanels[i]->SetPivot(ME::UIPivot::Center);
        answerButtonPanels[i]->SetSize(ME::Vec2{180.0f, 50.0f});
        answerButtonPanels[i]->SetParent(answerButtons[i]);
        answerButtonPanels[i]->Init();
        AddUIElement(answerButtonPanels[i]);

        answerButtonLabels[i] =
            new ME::Label("-", 0, 1, 0, ME::Color::Black(), 24, 24, 0, 2, 40, ME::TextAlignment::Center);
        answerButtonLabels[i]->SetAnchor(ME::UIAnchor::Center);
        answerButtonLabels[i]->SetPivot(ME::UIPivot::Center);
        answerButtonLabels[i]->SetSize(ME::Vec2{180.0f, 50.0f});
        answerButtonLabels[i]->SetParent(answerButtons[i]);
        answerButtonLabels[i]->Init();
        AddUIElement(answerButtonLabels[i]);

        answerButtons[i]->SetPanel(answerButtonPanels[i]);
        // Shown only while a question is open; children hide with their parent.
        answerButtons[i]->SetVisible(false);
    }
}

ME::Label* Samples::SceneUINetTest::GetStatusLabel() const {
    return statusLabel;
}

ME::Label* Samples::SceneUINetTest::GetPingLabel() const {
    return pingLabel;
}

ME::Label* Samples::SceneUINetTest::GetYourScoreLabel() const {
    return yourScoreLabel;
}

ME::Label* Samples::SceneUINetTest::GetOpponentScoreLabel() const {
    return opponentScoreLabel;
}

ME::Label* Samples::SceneUINetTest::GetHighScoreLabel() const {
    return highScoreLabel;
}

ME::Button* Samples::SceneUINetTest::GetScoreButton() const {
    return scoreButton;
}

ME::Label* Samples::SceneUINetTest::GetQuizQuestionLabel() const {
    return quizQuestionLabel;
}

ME::Button* Samples::SceneUINetTest::GetAnswerButton(int index) const {
    if (index < 0 || index >= ANSWER_COUNT) {
        return nullptr;
    }
    return answerButtons[index];
}

ME::Label* Samples::SceneUINetTest::GetAnswerLabel(int index) const {
    if (index < 0 || index >= ANSWER_COUNT) {
        return nullptr;
    }
    return answerButtonLabels[index];
}
