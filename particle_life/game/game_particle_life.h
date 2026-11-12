/**
 * Particle Life game: a CPU load test that looks good. Each frame runs one simulation step on the main thread and
 * copies every particle into its sprite; - and + step the particle count through COUNTS, respawning the world, so
 * the frame rate can be compared across machines. The rules change every RULE_PERIOD seconds (R changes them now).
 * The first rules and every spawn come from fixed seeds, so each run starts the same. No physics, audio or network.
 * Headless, it is a CPU benchmark instead: BENCH_STEPS fixed steps at every count, logging the average simulation time
 * and a checksum of the result (the same on every machine with the same compiler and floating-point behaviour), then it
 * quits.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "moksha/client/game/game.h"
#include "particle_life/game/particle_life_sim.h"
#include "particle_life/scene/scene_particle_life.h"
#include "particle_life/scene/scene_ui_particle_life.h"

namespace Samples {

class GameParticleLife : public ME::Game {
   public:
    virtual void Init(ME::Engine& inEngine) override;
    virtual void Update(double deltaTime) override;
    virtual void End() override;

    virtual const char* GetDisplayName() const override;

   private:
    // Button callbacks.
    void OnCountDown();
    void OnCountUp();

    // Respawns the world with COUNTS[countIndex] particles.
    void ApplyCount();

    // Switches to the next rule table.
    void NextRules();

    // One fixed benchmark step; logs and moves to the next count every BENCH_STEPS, and quits after the last.
    void Benchmark();

    static constexpr double RULE_PERIOD = 45.0;     // seconds
    static constexpr double INFO_PERIOD = 0.25;     // seconds between updates of the simulation time on screen
    static constexpr float SIM_SPEED = 1.0f;        // simulated seconds per real second
    static constexpr size_t START_COUNT_INDEX = 3;  // into COUNTS
    static constexpr uint32_t BENCH_STEPS = 120;

    SceneParticleLife* lifeScene = nullptr;
    SceneUIParticleLife* lifeUI = nullptr;
    ParticleLifeSim sim;

    bool bBenchmark = false;
    size_t countIndex = START_COUNT_INDEX;
    uint32_t ruleSeed = 1;
    double ruleTime = 0.0;

    // The simulation time shown is the average over the last INFO_PERIOD.
    double infoTime = 0.0;
    double simTimeSum = 0.0;
    uint32_t simSteps = 0;
    int32_t shownFps = -1;  // the rate on screen; the label is rewritten only when it changes
};

}  // namespace Samples
