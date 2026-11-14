/**
 * Tests for Colony's ants and systems: slots, the ant grid, sensing, claims, marking, fading, and a short run of the
 * whole step with population changes (the invariants hold).
 */

#include <cmath>

#include "colony/game/colony_ants.h"
#include "colony/game/colony_step.h"
#include "colony/game/colony_systems.h"
#include "colony/game/colony_world.h"
#include "moksha/shared/random/random_engine.h"
#include "moksha/test_framework/test_framework.h"

using Samples::AntRole;
using Samples::AntTask;
using Samples::ColonyAntGrid;
using Samples::ColonyAnts;
using Samples::ColonySettings;
using Samples::ColonySim;
using Samples::ColonyWorld;
namespace Systems = Samples::ColonySystems;

static constexpr float DT = 1.0f / 60.0f;

TEST(ColonyAnts, FreedSlotsAreReused) {
    ColonyAnts ants;
    ASSERT(ants.Init());
    EXPECT(ants.Spawn(0, 10.0f, 10.0f, 0.0f, AntRole::Follower) == 0);
    EXPECT(ants.Spawn(0, 20.0f, 20.0f, 0.0f, AntRole::Follower) == 1);
    ants.Free(0);
    ants.Free(0);  // already free: ignored
    EXPECT(ants.GetCount() == 1);
    EXPECT(ants.Spawn(0, 30.0f, 30.0f, 0.0f, AntRole::Explorer) == 0);
    EXPECT(ants.GetSlotEnd() == 2);
    EXPECT(ants.x[0] == 30.0f && ants.prevX[0] == 30.0f && ants.task[0] == AntTask::ToFood);
}

TEST(ColonyAntGrid, HoldsEveryLiveAntOnce) {
    ColonyAnts ants;
    ColonyAntGrid grid;
    ASSERT(ants.Init());
    ASSERT(grid.Init(ColonyAnts::MAX_ANTS));
    ME::Random random(5);
    for (int i = 0; i < 2'000; ++i) {
        const float x = static_cast<float>(random.NextDouble()) * ColonyWorld::WIDTH;
        const float y = static_cast<float>(random.NextDouble()) * ColonyWorld::HEIGHT;
        ants.Spawn(0, x, y, 0.0f, AntRole::Follower);
    }
    for (uint32_t slot = 0; slot < 2'000; slot += 7) {
        ants.Free(slot);
    }
    Systems::UpdateAntGrid(ants, grid, 0, ants.GetSlotEnd());
    EXPECT(grid.slotCount == ants.GetCount());

    static uint32_t seen[2'000];
    for (uint32_t& count : seen) {
        count = 0;
    }
    bool bInRightCell = true;
    for (uint32_t cell = 0; cell < ColonyAntGrid::CELL_COUNT; ++cell) {
        for (uint32_t i = grid.cellStart[cell]; i < grid.cellStart[cell + 1]; ++i) {
            const uint32_t slot = grid.slots[i];
            ++seen[slot];
            bInRightCell = bInRightCell && ColonyAntGrid::CellAt(ants.x[slot], ants.y[slot]) == cell;
        }
    }
    bool bOnce = true;
    for (uint32_t slot = 0; slot < 2'000; ++slot) {
        bOnce = bOnce && seen[slot] == (ants.alive[slot] != 0 ? 1u : 0u);
    }
    EXPECT(bOnce);
    EXPECT(bInRightCell);
}

TEST(ColonySystems, SensingTurnsTowardTheStrongestMarker) {
    ColonyWorld world;
    ColonyAnts ants;
    ASSERT(world.Init());
    ASSERT(ants.Init());
    world.AddColony(100.0f, 100.0f, 10.0f);
    // toFood everywhere: strong below y = 360, weak above. A searching ant facing +x should turn to face down.
    float* toFood = world.GetToFood(0);
    for (uint32_t cell = 0; cell < ColonyWorld::CELL_COUNT; ++cell) {
        toFood[cell] = cell / ColonyWorld::GRID_WIDTH < ColonyWorld::GRID_HEIGHT / 2 ? 0.8f : 0.2f;
    }
    const uint32_t slot = ants.Spawn(0, 640.0f, 361.0f, 0.0f, AntRole::Follower);
    ColonySettings settings;
    ME::Random random(11);
    for (int step = 0; step < 30; ++step) {
        Systems::UpdateSensing(world, ants, settings, random, DT, 0, ants.GetSlotEnd());
    }
    EXPECT(std::sin(ants.heading[slot]) < -0.3f);
}

// Two ants 1 unit apart, both walking up; returns their distance after one Move.
static float DistanceAfterOneMove(bool bCollisions) {
    ColonyWorld world;
    ColonyAnts ants;
    ColonyAntGrid grid;
    world.Init();
    ants.Init();
    grid.Init(ColonyAnts::MAX_ANTS);
    const float up = 1.5707963f;
    ants.Spawn(0, 400.0f, 400.0f, up, AntRole::Follower);
    ants.Spawn(0, 401.0f, 400.0f, up, AntRole::Follower);
    ColonySettings settings;
    settings.bCollisions = bCollisions;
    Systems::UpdateAntGrid(ants, grid, 0, ants.GetSlotEnd());
    Systems::UpdateMovement(world, ants, grid, settings, DT, 0, ants.GetSlotEnd());
    const float dx = ants.x[1] - ants.x[0];
    const float dy = ants.y[1] - ants.y[0];
    return std::sqrt(dx * dx + dy * dy);
}

TEST(ColonySystems, CollisionsPushOverlappingAntsApart) {
    // Each moves first, then pushes itself half its overlap with the other's previous position, so they part to
    // about 1.5 units in one step, not the full collision distance.
    EXPECT(DistanceAfterOneMove(true) > 1.4f);
    EXPECT(DistanceAfterOneMove(true) <= Samples::ColonyTuning::COLLISION_DISTANCE);
    EXPECT_NEAR(DistanceAfterOneMove(false), 1.0f, 1e-4f);
}

TEST(ColonySystems, FadeScalesAndSnapsToZero) {
    ColonyWorld world;
    ASSERT(world.Init());
    world.GetToHome(0)[10] = 0.5f;
    world.GetToFood(0)[20] = 0.0015f;
    Systems::UpdateFade(world, 0.5f, 0, ColonyWorld::CELL_COUNT);
    EXPECT_NEAR(world.GetToHome(0)[10], 0.25f, 1e-6f);
    EXPECT(world.GetToFood(0)[20] == 0.0f);  // 0.00075 is below MARKER_FLOOR
    EXPECT(world.GetToHome(0)[11] == 0.0f);
}

TEST(ColonySystems, FoodIsNeverTakenBelowZero) {
    ColonyWorld world;
    ColonyAnts ants;
    ASSERT(world.Init());
    ASSERT(ants.Init());
    world.AddColony(100.0f, 100.0f, 10.0f);
    world.AddFood(402.0f, 402.0f, 2.0f, 1);  // one unit on one cell
    for (int i = 0; i < 10; ++i) {
        ants.Spawn(0, 402.0f, 402.0f, 0.0f, AntRole::Follower);
    }
    Systems::UpdateClaims(world, ants, 0, ants.GetSlotEnd());
    Systems::UpdateClaims(world, ants, 0, ants.GetSlotEnd());
    EXPECT(world.CountFood() == 0);
    uint32_t carrying = 0;
    for (uint32_t slot = 0; slot < ants.GetSlotEnd(); ++slot) {
        carrying += ants.carrying[slot];
    }
    EXPECT(carrying == 1);
}

TEST(ColonySystems, CarriedFoodIsStoredAtTheNest) {
    ColonyWorld world;
    ColonyAnts ants;
    ASSERT(world.Init());
    ASSERT(ants.Init());
    world.AddColony(100.0f, 100.0f, 10.0f);
    const uint32_t slot = ants.Spawn(0, 102.0f, 100.0f, 0.5f, AntRole::Follower);
    ants.task[slot] = AntTask::ToHome;
    ants.carrying[slot] = 1;
    ants.walkTime[slot] = 9.0f;
    Systems::UpdateClaims(world, ants, 0, ants.GetSlotEnd());
    EXPECT(world.GetNest(0).stock == 1);
    EXPECT(ants.carrying[slot] == 0 && ants.task[slot] == AntTask::ToFood);
    EXPECT(ants.walkTime[slot] == 0.0f);
}

TEST(ColonySystems, MarkersKeepTheStrongest) {
    ColonyWorld world;
    ColonyAnts ants;
    ASSERT(world.Init());
    ASSERT(ants.Init());
    world.AddColony(100.0f, 100.0f, 10.0f);
    // Slot 0 marks on steps 0, 15, ...; slot 1 on steps 14, 29, ...
    ants.Spawn(0, 402.0f, 402.0f, 0.0f, AntRole::Follower);
    ants.Spawn(0, 402.0f, 402.0f, 0.0f, AntRole::Follower);
    ants.walkTime[1] = 20.0f;
    const uint32_t cell = ColonyWorld::CellAt(402.0f, 402.0f);
    Systems::UpdateMarkers(world, ants, 0, 0, ants.GetSlotEnd());
    EXPECT_NEAR(world.GetToHome(0)[cell], 1.0f, 1e-6f);
    Systems::UpdateMarkers(world, ants, 14, 0, ants.GetSlotEnd());
    EXPECT_NEAR(world.GetToHome(0)[cell], 1.0f, 1e-6f);
    EXPECT(world.GetToFood(0)[cell] == 0.0f);  // searching ants drop toHome only
}

TEST(ColonyAnts, FreeingTheTopShrinksTheSlotEnd) {
    ColonyAnts ants;
    ASSERT(ants.Init());
    for (int i = 0; i < 3; ++i) {
        ants.Spawn(0, 10.0f, 10.0f, 0.0f, AntRole::Follower);
    }
    ants.Free(2);
    EXPECT(ants.GetSlotEnd() == 2);
    ants.Free(0);  // in the middle: kept for reuse
    EXPECT(ants.GetSlotEnd() == 2);
    ants.Free(1);  // the top, and slot 0 below it is free too
    EXPECT(ants.GetSlotEnd() == 0 && ants.GetCount() == 0);
    // Slot 0's stale free entry is skipped; the new ant still gets slot 0, once.
    EXPECT(ants.Spawn(0, 10.0f, 10.0f, 0.0f, AntRole::Follower) == 0);
    EXPECT(ants.Spawn(0, 10.0f, 10.0f, 0.0f, AntRole::Follower) == 1);
    EXPECT(ants.GetSlotEnd() == 2 && ants.GetCount() == 2);
}

TEST(ColonySystems, SpawnsCostFoodUntilTheStockRunsShort) {
    ColonyWorld world;
    ColonyAnts ants;
    ASSERT(world.Init());
    ASSERT(ants.Init());
    world.AddColony(100.0f, 100.0f, 10.0f);
    world.GetNest(0).stock = 10;
    ColonySettings settings;
    settings.antTarget = 3;
    static Samples::ColonyPending pending;
    pending.Clear();
    ME::Random random(1);
    Systems::UpdateColonies(world, ants, settings, random, pending, 0, 1);
    EXPECT(pending.spawnCount == 3 && pending.deathCount == 0);
    EXPECT(ants.GetCount() == 0);  // nothing changes until Apply
    // Two paid (4 each), the third free.
    EXPECT(world.GetNest(0).stock == 2 && world.GetNest(0).foodSpent == 8 && world.GetNest(0).freeSpawns == 1);
    Systems::ApplyPending(world, ants, pending, 0, pending.deathCount + pending.spawnCount);
    EXPECT(ants.GetCount() == 3);
    bool bInNest = true;
    for (uint32_t slot = 0; slot < ants.GetSlotEnd(); ++slot) {
        bInNest = bInNest && world.GetNest(0).Contains(ants.x[slot], ants.y[slot]);
    }
    EXPECT(bInNest);
}

TEST(ColonySystems, ExtraAntsLeaveFromTheTopAndDropTheirFood) {
    ColonyWorld world;
    ColonyAnts ants;
    ASSERT(world.Init());
    ASSERT(ants.Init());
    world.AddColony(100.0f, 100.0f, 10.0f);
    for (int i = 0; i < 10; ++i) {
        ants.Spawn(0, 402.0f, 402.0f, 0.0f, AntRole::Follower);
    }
    ants.carrying[9] = 1;
    ants.task[9] = AntTask::ToHome;
    ColonySettings settings;
    settings.antTarget = 7;
    static Samples::ColonyPending pending;
    pending.Clear();
    ME::Random random(1);
    Systems::UpdateColonies(world, ants, settings, random, pending, 0, 1);
    EXPECT(pending.deathCount == 3 && pending.spawnCount == 0);
    Systems::ApplyPending(world, ants, pending, 0, pending.deathCount + pending.spawnCount);
    EXPECT(ants.GetCount() == 7 && ants.GetSlotEnd() == 7);
    EXPECT(world.CountFood() == 1);  // the carried unit is back on its cell
}

TEST(ColonySim, RunsHoldTheInvariantsAsThePopulationChanges) {
    ColonySim sim;
    ASSERT(sim.Init());
    sim.SetCheckInvariants(true);
    ColonyWorld& world = sim.GetWorld();
    world.Clear();
    world.SetWalls(500.0f, 200.0f, 520.0f, 520.0f, true);
    world.AddFood(700.0f, 360.0f, 30.0f, 10);
    world.AddColony(300.0f, 360.0f, 20.0f);
    sim.Start(1'000, 3);
    EXPECT(sim.GetAnts().GetCount() == 1'000);
    EXPECT(sim.GetStartFood() == world.CountFood());

    // 30 s at the start, then up to 1,500 and down to 600.
    for (int step = 0; step < 1'800; ++step) {
        sim.Step();
    }
    EXPECT(sim.GetInvariantFailure() == nullptr);
    sim.GetSettings().antTarget = 1'500;
    for (int step = 0; step < 300; ++step) {
        sim.Step();
    }
    EXPECT(sim.GetAnts().GetCount() == 1'500);
    sim.GetSettings().antTarget = 600;
    for (int step = 0; step < 60; ++step) {
        sim.Step();
    }
    EXPECT(sim.GetAnts().GetCount() == 600);
    EXPECT(sim.GetAnts().GetSlotEnd() == 600);
    EXPECT(sim.GetInvariantFailure() == nullptr);
    EXPECT(sim.CheckInvariants() == nullptr);
}

TEST(ColonySystems, AntsThatDontMoveGetStuck) {
    ColonyWorld world;
    ColonyAnts ants;
    ASSERT(world.Init());
    ASSERT(ants.Init());
    world.AddColony(100.0f, 100.0f, 10.0f);
    const uint32_t slot = ants.Spawn(0, 640.0f, 360.0f, 0.0f, AntRole::Follower);
    ColonySettings settings;
    ME::Random random(2);
    // Sense alone never moves the ant, so the progress check at 4 s finds it where it started.
    for (int step = 0; step < 230; ++step) {
        Systems::UpdateSensing(world, ants, settings, random, DT, 0, ants.GetSlotEnd());
    }
    EXPECT(ants.stuckTime[slot] == 0.0f);
    for (int step = 0; step < 20; ++step) {
        Systems::UpdateSensing(world, ants, settings, random, DT, 0, ants.GetSlotEnd());
    }
    EXPECT(ants.stuckTime[slot] > 0.0f);
}

TEST(ColonySystems, StuckAntsWipeMarkers) {
    ColonyWorld world;
    ColonyAnts ants;
    ASSERT(world.Init());
    ASSERT(ants.Init());
    world.AddColony(100.0f, 100.0f, 10.0f);
    const uint32_t slot = ants.Spawn(0, 402.0f, 402.0f, 0.0f, AntRole::Follower);
    ants.stuckTime[slot] = 1.0f;
    const uint32_t cell = ColonyWorld::CellAt(402.0f, 402.0f);
    world.GetToHome(0)[cell] = 0.5f;
    world.GetToFood(0)[cell] = 0.5f;
    Systems::UpdateMarkers(world, ants, 0, 0, ants.GetSlotEnd());  // a marking step for slot 0, but it is stuck
    EXPECT_NEAR(world.GetToHome(0)[cell], 0.5f * Samples::ColonyTuning::STUCK_WIPE, 1e-6f);
    EXPECT_NEAR(world.GetToFood(0)[cell], 0.5f * Samples::ColonyTuning::STUCK_WIPE, 1e-6f);
}

// Initializes a sim with invariant checks on and an empty map with a nest at (300, 360). False if Init failed.
static bool InitSim(ColonySim& sim) {
    if (!sim.Init()) {
        return false;
    }
    sim.SetCheckInvariants(true);
    sim.GetWorld().Clear();
    sim.GetWorld().AddColony(300.0f, 360.0f, 20.0f);
    return true;
}

TEST(ColonySim, ExplorerShareSetsRoles) {
    ColonySim sim;
    ASSERT(InitSim(sim));
    sim.GetSettings().explorerShare = 0.3f;
    sim.Start(2'000, 5);
    const ColonyAnts& ants = sim.GetAnts();
    uint32_t explorers = 0;
    for (uint32_t slot = 0; slot < ants.GetSlotEnd(); ++slot) {
        explorers += ants.role[slot] == AntRole::Explorer ? 1 : 0;
    }
    EXPECT(explorers > 480 && explorers < 720);  // 600 expected

    sim.SetExplorerShare(0.0f);
    explorers = 0;
    for (uint32_t slot = 0; slot < ants.GetSlotEnd(); ++slot) {
        explorers += ants.role[slot] == AntRole::Explorer ? 1 : 0;
    }
    EXPECT(explorers == 0);
}

TEST(ColonySim, WallsAndFoodDuringARunKeepTheInvariants) {
    ColonySim sim;
    ASSERT(InitSim(sim));
    sim.GetWorld().AddFood(700.0f, 360.0f, 30.0f, 10);
    sim.Start(500, 6);
    for (int step = 0; step < 120; ++step) {
        sim.Step();
    }

    // A wall over the nest, where the ants are, and one over the food: ants move out, the food leaves the total.
    const uint64_t totalBefore = sim.GetStartFood();
    sim.SetWalls(260.0f, 320.0f, 340.0f, 400.0f, true);
    sim.SetWalls(660.0f, 320.0f, 740.0f, 400.0f, true);
    EXPECT(sim.GetStartFood() < totalBefore);
    EXPECT(sim.CheckInvariants() == nullptr);

    // Opening them again and adding food keeps the books straight too.
    sim.SetWalls(260.0f, 320.0f, 340.0f, 400.0f, false);
    sim.AddFood(500.0f, 500.0f, 20.0f, 5);
    for (int step = 0; step < 300; ++step) {
        sim.Step();
    }
    EXPECT(sim.GetInvariantFailure() == nullptr);
    EXPECT(sim.CheckInvariants() == nullptr);
}

TEST(ColonySim, HarvestStepIsRecorded) {
    ColonySim sim;
    ASSERT(InitSim(sim));
    sim.GetWorld().AddFood(342.0f, 362.0f, 2.0f, 1);  // one unit on one cell, 40 units from the nest
    sim.Start(200, 7);
    EXPECT(sim.GetHarvestStep() == ColonySim::NO_STEP);
    for (int step = 0; step < 1'800 && sim.GetHarvestStep() == ColonySim::NO_STEP; ++step) {
        sim.Step();
    }
    EXPECT(sim.GetHarvestStep() != ColonySim::NO_STEP);
    EXPECT(sim.GetHarvestStep() % ColonySim::HARVEST_CHECK_STEPS == 0);
}
