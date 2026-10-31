/**
 * Rally UI: the score at the top and a centred message with a hint below it. Loads the engine's UI atlas at slot 0
 * and font at slot 1, the slots the UI passes draw from.
 */

#pragma once

#include <cstdint>

#include "moksha/client/scene/scene_ui.h"
#include "moksha/client/ui/label.h"

namespace Samples {

class SceneUIRally : public ME::SceneUI {
   public:
    SceneUIRally();
    virtual ~SceneUIRally() override;

    virtual void CreateResources() override;
    virtual void BuildUIElements() override;

    void SetScore(uint8_t ownScore, uint8_t otherScore);

    // An empty text hides the line (its old text stays, hidden).
    void SetMessage(const char* message);
    void SetHint(const char* hint);

   private:
    ME::Label* scoreLabel = nullptr;
    ME::Label* messageLabel = nullptr;
    ME::Label* hintLabel = nullptr;
};

}  // namespace Samples
