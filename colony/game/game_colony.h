/**
 * Colony game: an ant colony in Simulation mode, the test bed for the engine's multithreading (colony_design.md).
 * The simulation runs a fixed number of 1/60 s steps per frame in Update, never in FixedUpdate, so the load is what
 * the settings say and a slow frame shows as a lower frame rate. - and + step the ant target through the design's
 * ladder (the colony spawns or removes ants toward it); C toggles collisions; the scroll wheel zooms about the
 * pointer (up in, down out) and WASD pans; Escape quits.
 * Debug builds check the invariants after every step and log the first failure.
 * Headless, it is the benchmark: it runs a fixed list of scenarios and load settings, logs, and quits.
 */

#pragma once

#include <cstdint>

#include "colony/game/colony_scenario.h"
#include "colony/game/colony_step.h"
#include "colony/scene/scene_colony.h"
#include "colony/scene/scene_ui_colony.h"
#include "moksha/client/game/game.h"

namespace Samples {

class GameColony : public ME::Game {
   public:
    virtual void Init(ME::Engine& inEngine) override;
    virtual void Update(double deltaTime) override;
    virtual void End() override;

    virtual const char* GetDisplayName() const override;

   private:
    // Loads a scenario from resources, builds its map and spawns its ants. On failure the world is an empty map.
    void StartScenario(const char* path);

    // Moves the ant target one rung down or up the ladder.
    void StepAntTarget(int32_t direction);

    // Simulated steps of ColonySim::STEP_TIME per frame.
    static constexpr uint32_t STEPS_PER_FRAME = 1;

    // Camera panning with WASD, in view heights per second.
    static constexpr float PAN_SPEED = 0.8f;

    SceneColony* colonyScene = nullptr;
    SceneUIColony* colonyUI = nullptr;

    ColonySim sim;
    ColonyScenario scenario;

    bool bBenchmark = false;
    bool bInvariantLogged = false;
    int32_t shownFps = -1;  // the rate on screen; the label is rewritten only when it changes
};

}  // namespace Samples
