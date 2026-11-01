/**
 * Graphics Stress game: loads every render path while the camera rolls, until the frame rate drops. One load is
 * selected at a time and raised or lowered with buttons: stars and clouds (instanced sets and overdraw), moving stars
 * (the per-frame instance update), UI panels and text (the UI passes and their per-frame rebuild). 200 giant stars
 * always orbit as plain sprites, one draw call each. No physics, audio or network.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "graphics_stress/scene/scene_graphics_stress.h"
#include "graphics_stress/scene/scene_ui_graphics_stress.h"
#include "moksha/client/game/game.h"

namespace Samples {

class GameGraphicsStress : public ME::Game {
   public:
    virtual void Init(ME::Engine& inEngine) override;
    virtual void Update(double deltaTime) override;
    virtual void End() override;

    virtual const char* GetDisplayName() const override;

   private:
    // Button callbacks.
    void OnSelectPrevious();
    void OnSelectNext();
    void OnLoadDown();
    void OnLoadUp();

    // Sends load index's value to the scene or UI.
    void ApplyLoad(size_t index);

    // Rewrites every load's row, marking the selected one.
    void ShowLoads();

    static constexpr float ROLL_SPEED = 0.05f;  // radians per second

    SceneGraphicsStress* stressScene = nullptr;
    SceneUIGraphicsStress* stressUI = nullptr;

    uint32_t loadValues[SceneUIGraphicsStress::ROW_COUNT] = {};
    size_t selectedLoad = 0;
    double roll = 0.0;
    int32_t shownFps = -1;  // the rate on screen; the label is rewritten only when it changes
};

}  // namespace Samples
