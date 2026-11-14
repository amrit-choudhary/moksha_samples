/**
 * Colony UI: the frame rate at the top centre; the stats panel at the top right (rows of text the game writes, Tab
 * toggles it); the load panel at the bottom left (one row per setting, the selected one highlighted, and a help row);
 * and ^ v - + buttons at the bottom right that select a row and change it. Loads the engine's UI atlas at slot 0 and
 * font at slot 1, the slots the UI passes draw from.
 */

#pragma once

#include <cstdint>

#include "moksha/client/rendering/shared/color.h"
#include "moksha/client/scene/scene_ui.h"
#include "moksha/client/ui/button.h"
#include "moksha/client/ui/label.h"
#include "moksha/client/ui/panel.h"

namespace Samples {

class SceneUIColony : public ME::SceneUI {
   public:
    static constexpr uint32_t STATS_ROWS = 26;  // 6 heads, 14 timers, a note, requests, 4 colonies
    static constexpr uint32_t STATS_CHARS = 46;
    static constexpr uint32_t LOAD_ROWS = 9;
    static constexpr uint32_t LOAD_CHARS = 40;

    SceneUIColony() = default;
    virtual ~SceneUIColony() override;

    virtual void CreateResources() override;
    virtual void BuildUIElements() override;

    void SetFps(double fps);

    // Shows or hides the stats panel and its rows.
    void SetStatsVisible(bool bVisible);
    bool IsStatsVisible() const;

    // Rewrites a row of the stats panel (up to STATS_CHARS characters).
    void SetStatsRow(uint32_t row, const char* text, const ME::Color& color);

    // Rewrites a row of the load panel (up to LOAD_CHARS characters); the selected one stands out.
    void SetLoadRow(uint32_t row, const char* text, bool bSelected);

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

    // Builds a button on the bottom row, offsetX from the bottom right corner.
    void BuildStepButton(StepButton& out, const char* text, float offsetX);

    ME::Label* fpsLabel = nullptr;
    ME::Panel* statsPanel = nullptr;
    ME::Label* statsRows[STATS_ROWS] = {};
    ME::Panel* loadPanel = nullptr;
    ME::Label* loadRows[LOAD_ROWS] = {};
    StepButton previousButton;
    StepButton nextButton;
    StepButton downButton;
    StepButton upButton;
};

}  // namespace Samples
