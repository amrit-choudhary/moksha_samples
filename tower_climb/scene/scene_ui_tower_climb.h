/**
 * Tower Climb UI: the height climbed at the top, and a centred message block for the ready and game-over screens.
 * Loads the engine's UI atlas at slot 0 and font at slot 1, the slots the UI passes draw from.
 */

#pragma once

#include <cstdint>

#include "moksha/client/scene/scene_ui.h"
#include "moksha/client/ui/label.h"

namespace Samples {

class SceneUITowerClimb : public ME::SceneUI {
   public:
    SceneUITowerClimb();
    virtual ~SceneUITowerClimb() override;

    virtual void CreateResources() override;
    virtual void BuildUIElements() override;

    // Title screen before the first climb.
    void ShowReady(uint32_t bestMeters);

    // Hides the message block while climbing.
    void ShowPlaying();

    // Game-over screen for a finished climb.
    void ShowGameOver(uint32_t meters, uint32_t bestMeters, bool bNewBest);

    void SetHeight(uint32_t meters);

   private:
    void SetMessageVisible(bool bVisible);

    ME::Label* heightLabel = nullptr;
    ME::Label* titleLabel = nullptr;
    ME::Label* bestLabel = nullptr;
    ME::Label* hintLabel = nullptr;
};

}  // namespace Samples
