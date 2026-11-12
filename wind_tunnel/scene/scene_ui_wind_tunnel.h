/**
 * Wind tunnel UI: the frame rate at the top, the current settings and the lift and drag under it, and a row of buttons
 * at the bottom (for touch) with the keyboard keys above them. Loads the engine's UI atlas at slot 0 and font at slot 1,
 * the slots the UI passes draw from.
 */

#pragma once

#include <cstdint>

#include "moksha/client/scene/scene_ui.h"
#include "moksha/client/ui/button.h"
#include "moksha/client/ui/label.h"
#include "moksha/client/ui/panel.h"

namespace Samples {

class SceneUIWindTunnel : public ME::SceneUI {
   public:
    enum class ButtonId : uint8_t { Shape, Foil, View, Lines, Smoke, Slower, Faster, Count };

    SceneUIWindTunnel() = default;
    virtual ~SceneUIWindTunnel() override;

    virtual void CreateResources() override;
    virtual void BuildUIElements() override;

    void SetFps(double fps);
    void SetInfo(const char* text);
    void SetForces(const char* text);

    ME::Button* GetButton(ButtonId id) const {
        return buttons[static_cast<size_t>(id)].button;
    }

    // Whether a point in UI space (the mouse's own: top left origin, y down) is over a button.
    bool IsOverButton(float x, float y) const;

   private:
    // A button with its background and caption.
    class BarButton {
       public:
        ME::Button* button = nullptr;
        ME::Panel* panel = nullptr;
        ME::Label* label = nullptr;
    };

    ME::Label* fpsLabel = nullptr;
    ME::Label* infoLabel = nullptr;
    ME::Label* forcesLabel = nullptr;
    ME::Label* keysLabel = nullptr;
    BarButton buttons[static_cast<size_t>(ButtonId::Count)];
};

}  // namespace Samples
