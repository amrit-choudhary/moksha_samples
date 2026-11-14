/**
 * Tests for Colony v2: several colonies (target split, per-colony counts), soldiers, fights and deaths, toEnemy
 * markers, the background requests (wall distance field, path field, AI think, results applied a step later,
 * superseding) and whole runs with rival colonies keeping the invariants.
 */

#include "colony/game/colony_ants.h"
#include "colony/game/colony_requests.h"
#include "colony/game/colony_step.h"
#include "colony/game/colony_systems.h"
#include "colony/game/colony_world.h"
#include "moksha/shared/random/random_engine.h"
#include "moksha/test_framework/test_framework.h"

using Samples::AntCaste;
using Samples::AntRole;
using Samples::AntTask;
using Samples::ColonyAntGrid;
using Samples::ColonyAnts;
using Samples::ColonyRequestKind;
using Samples::ColonyRequests;
using Samples::ColonySettings;
using Samples::ColonySim;
using Samples::ColonyThinkResult;
using Samples::ColonyThinkSnapshot;
using Samples::ColonyWorld;
namespace Jobs = Samples::ColonyJobs;
namespace Systems = Samples::ColonySystems;
namespace Tuning = Samples::ColonyTuning;

static constexpr float DT = 1.0f / 60.0f;
static constexpr uint32_t W = ColonyWorld::GRID_WIDTH;

// The cell at a column and row.
static uint32_t Cell(uint32_t column, uint32_t row) {
    return row * W + column;
}

TEST(ColonySettings, TargetIsSplitEvenly) {
    ColonySettings settings;
    settings.antTarget = 10;
    EXPECT(settings.TargetOf(0, 3) == 4 && settings.TargetOf(1, 3) == 3 && settings.TargetOf(2, 3) == 3);
    EXPECT(settings.TargetOf(0, 1) == 10);
    EXPECT(settings.TargetOf(0, 0) == 0);
}

TEST(ColonyAnts, CountsPerColonyAndCaste) {
    ColonyAnts ants;
    ASSERT(ants.Init());
    const uint32_t worker = ants.Spawn(0, 10.0f, 10.0f, 0.0f, AntRole::Follower);
    const uint32_t soldier = ants.Spawn(2, 10.0f, 10.0f, 0.0f, AntRole::Follower, AntCaste::Soldier);
    EXPECT(ants.Spawn(ColonyWorld::MAX_COLONIES, 10.0f, 10.0f, 0.0f, AntRole::Follower) == ColonyAnts::NO_SLOT);
    EXPECT(ants.GetCountOf(0) == 1 && ants.GetCountOf(2) == 1 && ants.GetSoldiersOf(2) == 1);
    EXPECT(ants.task[soldier] == AntTask::Fight && ants.energy[soldier] == ColonyAnts::SOLDIER_ENERGY);
    EXPECT(ants.task[worker] == AntTask::ToFood && ants.energy[worker] == ColonyAnts::WORKER_ENERGY);
    ants.Free(soldier);
    EXPECT(ants.GetCountOf(2) == 0 && ants.GetSoldiersOf(2) == 0 && ants.GetCount() == 1);
}

TEST(ColonyJobs, WallDistanceCountsStepsToTheNearestWall) {
    ColonyWorld world;
    ASSERT(world.Init());
    world.SetWalls(397.0f, 197.0f, 399.0f, 199.0f, true);  // one cell: column 99, row 49
    static uint8_t distance[ColonyWorld::CELL_COUNT];
    static uint32_t queue[ColonyWorld::CELL_COUNT];
    Jobs::ComputeWallDistance(world.GetWalls(), distance, queue);
    EXPECT(distance[Cell(0, 0)] == 0);  // border
    EXPECT(distance[Cell(1, 1)] == 1);
    EXPECT(distance[Cell(99, 49)] == 0);  // the wall
    EXPECT(distance[Cell(102, 49)] == 3);
    EXPECT(distance[Cell(160, 90)] == 89);  // to the top border; the wall is 102 away
}

TEST(ColonyJobs, PathFieldGoesAroundWalls) {
    ColonyWorld world;
    ASSERT(world.Init());
    // A wall across column 50 from row 1 up to row 100: the way from column 40 to 60 at row 10 goes over its top.
    world.SetWalls(200.0f, 0.0f, 204.0f, 402.0f, true);
    static uint16_t field[ColonyWorld::CELL_COUNT];
    static uint32_t queue[ColonyWorld::CELL_COUNT];
    Jobs::ComputePathField(world.GetWalls(), Cell(40, 10), field, queue);
    EXPECT(field[Cell(40, 10)] == 0);
    EXPECT(field[Cell(45, 10)] == 5);
    EXPECT(field[Cell(50, 10)] == ColonyWorld::NO_PATH);  // the wall
    EXPECT(field[Cell(0, 0)] == ColonyWorld::NO_PATH);    // the border
    // Up 91 rows to row 101, across 20 columns, down 91 rows.
    EXPECT(field[Cell(60, 10)] == 91 + 20 + 91);

    // A target in a wall reaches nothing.
    Jobs::ComputePathField(world.GetWalls(), Cell(50, 10), field, queue);
    EXPECT(field[Cell(40, 10)] == ColonyWorld::NO_PATH);
}

// A two-colony snapshot: nests at (100, 100) and (1100, 600), 100 ants each, no threat, no fights.
static void MakeSnapshot(ColonyThinkSnapshot& snapshot, float* toEnemy) {
    snapshot = ColonyThinkSnapshot();
    snapshot.colony = 0;
    snapshot.colonyCount = 2;
    snapshot.nestX[0] = 100.0f;
    snapshot.nestY[0] = 100.0f;
    snapshot.nestX[1] = 1100.0f;
    snapshot.nestY[1] = 600.0f;
    snapshot.ants[0] = 100;
    snapshot.ants[1] = 100;
    for (uint32_t cell = 0; cell < ColonyWorld::CELL_COUNT; ++cell) {
        toEnemy[cell] = 0.0f;
    }
    snapshot.toEnemy = toEnemy;
}

TEST(ColonyJobs, ThinkGuardsDefendsJoinsAndAttacks) {
    using namespace Samples::ColonyAI;
    static float toEnemy[ColonyWorld::CELL_COUNT];
    ColonyThinkSnapshot snapshot;
    ColonyThinkResult result;

    // Nothing going on: guard the nest with the base soldier share.
    MakeSnapshot(snapshot, toEnemy);
    Jobs::Think(snapshot, result);
    EXPECT(result.bRally && result.rallyX == 100.0f && result.rallyY == 100.0f);
    EXPECT_NEAR(result.soldierShare, BASE_SOLDIERS, 1e-6f);

    // Enemies near the nest: meet them, with more soldiers.
    MakeSnapshot(snapshot, toEnemy);
    snapshot.threatCount = 10;
    snapshot.threatX = 180.0f;
    snapshot.threatY = 120.0f;
    Jobs::Think(snapshot, result);
    EXPECT(result.rallyX == 180.0f && result.rallyY == 120.0f);
    EXPECT_NEAR(result.soldierShare, BASE_SOLDIERS + THREAT_SOLDIERS * 0.1f, 1e-5f);

    // A fight elsewhere (toEnemy in one block): join it.
    MakeSnapshot(snapshot, toEnemy);
    for (uint32_t row = 40; row < 48; ++row) {
        for (uint32_t column = 80; column < 88; ++column) {
            toEnemy[Cell(column, row)] = 0.5f;  // 32 in all
        }
    }
    Jobs::Think(snapshot, result);
    EXPECT_NEAR(result.rallyX, 84.0f * ColonyWorld::CELL_SIZE, 1e-3f);
    EXPECT_NEAR(result.rallyY, 44.0f * ColonyWorld::CELL_SIZE, 1e-3f);

    // Well ahead and stocked: attack the weakest nest.
    MakeSnapshot(snapshot, toEnemy);
    snapshot.ants[1] = 50;
    snapshot.stock = ATTACK_STOCK;
    Jobs::Think(snapshot, result);
    EXPECT(result.rallyX == 1100.0f && result.rallyY == 600.0f);

    // A new point close to the old one keeps the old one.
    MakeSnapshot(snapshot, toEnemy);
    snapshot.bRally = true;
    snapshot.rallyX = 110.0f;
    snapshot.rallyY = 100.0f;
    Jobs::Think(snapshot, result);
    EXPECT(result.rallyX == 110.0f && result.rallyY == 100.0f);
}

TEST(ColonyRequests, ResultsAreAppliedAfterTheyRunAndNewRequestsSupersede) {
    ColonyWorld world;
    ASSERT(world.Init());
    world.AddColony(100.0f, 100.0f, 10.0f);
    static ColonyRequests requests;
    ASSERT(requests.Init());
    requests.Clear();

    requests.RequestWallDistance(world);
    requests.ApplyReady(world);
    EXPECT(world.GetWallDistance() == nullptr);  // queued, not run
    requests.RunQueued();
    EXPECT(world.GetWallDistance() == nullptr);  // done, not applied
    requests.ApplyReady(world);
    EXPECT(world.GetWallDistance() != nullptr);
    EXPECT(requests.GetStats(ColonyRequestKind::WallDistance).applied == 1);

    // Two path requests before one runs: the second wins.
    requests.RequestPath(world, 0, 400.0f, 400.0f);
    requests.RequestPath(world, 0, 800.0f, 400.0f);
    requests.RunQueued();
    requests.ApplyReady(world);
    const uint16_t* field = world.GetPathField(0);
    ASSERT(field != nullptr);
    EXPECT(field[ColonyWorld::CellAt(800.0f, 400.0f)] == 0);
    EXPECT(requests.GetStats(ColonyRequestKind::Path).requested == 2);
    EXPECT(requests.GetStats(ColonyRequestKind::Path).superseded == 1);
    EXPECT(requests.GetStats(ColonyRequestKind::Path).applied == 1);
}

TEST(ColonySystems, EnemiesInReachFightAndTheBeatenDie) {
    ColonyWorld world;
    ColonyAnts ants;
    ColonyAntGrid grid;
    ASSERT(world.Init());
    ASSERT(ants.Init());
    ASSERT(grid.Init(ColonyAnts::MAX_ANTS));
    world.AddColony(100.0f, 100.0f, 10.0f);
    world.AddColony(1100.0f, 600.0f, 10.0f);
    const uint32_t worker = ants.Spawn(0, 400.0f, 400.0f, 0.0f, AntRole::Follower);
    const uint32_t soldier = ants.Spawn(1, 402.0f, 400.0f, 0.0f, AntRole::Follower, AntCaste::Soldier);
    const uint32_t friendOfWorker = ants.Spawn(0, 400.0f, 401.0f, 0.0f, AntRole::Follower);
    ants.carrying[worker] = 1;
    ants.task[worker] = AntTask::ToHome;
    Systems::UpdateAntGrid(ants, grid, 0, ants.GetSlotEnd());

    // Each ant looks for an enemy on its own step of four (slot 0 on step 0, slot 1 on step 1, ...).
    for (uint32_t step = 0; step < Tuning::FIGHT_SEARCH_STEPS; ++step) {
        Systems::UpdateFights(ants, grid, step, DT, 0, ants.GetSlotEnd());
    }
    // The soldier hits the nearest enemy (the worker, 2 away; the other is 2.24 away); both workers hit the soldier.
    EXPECT_NEAR(ants.energy[worker], ColonyAnts::WORKER_ENERGY - Tuning::SOLDIER_DAMAGE, 1e-6f);
    EXPECT_NEAR(ants.energy[soldier], ColonyAnts::SOLDIER_ENERGY - 2.0f * Tuning::WORKER_DAMAGE, 1e-6f);
    EXPECT(ants.energy[friendOfWorker] == ColonyAnts::WORKER_ENERGY);
    // The soldier attacked on step 1; steps 2 and 3 count its fight time up and its cooldown down.
    EXPECT_NEAR(ants.fightTime[soldier], 2.0f * DT, 1e-6f);
    EXPECT_NEAR(ants.attackCooldown[soldier], Tuning::ATTACK_PERIOD - 2.0f * DT, 1e-6f);

    // Cooling down: nobody hits again over the next round of searches.
    for (uint32_t step = Tuning::FIGHT_SEARCH_STEPS; step < 2 * Tuning::FIGHT_SEARCH_STEPS; ++step) {
        Systems::UpdateFights(ants, grid, step, DT, 0, ants.GetSlotEnd());
    }
    EXPECT_NEAR(ants.energy[soldier], ColonyAnts::SOLDIER_ENERGY - 2.0f * Tuning::WORKER_DAMAGE, 1e-6f);

    // Beaten: the death is queued, counted, and the carried food lands on the ant's cell.
    ants.energy[worker] = 0.0f;
    static Samples::ColonyPending pending;
    pending.Clear();
    Systems::CollectDeaths(world, ants, pending, 0, ants.GetSlotEnd());
    EXPECT(pending.deathCount == 1 && pending.deaths[0] == worker && pending.killsOf[0] == 1);
    EXPECT(world.GetNest(0).losses == 1);
    Systems::ApplyPending(world, ants, pending, 0, pending.deathCount);
    EXPECT(ants.alive[worker] == 0 && world.CountFood() == 1);
}

TEST(ColonySystems, FightersMarkToEnemyAndSoldiersMarkNothingElse) {
    ColonyWorld world;
    ColonyAnts ants;
    ASSERT(world.Init());
    ASSERT(ants.Init());
    world.AddColony(100.0f, 100.0f, 10.0f);
    const uint32_t soldier = ants.Spawn(0, 402.0f, 402.0f, 0.0f, AntRole::Follower, AntCaste::Soldier);
    ants.fightTime[soldier] = 0.0f;
    Systems::UpdateMarkers(world, ants, 0, 0, ants.GetSlotEnd());  // a marking step for slot 0
    const uint32_t cell = ColonyWorld::CellAt(402.0f, 402.0f);
    EXPECT_NEAR(world.GetToEnemy(0)[cell], 1.0f, 1e-6f);
    EXPECT(world.GetToHome(0)[cell] == 0.0f && world.GetToFood(0)[cell] == 0.0f);
}

TEST(ColonySystems, SoldiersSpawnByTheSoldierShare) {
    ColonyWorld world;
    ColonyAnts ants;
    ASSERT(world.Init());
    ASSERT(ants.Init());
    world.AddColony(100.0f, 100.0f, 10.0f);
    world.GetNest(0).soldierShare = 0.25f;
    world.GetNest(0).stock = 1'000;
    ColonySettings settings;
    settings.antTarget = 40;
    static Samples::ColonyPending pending;
    pending.Clear();
    ME::Random random(1);
    Systems::UpdateColonies(world, ants, settings, random, pending, 0, 1);
    Systems::ApplyPending(world, ants, pending, 0, pending.spawnCount);
    EXPECT(ants.GetCountOf(0) == 40 && ants.GetSoldiersOf(0) == 10);
    EXPECT(world.GetNest(0).foodSpent == 10 * Tuning::SOLDIER_COST + 30 * Tuning::SPAWN_COST);
}

// A sim with invariant checks on and two nests 120 units apart, so the colonies meet at once.
static bool InitDuel(ColonySim& sim) {
    if (!sim.Init()) {
        return false;
    }
    sim.SetCheckInvariants(true);
    ColonyWorld& world = sim.GetWorld();
    world.Clear();
    world.AddColony(580.0f, 360.0f, 20.0f);
    world.AddColony(700.0f, 360.0f, 20.0f);
    for (uint32_t colony = 0; colony < 2; ++colony) {
        world.GetNest(colony).soldierShare = 0.2f;
    }
    world.AddFood(640.0f, 300.0f, 20.0f, 10);
    return true;
}

TEST(ColonySim, RivalColoniesFightThinkAndKeepTheInvariants) {
    ColonySim sim;
    ASSERT(InitDuel(sim));
    sim.Start(1'000, 8);
    EXPECT(sim.IsAIOn());
    EXPECT(sim.GetAnts().GetCountOf(0) == 500 && sim.GetAnts().GetCountOf(1) == 500);
    EXPECT(sim.GetAnts().GetSoldiersOf(0) == 100);
    for (int step = 0; step < 1'200; ++step) {
        sim.Step();
    }
    EXPECT(sim.GetInvariantFailure() == nullptr);
    EXPECT(sim.CheckInvariants() == nullptr);
    const ColonyWorld& world = sim.GetWorld();
    EXPECT(world.GetNest(0).losses + world.GetNest(1).losses > 0);
    EXPECT(world.GetWallDistance() != nullptr);
    EXPECT(sim.GetRequests().GetStats(ColonyRequestKind::Think).applied >= 2 * (1'200 / 30) - 2);
    EXPECT(world.GetNest(0).bRally && world.GetPathField(0) != nullptr);
    // The colonies are kept at their targets: losses are replaced.
    EXPECT(sim.GetAnts().GetCount() > 900);
}

TEST(ColonySim, OneColonyHasNoAIOrSoldiers) {
    ColonySim sim;
    ASSERT(sim.Init());
    sim.GetWorld().Clear();
    sim.GetWorld().AddColony(300.0f, 360.0f, 20.0f);
    sim.Start(500, 9);
    for (int step = 0; step < 120; ++step) {
        sim.Step();
    }
    EXPECT(!sim.IsAIOn());
    EXPECT(sim.GetAnts().GetSoldiersOf(0) == 0 && !sim.GetWorld().GetNest(0).bRally);
    EXPECT(sim.GetRequests().GetStats(ColonyRequestKind::Think).requested == 0);
    EXPECT(sim.GetRequests().GetStats(ColonyRequestKind::WallDistance).applied == 1);
}
