/**
 * Evolution Game.
 */

#pragma once

#include "evolution/game/physics_scene_evolution.h"
#include "evolution/scene/scene_evolution.h"
#include "evolution/scene/scene_ui_evolution.h"
#include "moksha/client/game/game.h"
#include "moksha/shared/datastructure/grid.h"

namespace Samples {

class GameEvolution : public ME::Game {
   public:
    GameEvolution();
    virtual ~GameEvolution();

    // Game control functions.

    // Init game
    virtual void Init(ME::Engine& inEngine) override;

    // Start game. This is called after all systems are initialized and before Update.
    virtual void Start() override;

    // Update game.
    virtual void Update(double deltaTime) override;

    // End game.
    virtual void End() override;

    virtual const char* GetDisplayName() const override;

   private:
    Samples::SceneEvolution* evoScene = nullptr;                // Scene for the game.
    Samples::SceneUIEvolution* evoUIScene = nullptr;            // UI scene for the game.
    Samples::PhysicsSceneEvolution* evoPhysicsScene = nullptr;  // Physics scene for the game.

    const float cameraSpeed = 100.0f;
};

}  // namespace Samples
