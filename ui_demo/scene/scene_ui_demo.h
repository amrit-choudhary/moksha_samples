#pragma once

/**
 * SceneUI for GameUIDemo. Loads the engine's UI atlas + font textures at the fixed indices the DX12
 * renderer hardcodes for UI drawing (spriteTextures[0]/textureAtlasProperties[0] for sprites,
 * [1]/[1] for text — see the engine's renderer_dx.cpp UI Sprite/Text Drawing sections). No BuildUISprites/
 * BuildTextRenderers override — content comes from the dynamic UIElement tree, built in
 * BuildUIElements() below rather than in GameUIDemo.
 */

#include "moksha/client/scene/scene_ui.h"
#include "moksha/client/ui/button.h"
#include "moksha/client/ui/image.h"
#include "moksha/client/ui/label.h"
#include "moksha/client/ui/panel.h"

namespace Samples {

class SceneUIDemo : public ME::SceneUI {
   public:
    SceneUIDemo();
    virtual ~SceneUIDemo() override;

    virtual void CreateResources() override;
    virtual void BuildUIElements() override;

    // GameUIDemo needs these to update from input each frame; topBarPanel/titleLabel are never
    // touched after creation.
    ME::Label* GetHealthLabel() const;
    ME::Label* GetScoreLabel() const;
    ME::Image* GetCenterImage() const;
    ME::Button* GetScoreButton() const;

   private:
    ME::Panel* topBarPanel = nullptr;
    ME::Label* titleLabel = nullptr;
    ME::Image* centerImage = nullptr;
    ME::Label* healthLabel = nullptr;
    ME::Label* scoreLabel = nullptr;

    ME::Button* scoreButton = nullptr;
    ME::Panel* scoreButtonPanel = nullptr;
    ME::Label* scoreButtonLabel = nullptr;
};

}  // namespace Samples
