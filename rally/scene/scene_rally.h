/**
 * Rally scene: plain sprites tinted from one white tile. The player's own paddle is always at the bottom, the
 * opponent's at the top; the game places them and the ball each frame in view coordinates (the court, turned so
 * the viewer defends the bottom).
 */

#pragma once

#include <cstddef>

#include "moksha/client/scene/scene.h"
#include "moksha/shared/math/transform.h"

namespace Samples {

class SceneRally : public ME::Scene {
   public:
    virtual void CreateResources() override;
    virtual void BuildSpriteTransforms() override;
    virtual void BuildSpriteRenderers() override;

    virtual const char* GetDisplayName() const override;

    // Sets the camera's aspect ratio and zooms out until the whole court fits in the window.
    void FitCamera(float aspectRatio);

    // Places the paddles and the ball; a hidden ball is parked off-screen.
    void Place(float ownPaddleX, float otherPaddleX, float ballX, float ballY, bool bBallVisible);

    // World x of the window's horizontal position, from 0 (left edge) to 1 (right edge).
    float ScreenToWorldX(float normalizedX) const;

   private:
    static constexpr size_t OWN_PADDLE = 0;
    static constexpr size_t OTHER_PADDLE = 1;
    static constexpr size_t BALL = 2;
    static constexpr size_t LEFT_WALL = 3;
    static constexpr size_t RIGHT_WALL = 4;
    static constexpr size_t FIRST_DASH = 5;
    static constexpr size_t DASH_COUNT = 9;
    static constexpr size_t SPRITE_COUNT = FIRST_DASH + DASH_COUNT;

    static constexpr float WALL_THICKNESS = 2.0f;
    static constexpr float VIEW_MARGIN = 4.0f;  // world units kept clear around the court
};

}  // namespace Samples
