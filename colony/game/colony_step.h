/**
 * Colony simulation and its schedule. ColonySim owns the world, the ants, the ant grid and the pending list; Step()
 * advances them by STEP_TIME by calling each system once over its full range, in the design's order: AntGrid, Sense,
 * Move, Claims, Mark, Fade, Colony, Apply (Claims before Mark, as Mark reads what Claims changes; Colony only queues
 * spawns and deaths, which Apply makes at the end), timing each with std::chrono::steady_clock. This is the only file
 * threading will change.
 * Pure code, no engine systems: one ME::Random, seeded by Start, serves every system.
 */

#pragma once

#include <cstdint>

#include "colony/game/colony_ants.h"
#include "colony/game/colony_systems.h"
#include "colony/game/colony_timing.h"
#include "colony/game/colony_world.h"
#include "moksha/shared/random/random_engine.h"

namespace Samples {

class ColonySim {
   public:
    static constexpr float STEP_TIME = 1.0f / 60.0f;

    ColonySim() = default;
    ~ColonySim();

    ColonySim(const ColonySim&) = delete;
    ColonySim& operator=(const ColonySim&) = delete;

    // Allocates the world, ants and ant grid. False if called twice.
    bool Init();

    // Seeds the random generator, removes every ant and spawns antCount searching ants in colony 0's nest at no cost
    // (none if the world has no colony), explorers by the explorer share; antCount becomes the target. Call after
    // building the map into GetWorld().
    void Start(uint32_t antCount, uint32_t seed);

    // Sets the explorer share and draws every live ant's role again with it.
    void SetExplorerShare(float share);

    // Changes walls during a run (timed events): as ColonyWorld::SetWalls, then moves every ant caught in a new wall
    // to the nearest open cell (or removes it, its food going to its nest, if none is near). Food removed by the wall
    // leaves the conserved total.
    void SetWalls(float x0, float y0, float x1, float y1, bool bWall);

    // Adds food during a run (timed events), as ColonyWorld::AddFood; the conserved total grows by what was added.
    void AddFood(float x, float y, float radius, uint16_t amount);

    // Advances the simulation by STEP_TIME. With invariant checks on, checks them after the step (untimed).
    void Step();

    // The last step's time per system in ms, COLONY_STEP_TIMERS values in ColonyTimer order.
    const float* GetStepTimes() const {
        return stepMs;
    }

    // Checks the design's invariants: food conserved (cells + carried + stock + spent = the total at Start), food
    // per cell within MAX_FOOD, markers finite and in [0, 1], live ants inside the world and outside walls, and the
    // ant grid (rebuilt here) holding every live ant once. Returns null when all hold, else what failed.
    const char* CheckInvariants();

    // On by default in Debug builds. The first failure is kept, and checks stop after it.
    void SetCheckInvariants(bool bCheck) {
        bCheckInvariants = bCheck;
    }
    bool IsCheckingInvariants() const {
        return bCheckInvariants;
    }

    // The first invariant that failed during Step, or null.
    const char* GetInvariantFailure() const {
        return invariantFailure;
    }

    ColonyWorld& GetWorld() {
        return world;
    }
    const ColonyWorld& GetWorld() const {
        return world;
    }
    const ColonyAnts& GetAnts() const {
        return ants;
    }
    const ColonyAntGrid& GetAntGrid() const {
        return antGrid;
    }
    ColonySettings& GetSettings() {
        return settings;
    }

    // Steps since Start.
    uint32_t GetStepIndex() const {
        return stepIndex;
    }

    // The step at which the world's cells first had no food left (checked every HARVEST_CHECK_STEPS), or NO_STEP.
    uint32_t GetHarvestStep() const {
        return harvestStep;
    }

    static constexpr uint32_t NO_STEP = 0xFFFFFFFF;
    static constexpr uint32_t HARVEST_CHECK_STEPS = 60;

    // Food in the world at Start, which every later step must account for.
    uint64_t GetStartFood() const {
        return startFood;
    }

   private:
    // A role drawn with the explorer share.
    AntRole DrawRole();

    // Moves an ant in a wall to the nearest open cell within RELOCATE_CELLS; false if there is none.
    bool Relocate(uint32_t slot);

    static constexpr int32_t RELOCATE_CELLS = 24;

    ColonyWorld world;
    ColonyAnts ants;
    ColonyAntGrid antGrid;
    ColonyPending pending;
    ColonySettings settings;
    ME::Random random;
    uint32_t stepIndex = 0;
    float fadeDecay = 1.0f;  // per step
    uint64_t startFood = 0;
    uint32_t harvestStep = NO_STEP;
    float stepMs[COLONY_STEP_TIMERS] = {};

#ifdef NDEBUG
    bool bCheckInvariants = false;
#else
    bool bCheckInvariants = true;
#endif
    const char* invariantFailure = nullptr;
    uint8_t* seen = nullptr;  // per ant slot, for the ant grid check
};

}  // namespace Samples
