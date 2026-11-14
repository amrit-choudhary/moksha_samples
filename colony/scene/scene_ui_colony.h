/**
 * Colony UI: the frame rate at the top centre for now; the stats and load panels come later. Loads the engine's UI
 * atlas at slot 0 and font at slot 1, the slots the UI passes draw from.
 */

#pragma once

#include "moksha/client/scene/scene_ui.h"
#include "moksha/client/ui/label.h"

namespace Samples {

class SceneUIColony : public ME::SceneUI {
   public:
    SceneUIColony() = default;
    virtual ~SceneUIColony() override;

    virtual void CreateResources() override;
    virtual void BuildUIElements() override;

    void SetFps(double fps);

   private:
    ME::Label* fpsLabel = nullptr;
};

}  // namespace Samples
