/**
 * Particle Life UI: the frame rate at the top, the particle count and simulation time under it, and - / + buttons at
 * the bottom corners. Loads the engine's UI atlas at slot 0 and font at slot 1, the slots the UI passes draw from.
 */

#pragma once

#include <cstdint>

#include "moksha/client/scene/scene_ui.h"
#include "moksha/client/ui/button.h"
#include "moksha/client/ui/label.h"
#include "moksha/client/ui/panel.h"

namespace Samples {

class SceneUIParticleLife : public ME::SceneUI {
   public:
    SceneUIParticleLife() = default;
    virtual ~SceneUIParticleLife() override;

    virtual void CreateResources() override;
    virtual void BuildUIElements() override;

    void SetFps(double fps);

    // The particle count and the last step's simulation time.
    void SetInfo(uint32_t particleCount, double simMilliseconds);

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

    ME::Label* fpsLabel = nullptr;
    ME::Label* infoLabel = nullptr;
    StepButton downButton;
    StepButton upButton;
};

}  // namespace Samples
