#include "colony/game/colony_step.h"

#include <chrono>
#include <cmath>
#include <cstring>

namespace {

constexpr float PI = 3.14159265f;

using Clock = std::chrono::steady_clock;

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
    requests.Init();
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
    harvestStep = NO_STEP;
    requests.Clear();
    requests.RequestWallDistance(world);

    startFood = world.CountFood();
    for (uint32_t colony = 0; colony < world.GetColonyCount(); ++colony) {
        const ColonyNest& nest = world.GetNest(colony);
        startFood += nest.stock + nest.foodSpent;
    }
    if (world.GetColonyCount() == 0) {
        return;
    }

    // The starting population, all at once, each colony its share: spread over the inner half of its nest, facing
    // every way. Soldiers are spread through the slots by the nest's soldier share (no random draw).
    for (uint32_t colony = 0; colony < world.GetColonyCount(); ++colony) {
        const ColonyNest& nest = world.GetNest(colony);
        const uint32_t share = settings.TargetOf(colony, world.GetColonyCount());
        for (uint32_t i = 0; i < share; ++i) {
            const bool bSoldier = static_cast<uint32_t>(static_cast<float>(i + 1) * nest.soldierShare) >
                                  static_cast<uint32_t>(static_cast<float>(i) * nest.soldierShare);
            const float angle = static_cast<float>(random.NextDouble()) * 2.0f * PI;
            const float distance = static_cast<float>(random.NextDouble()) * 0.5f * nest.radius;
            const float heading = static_cast<float>(random.NextDouble()) * 2.0f * PI - PI;
            const float x = nest.x + std::cos(angle) * distance;
            const float y = nest.y + std::sin(angle) * distance;
            const AntCaste caste = bSoldier ? AntCaste::Soldier : AntCaste::Worker;
            if (ants.Spawn(static_cast<uint8_t>(colony), x, y, heading, DrawRole(), caste) == ColonyAnts::NO_SLOT) {
                return;
            }
        }
    }
}

void Samples::ColonySim::Step() {
    const uint32_t antEnd = ants.GetSlotEnd();

    // Each system's wall-clock time, in ColonyTimer order.
    Clock::time_point last = Clock::now();
    auto lap = [&](ColonyTimer timer) {
        const Clock::time_point now = Clock::now();
        stepMs[static_cast<uint32_t>(timer)] = std::chrono::duration<float, std::milli>(now - last).count();
        last = now;
    };

    // Results finished since the last step (all of them, today), timed with the requests.
    requests.ApplyReady(world);
    lap(ColonyTimer::Requests);
    const float applyMs = stepMs[static_cast<uint32_t>(ColonyTimer::Requests)];

    ColonySystems::UpdateAntGrid(ants, antGrid, 0, antEnd);
    lap(ColonyTimer::AntGrid);
    ColonySystems::UpdateSensing(world, ants, antGrid, settings, random, STEP_TIME, 0, antEnd);
    lap(ColonyTimer::Sense);
    ColonySystems::UpdateMovement(world, ants, antGrid, settings, STEP_TIME, 0, antEnd);
    lap(ColonyTimer::Move);
    ColonySystems::UpdateClaims(world, ants, 0, antEnd);
    lap(ColonyTimer::Claims);
    if (world.GetColonyCount() > 1) {  // one colony has no enemies: skip the search, so v1 runs cost what they did
        ColonySystems::UpdateFights(ants, antGrid, stepIndex, STEP_TIME, 0, antEnd);
    }
    lap(ColonyTimer::Fight);
    ColonySystems::UpdateMarkers(world, ants, stepIndex, 0, antEnd);
    lap(ColonyTimer::Mark);
    ColonySystems::UpdateFade(world, fadeDecay, 0, ColonyWorld::CELL_COUNT);
    lap(ColonyTimer::Fade);
    pending.Clear();
    ColonySystems::CollectDeaths(world, ants, pending, 0, antEnd);
    ColonySystems::UpdateColonies(world, ants, settings, random, pending, 0, world.GetColonyCount());
    lap(ColonyTimer::Colony);
    ColonySystems::ApplyPending(world, ants, pending, 0, pending.deathCount + pending.spawnCount);
    lap(ColonyTimer::ApplySerial);
    ColonySystems::UpdatePrevPositions(ants, 0, ants.GetSlotEnd());
    lap(ColonyTimer::ApplyCopy);

    ++stepIndex;

    // AI think, staggered so the colonies' snapshots fall on different steps; then the queued work (inline today).
    if (IsAIOn()) {
        constexpr uint32_t STAGGER = ColonyAI::THINK_PERIOD / ColonyWorld::MAX_COLONIES;
        for (uint32_t colony = 0; colony < world.GetColonyCount(); ++colony) {
            if (stepIndex % ColonyAI::THINK_PERIOD == colony * STAGGER) {
                requests.RequestThink(world, ants, antGrid, colony);
            }
        }
    }
    requests.RunQueued();
    lap(ColonyTimer::Requests);
    stepMs[static_cast<uint32_t>(ColonyTimer::Requests)] += applyMs;

    if (harvestStep == NO_STEP && stepIndex % HARVEST_CHECK_STEPS == 0 && world.CountFood() == 0) {
        harvestStep = stepIndex;
    }
    if (bCheckInvariants && invariantFailure == nullptr) {
        invariantFailure = CheckInvariants();
    }
}

void Samples::ColonySim::SetExplorerShare(float share) {
    settings.explorerShare = share;
    const uint32_t antEnd = ants.GetSlotEnd();
    for (uint32_t slot = 0; slot < antEnd; ++slot) {
        if (ants.alive[slot] != 0) {
            ants.role[slot] = DrawRole();
        }
    }
}

void Samples::ColonySim::SetWalls(float x0, float y0, float x1, float y1, bool bWall) {
    const uint64_t foodBefore = world.CountFood();
    world.SetWalls(x0, y0, x1, y1, bWall);
    startFood -= foodBefore - world.CountFood();

    // The fields were searched over the old walls; until the new ones are applied, the old ones serve.
    requests.RequestWallDistance(world);
    for (uint32_t colony = 0; colony < world.GetColonyCount(); ++colony) {
        const ColonyNest& nest = world.GetNest(colony);
        if (nest.bRally) {
            requests.RequestPath(world, colony, nest.rallyX, nest.rallyY);
        }
    }
    if (!bWall) {
        return;
    }

    // Ants caught inside the new wall move out, or leave if no open cell is near; carried food stays accounted for.
    const uint32_t antEnd = ants.GetSlotEnd();
    for (uint32_t slot = 0; slot < antEnd; ++slot) {
        if (ants.alive[slot] == 0 || !world.IsWall(ants.x[slot], ants.y[slot]) || Relocate(slot)) {
            continue;
        }
        world.GetNest(ants.colony[slot]).stock += ants.carrying[slot];
        ants.Free(slot);
    }
}

void Samples::ColonySim::AddFood(float x, float y, float radius, uint16_t amount) {
    const uint64_t foodBefore = world.CountFood();
    world.AddFood(x, y, radius, amount);
    startFood += world.CountFood() - foodBefore;
}

Samples::AntRole Samples::ColonySim::DrawRole() {
    const float roll = static_cast<float>(random.NextDouble());
    return roll < settings.explorerShare ? AntRole::Explorer : AntRole::Follower;
}

bool Samples::ColonySim::Relocate(uint32_t slot) {
    // Rings of cells around the ant's, nearest first.
    const uint32_t home = ColonyWorld::CellAt(ants.x[slot], ants.y[slot]);
    const int32_t column = static_cast<int32_t>(home % ColonyWorld::GRID_WIDTH);
    const int32_t row = static_cast<int32_t>(home / ColonyWorld::GRID_WIDTH);
    for (int32_t ring = 1; ring <= RELOCATE_CELLS; ++ring) {
        for (int32_t dy = -ring; dy <= ring; ++dy) {
            for (int32_t dx = -ring; dx <= ring; ++dx) {
                if (dx != -ring && dx != ring && dy != -ring && dy != ring) {
                    continue;
                }
                const float x = (static_cast<float>(column + dx) + 0.5f) * ColonyWorld::CELL_SIZE;
                const float y = (static_cast<float>(row + dy) + 0.5f) * ColonyWorld::CELL_SIZE;
                if (world.IsWall(x, y)) {
                    continue;
                }
                ants.x[slot] = x;
                ants.y[slot] = y;
                ants.prevX[slot] = x;
                ants.prevY[slot] = y;
                return true;
            }
        }
    }
    return false;
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
        const float* const grids[] = {world.GetToHome(colony), world.GetToFood(colony), world.GetToEnemy(colony)};
        for (const float* markers : grids) {
            for (uint32_t cell = 0; cell < ColonyWorld::CELL_COUNT; ++cell) {
                if (!(markers[cell] >= 0.0f && markers[cell] <= 1.0f)) {
                    return "a marker is negative, above 1 or NaN";
                }
            }
        }
    }

    // Ants.
    uint32_t live = 0;
    uint32_t liveOf[ColonyWorld::MAX_COLONIES] = {};
    uint32_t soldiersOf[ColonyWorld::MAX_COLONIES] = {};
    for (uint32_t slot = 0; slot < antEnd; ++slot) {
        if (ants.alive[slot] == 0) {
            continue;
        }
        ++live;
        if (ants.colony[slot] >= world.GetColonyCount()) {
            return "an ant belongs to a colony that doesn't exist";
        }
        ++liveOf[ants.colony[slot]];
        if (ants.caste[slot] == AntCaste::Soldier) {
            ++soldiersOf[ants.colony[slot]];
        }
        if (!std::isfinite(ants.energy[slot])) {
            return "an ant's energy is not finite";
        }
        if (world.IsWall(ants.x[slot], ants.y[slot])) {
            return "an ant is outside the world or in a wall";
        }
    }
    if (live != ants.GetCount()) {
        return "the live ant count is wrong";
    }
    for (uint32_t colony = 0; colony < ColonyWorld::MAX_COLONIES; ++colony) {
        if (liveOf[colony] != ants.GetCountOf(colony) || soldiersOf[colony] != ants.GetSoldiersOf(colony)) {
            return "a colony's live ant or soldier count is wrong";
        }
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
