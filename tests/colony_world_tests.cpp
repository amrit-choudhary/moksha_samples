/**
 * Tests for Colony's world (ColonyWorld): the border, walls, food, nests and the ray walk.
 */

#include "colony/game/colony_world.h"
#include "moksha/test_framework/test_framework.h"

using Samples::ColonyWorld;

static constexpr float CELL = ColonyWorld::CELL_SIZE;

TEST(ColonyWorld, InitOnlyOnce) {
    ColonyWorld world;
    EXPECT(world.Init());
    EXPECT(!world.Init());
}

TEST(ColonyWorld, ClearLeavesOnlyTheBorder) {
    ColonyWorld world;
    ASSERT(world.Init());
    EXPECT(world.IsWall(1.0f, 1.0f));                                       // corner cell
    EXPECT(world.IsWall(ColonyWorld::WIDTH - 1.0f, 300.0f));                // right column
    EXPECT(!world.IsWall(CELL + 1.0f, CELL + 1.0f));                        // first cell inside
    EXPECT(!world.IsWall(640.0f, 360.0f));                                  // middle
    EXPECT(world.IsWall(-5.0f, 360.0f) && world.IsWall(640.0f, 1'000.0f));  // outside the world
    EXPECT(world.CountFood() == 0);
    EXPECT(world.GetColonyCount() == 0);
}

TEST(ColonyWorld, WallsCoverCellCentresAndKeepTheBorder) {
    ColonyWorld world;
    ASSERT(world.Init());
    // Centres at 102 and 106 lie in [100, 108]; the cell centred at 110 doesn't.
    world.SetWalls(100.0f, 100.0f, 108.0f, 108.0f, true);
    EXPECT(world.IsWall(101.0f, 101.0f));
    EXPECT(world.IsWall(107.0f, 107.0f));
    EXPECT(!world.IsWall(109.0f, 101.0f));
    EXPECT(!world.IsWall(97.0f, 101.0f));

    world.SetWalls(100.0f, 100.0f, 108.0f, 108.0f, false);
    EXPECT(!world.IsWall(101.0f, 101.0f));

    // Clearing the whole world keeps the border ring.
    world.SetWalls(0.0f, 0.0f, ColonyWorld::WIDTH, ColonyWorld::HEIGHT, false);
    EXPECT(world.IsWall(1.0f, 360.0f));
    EXPECT(!world.IsWall(CELL + 1.0f, 360.0f));
}

TEST(ColonyWorld, FoodFillsACircleAndIsCapped) {
    ColonyWorld world;
    ASSERT(world.Init());
    // Radius 2 around a cell centre: only that cell's centre lies within it.
    world.AddFood(102.0f, 102.0f, 2.0f, 7);
    EXPECT(world.CountFood() == 7);
    EXPECT(world.GetFood()[ColonyWorld::CellAt(102.0f, 102.0f)] == 7);

    world.AddFood(102.0f, 102.0f, 2.0f, ColonyWorld::MAX_FOOD);
    EXPECT(world.CountFood() == ColonyWorld::MAX_FOOD);
}

TEST(ColonyWorld, FoodSkipsWallsAndWallsRemoveFood) {
    ColonyWorld world;
    ASSERT(world.Init());
    world.SetWalls(100.0f, 100.0f, 104.0f, 104.0f, true);  // the cell centred at (102, 102)
    world.AddFood(102.0f, 102.0f, 2.0f, 5);
    EXPECT(world.CountFood() == 0);

    world.AddFood(202.0f, 202.0f, 2.0f, 5);
    world.SetWalls(200.0f, 200.0f, 204.0f, 204.0f, true);
    EXPECT(world.CountFood() == 0);
}

TEST(ColonyWorld, ColoniesAreCapped) {
    ColonyWorld world;
    ASSERT(world.Init());
    for (uint32_t i = 0; i < ColonyWorld::MAX_COLONIES; ++i) {
        EXPECT(world.AddColony(100.0f, 100.0f, 10.0f));
    }
    EXPECT(!world.AddColony(200.0f, 200.0f, 10.0f));
    EXPECT(world.GetColonyCount() == ColonyWorld::MAX_COLONIES);
    EXPECT(world.GetNest(0).radius == 10.0f);
}

TEST(ColonyWorld, RaysStopAtWalls) {
    ColonyWorld world;
    ASSERT(world.Init());
    EXPECT(world.IsRayClear(100.0f, 100.0f, 400.0f, 300.0f));
    EXPECT(world.IsRayClear(100.0f, 100.0f, 101.0f, 101.0f));  // within one cell
    EXPECT(world.IsRayClear(300.0f, 100.0f, 100.0f, 300.0f));  // up and to the left

    // A vertical wall at x in [200, 208] between the two ends.
    world.SetWalls(200.0f, 50.0f, 208.0f, 600.0f, true);
    EXPECT(!world.IsRayClear(100.0f, 300.0f, 300.0f, 300.0f));
    EXPECT(!world.IsRayClear(300.0f, 250.0f, 100.0f, 350.0f));
    // Passing above the wall's top is clear.
    EXPECT(world.IsRayClear(100.0f, 650.0f, 300.0f, 650.0f));
    // A ray ending on the wall is blocked; one ending just before it isn't.
    EXPECT(!world.IsRayClear(100.0f, 300.0f, 202.0f, 300.0f));
    EXPECT(world.IsRayClear(100.0f, 300.0f, 199.0f, 300.0f));
    // Rays leaving the world end on the border.
    EXPECT(!world.IsRayClear(640.0f, 360.0f, 640.0f, 2'000.0f));
}

TEST(ColonyWorld, DiagonalRaysCantSlipBetweenCells) {
    ColonyWorld world;
    ASSERT(world.Init());
    // A wall line on the diagonal, one cell per row: a 45-degree ray across it must hit a cell.
    for (int i = 0; i < 20; ++i) {
        const float x = 300.0f + static_cast<float>(i) * CELL;
        const float y = 300.0f + static_cast<float>(i) * CELL;
        world.SetWalls(x, y, x + CELL, y + CELL, true);
    }
    EXPECT(!world.IsRayClear(310.0f, 350.0f, 350.0f, 310.0f));
}
