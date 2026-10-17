/**
 * Evolution UI scene.
 */

#pragma once

#include "moksha/client/scene/scene_ui.h"

namespace Samples {

class SceneUIEvolution : public ME::SceneUI {
   public:
    SceneUIEvolution();
    virtual ~SceneUIEvolution() override;

    virtual void Init() override;
    virtual void CreateResources() override;
    virtual void BuildUISprites() override;
    virtual void BuildTextRenderers() override;
};

}  // namespace Samples
