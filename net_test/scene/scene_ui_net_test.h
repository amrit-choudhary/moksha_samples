#pragma once

/**
 * SceneUI for GameNetTest. Same fixed atlas/font texture indices as the UI demo's SceneUIDemo (see
 * ui_demo/scene/scene_ui_demo.h) - content comes from the dynamic UIElement tree built in
 * BuildUIElements() below.
 */

#include "moksha/client/scene/scene_ui.h"
#include "moksha/client/ui/button.h"
#include "moksha/client/ui/label.h"
#include "moksha/client/ui/panel.h"

namespace Samples {

class SceneUINetTest : public ME::SceneUI {
   public:
    SceneUINetTest();
    virtual ~SceneUINetTest() override;

    virtual void CreateResources() override;
    virtual void BuildUIElements() override;

    ME::Label* GetStatusLabel() const;
    ME::Label* GetPingLabel() const;
    ME::Label* GetYourScoreLabel() const;
    ME::Label* GetOpponentScoreLabel() const;
    ME::Label* GetHighScoreLabel() const;
    ME::Button* GetScoreButton() const;
    ME::Label* GetQuizQuestionLabel() const;

    // index is 0 or 1, matching QuizQuestion::options. Returns nullptr for any other index.
    ME::Button* GetAnswerButton(int index) const;
    ME::Label* GetAnswerLabel(int index) const;

    static constexpr int ANSWER_COUNT = 2;

   private:
    ME::Panel* topBarPanel = nullptr;
    ME::Label* titleLabel = nullptr;
    ME::Label* statusLabel = nullptr;
    ME::Label* pingLabel = nullptr;
    ME::Label* yourScoreLabel = nullptr;
    ME::Label* opponentScoreLabel = nullptr;
    ME::Label* highScoreLabel = nullptr;

    ME::Button* scoreButton = nullptr;
    ME::Panel* scoreButtonPanel = nullptr;
    ME::Label* scoreButtonLabel = nullptr;

    ME::Label* quizQuestionLabel = nullptr;
    ME::Button* answerButtons[ANSWER_COUNT] = {nullptr, nullptr};
    ME::Panel* answerButtonPanels[ANSWER_COUNT] = {nullptr, nullptr};
    ME::Label* answerButtonLabels[ANSWER_COUNT] = {nullptr, nullptr};
};

}  // namespace Samples
