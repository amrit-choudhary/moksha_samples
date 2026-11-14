#include "colony/game/colony_step.h"

#include <cmath>
#include <cstring>

namespace {

constexpr float PI = 3.14159265f;

}  // namespace

Samples::ColonySim::~ColonySim() {
    delete[] seen;
}

bool Samples::ColonySim::Init() {
    if (!world.Init()) {
        return false;
    }
    ants.Init();
    antGrid.Init(ColonyAnts::MAX_ANTS);
    seen = new uint8_t[ColonyAnts::MAX_ANTS];
    fadeDecay = std::exp(-STEP_TIME / ColonyTuning::MARKER_LIFETIME);
    return true;
}

void Samples::ColonySim::Start(uint32_t antCount, uint32_t seed) {
    random = ME::Random(seed);
    ants.Clear();
    pending.Clear();
    stepIndex = 0;
    settings.antTarget = antCount;
    invariantFailure = nullptr;

    startFood = world.CountFood();
    for (uint32_t colony = 0; colony < world.GetColonyCount(); ++colony) {
        const ColonyNest& nest = world.GetNest(colony);
        startFood += nest.stock + nest.foodSpent;
    }
    if (world.GetColonyCount() == 0) {
        return;
    }

    // The starting population, all at once: spread over the inner half of the nest, facing every way.
    const ColonyNest& nest = world.GetNest(0);
    for (uint32_t i = 0; i < antCount; ++i) {
        const float angle = static_cast<float>(random.NextDouble()) * 2.0f * PI;
        const float distance = static_cast<float>(random.NextDouble()) * 0.5f * nest.radius;
        const float heading = static_cast<float>(random.NextDouble()) * 2.0f * PI - PI;
        const float x = nest.x + std::cos(angle) * distance;
        const float y = nest.y + std::sin(angle) * distance;
        if (ants.Spawn(0, x, y, heading, AntRole::Follower) == ColonyAnts::NO_SLOT) {
            break;
        }
    }
}

void Samples::ColonySim::Step() {
    const uint32_t antEnd = ants.GetSlotEnd();

    ColonySystems::UpdateAntGrid(ants, antGrid, 0, antEnd);
    ColonySystems::UpdateSensing(world, ants, settings, random, STEP_TIME, 0, antEnd);
    ColonySystems::UpdateMovement(world, ants, antGrid, settings, STEP_TIME, 0, antEnd);
    ColonySystems::UpdateClaims(world, ants, 0, antEnd);
    ColonySystems::UpdateMarkers(world, ants, stepIndex, 0, antEnd);
    ColonySystems::UpdateFade(world, fadeDecay, 0, ColonyWorld::CELL_COUNT);
    pending.Clear();
    ColonySystems::UpdateColonies(world, ants, settings, random, pending, 0, world.GetColonyCount());
    ColonySystems::ApplyPending(world, ants, pending, 0, pending.deathCount + pending.spawnCount);
    ColonySystems::UpdatePrevPositions(ants, 0, ants.GetSlotEnd());

    ++stepIndex;

    if (bCheckInvariants && invariantFailure == nullptr) {
        invariantFailure = CheckInvariants();
    }
}

const char* Samples::ColonySim::CheckInvariants() {
    // Food.
    const uint16_t* food = world.GetFood();
    uint64_t total = 0;
    for (uint32_t cell = 0; cell < ColonyWorld::CELL_COUNT; ++cell) {
        if (food[cell] > ColonyWorld::MAX_FOOD) {
            return "a cell holds more than MAX_FOOD (taken below 0?)";
        }
        total += food[cell];
    }
    for (uint32_t colony = 0; colony < world.GetColonyCount(); ++colony) {
        const ColonyNest& nest = world.GetNest(colony);
        total += nest.stock + nest.foodSpent;
    }
    const uint32_t antEnd = ants.GetSlotEnd();
    for (uint32_t slot = 0; slot < antEnd; ++slot) {
        if (ants.alive[slot] != 0) {
            total += ants.carrying[slot];
        }
    }
    if (total != startFood) {
        return "food is not conserved";
    }

    // Markers: finite and in [0, 1] (written as !(in range), so NaN fails too).
    for (uint32_t colony = 0; colony < ColonyWorld::MAX_COLONIES; ++colony) {
        const float* toHome = world.GetToHome(colony);
        const float* toFood = world.GetToFood(colony);
        for (uint32_t cell = 0; cell < ColonyWorld::CELL_COUNT; ++cell) {
            if (!(toHome[cell] >= 0.0f && toHome[cell] <= 1.0f) || !(toFood[cell] >= 0.0f && toFood[cell] <= 1.0f)) {
                return "a marker is negative, above 1 or NaN";
            }
        }
    }

    // Ants.
    uint32_t live = 0;
    for (uint32_t slot = 0; slot < antEnd; ++slot) {
        if (ants.alive[slot] == 0) {
            continue;
        }
        ++live;
        if (world.IsWall(ants.x[slot], ants.y[slot])) {
            return "an ant is outside the world or in a wall";
        }
    }
    if (live != ants.GetCount()) {
        return "the live ant count is wrong";
    }

    // The ant grid, rebuilt from the current positions: every live ant once, in its own cell.
    ColonySystems::UpdateAntGrid(ants, antGrid, 0, antEnd);
    if (antGrid.slotCount != live) {
        return "the ant grid doesn't hold every live ant";
    }
    std::memset(seen, 0, antEnd * sizeof(seen[0]));
    for (uint32_t cell = 0; cell < ColonyAntGrid::CELL_COUNT; ++cell) {
        for (uint32_t i = antGrid.cellStart[cell]; i < antGrid.cellStart[cell + 1]; ++i) {
            const uint32_t slot = antGrid.slots[i];
            if (slot >= antEnd || ants.alive[slot] == 0 || seen[slot] != 0 ||
                ColonyAntGrid::CellAt(ants.x[slot], ants.y[slot]) != cell) {
                return "the ant grid holds an ant twice, a dead slot, or an ant in the wrong cell";
            }
            seen[slot] = 1;
        }
    }
    return nullptr;
}
