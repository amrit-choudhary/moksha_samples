#pragma once

/**
 * UI Subsystem Test/Demo Game.
 * Minimal Game subclass exercising the new UI subsystem end to end: a black background scene,
 * a top bar with the game name, a centered rectangular image, and health/score labels that
 * update from keyboard input. The actual widget tree is built by SceneUIDemo::BuildUIElements(),
 * not here — this class only owns gameplay state (health/score/image offset) and pushes it into
 * the widgets SceneUIDemo exposes. WASD also drives the center image's on-screen position, on
 * top of (not instead of) the existing health/score behavior.
 */

#include "moksha/client/game/game.h"
#include "moksha/shared/math/vec2.h"
#include "ui_demo/scene/scene_ui_demo.h"

namespace Samples {

class GameUIDemo : public ME::Game {
   public:
    GameUIDemo();
    ~GameUIDemo();

    virtual void Init(ME::Engine& inEngine) override;
    virtual void Start() override;
    virtual void Update(double deltaTime) override;
    virtual void End() override;

    void OnScoreButtonClick();

   private:
    void UpdateScoreLabel();

    // Same object as the base Game::uiScene, kept here with its concrete type so Update() can
    // reach SceneUIDemo::GetHealthLabel()/GetScoreLabel() without a cast.
    SceneUIDemo* sceneUIDemo = nullptr;

    int health = 100;
    int score = 0;

    ME::Vec2 imageOffset = ME::Vec2::Zero;
};

}  // namespace Samples
