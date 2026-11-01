/**
 * Graphics Stress UI: the frame rate at the top, the list of loads and their buttons at the bottom, and stress widgets
 * between them: panels for the UI sprite pass, labels for the text pass. The UI tree is laid out and copied every
 * frame. Loads the engine's UI atlas at slot 0 and font at slot 1, the slots the UI passes draw from.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "moksha/client/scene/scene_ui.h"
#include "moksha/client/ui/button.h"
#include "moksha/client/ui/label.h"
#include "moksha/client/ui/panel.h"

namespace Samples {

class SceneUIGraphicsStress : public ME::SceneUI {
   public:
    static constexpr size_t ROW_COUNT = 6;

    // Together with the 20 elements of the controls, just under the engine's 512 UI elements.
    static constexpr uint32_t MAX_STRESS_PANELS = 240;
    static constexpr uint32_t MAX_STRESS_LABELS = 240;

    SceneUIGraphicsStress();
    virtual ~SceneUIGraphicsStress() override;

    virtual void CreateResources() override;
    virtual void BuildUIElements() override;

    void SetFps(double fps);

    // Rewrites one row of the list of loads. The selected row stands out.
    void SetRow(size_t index, const char* text, bool bSelected);

    // Show the first count stress widgets; count is clamped to the maximum.
    void SetStressPanelCount(uint32_t count);
    void SetStressLabelCount(uint32_t count);

    ME::Button* GetPreviousButton() const {
        return previousButton.button;
    }

    ME::Button* GetNextButton() const {
        return nextButton.button;
    }

    ME::Button* GetDownButton() const {
        return downButton.button;
    }

    ME::Button* GetUpButton() const {
        return upButton.button;
    }

   private:
    // A button with its background and caption.
    class StepButton {
       public:
        ME::Button* button = nullptr;
        ME::Panel* panel = nullptr;
        ME::Label* label = nullptr;
    };

    // Builds a button on the bottom row, offsetX from the given corner.
    void BuildStepButton(StepButton& out, const char* text, ME::UIAnchor anchor, float offsetX);

    // Built first, so the controls draw over them.
    void BuildStressWidgets();

    ME::Panel* stressPanels[MAX_STRESS_PANELS] = {};
    ME::Label* stressLabels[MAX_STRESS_LABELS] = {};
    ME::Panel* controlsPanel = nullptr;
    ME::Label* fpsLabel = nullptr;
    ME::Label* rowLabels[ROW_COUNT] = {};
    StepButton previousButton;
    StepButton nextButton;
    StepButton downButton;
    StepButton upButton;
};

}  // namespace Samples
