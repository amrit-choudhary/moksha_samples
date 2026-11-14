/**
 * Colony simulation and its schedule. ColonySim owns the world, the ants, the ant grid and the pending list; Step()
 * advances them by STEP_TIME by calling each system once over its full range, in the design's order: AntGrid, Sense,
 * Move, Claims, Mark, Fade, Colony, Apply (Claims before Mark, as Mark reads what Claims changes; Colony only queues
 * spawns and deaths, which Apply makes at the end). This is the only file threading will change.
 * Pure code, no engine systems: one ME::Random, seeded by Start, serves every system.
 */

#pragma once

#include <cstdint>

#include "colony/game/colony_ants.h"
#include "colony/game/colony_systems.h"
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

    // Seeds the random generator, removes every ant and spawns antCount searching followers in colony 0's nest at no
    // cost (none if the world has no colony); antCount becomes the target. Call after building the map into GetWorld().
    void Start(uint32_t antCount, uint32_t seed);

    // Advances the simulation by STEP_TIME. With invariant checks on, checks them after the step.
    void Step();

    // Checks the design's invariants: food conserved (cells + carried + stock + spent = the total at Start), food
    // per cell within MAX_FOOD, markers finite and in [0, 1], live ants inside the world and outside walls, and the
    // ant grid (rebuilt here) holding every live ant once. Returns null when all hold, else what failed.
    const char* CheckInvariants();

    // On by default in Debug builds. The first failure is kept, and checks stop after it.
    void SetCheckInvariants(bool bCheck) {
        bCheckInvariants = bCheck;
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

    // Food in the world at Start, which every later step must account for.
    uint64_t GetStartFood() const {
        return startFood;
    }

   private:
    ColonyWorld world;
    ColonyAnts ants;
    ColonyAntGrid antGrid;
    ColonyPending pending;
    ColonySettings settings;
    ME::Random random;
    uint32_t stepIndex = 0;
    float fadeDecay = 1.0f;  // per step
    uint64_t startFood = 0;

#ifdef NDEBUG
    bool bCheckInvariants = false;
#else
    bool bCheckInvariants = true;
#endif
    const char* invariantFailure = nullptr;
    uint8_t* seen = nullptr;  // per ant slot, for the ant grid check
};

}  // namespace Samples
