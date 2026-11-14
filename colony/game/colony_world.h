/**
 * Colony world: a WIDTH × HEIGHT world (Y up, origin at the bottom left) on a grid of CELL_SIZE cells. Each cell holds
 * a wall flag, an amount of food and, per colony, toHome and toFood marker strengths in [0, 1]. Each colony has a nest.
 * The outermost ring of cells is always wall, so nothing leaves the world and a ray never leaves the grid.
 * ColonyAntGrid buckets ant slots by a coarser cell, for neighbour lookups.
 * Pure code, no engine systems, so it can be tested. Arrays are allocated once in Init.
 */

#pragma once

#include <cstdint>

namespace Samples {

class ColonyNest {
   public:
    float x = 0.0f;
    float y = 0.0f;
    float radius = 0.0f;
    uint64_t stock = 0;       // food brought home, less what spawns spent
    uint64_t foodSpent = 0;   // on spawns
    uint32_t freeSpawns = 0;  // spawns made while the stock was short, which cost nothing

    bool Contains(float pointX, float pointY) const {
        const float dx = pointX - x;
        const float dy = pointY - y;
        return dx * dx + dy * dy <= radius * radius;
    }
};

class ColonyWorld {
   public:
    static constexpr float WIDTH = 1280.0f;
    static constexpr float HEIGHT = 720.0f;
    static constexpr float CELL_SIZE = 4.0f;
    static constexpr uint32_t GRID_WIDTH = 320;
    static constexpr uint32_t GRID_HEIGHT = 180;
    static constexpr uint32_t CELL_COUNT = GRID_WIDTH * GRID_HEIGHT;
    static constexpr uint32_t MAX_COLONIES = 1;  // v2: 4

    ColonyWorld() = default;
    ~ColonyWorld();

    ColonyWorld(const ColonyWorld&) = delete;
    ColonyWorld& operator=(const ColonyWorld&) = delete;

    // Allocates the grids and clears them. False if called twice.
    bool Init();

    // An empty map: no food, markers or colonies, walls only on the border ring.
    void Clear();

    // Sets or clears the wall on every cell whose centre lies in [x0, x1] × [y0, y1]. The border ring stays wall.
    // Food and markers on a cell that becomes wall are removed.
    void SetWalls(float x0, float y0, float x1, float y1, bool bWall);

    // Adds amount food to every open cell whose centre lies within radius of (x, y), up to MAX_FOOD per cell.
    void AddFood(float x, float y, float radius, uint16_t amount);

    // Adds a colony with its nest. False when MAX_COLONIES already exist.
    bool AddColony(float x, float y, float radius);

    // The cell holding a point, clamped to the grid.
    static uint32_t CellAt(float x, float y);

    // True if the cell holding a point is wall; points outside the world count as wall.
    bool IsWall(float x, float y) const;

    // True if no cell the segment from (x0, y0) to (x1, y1) crosses is wall, its end cells included.
    bool IsRayClear(float x0, float y0, float x1, float y1) const;

    // The sum of food over every cell.
    uint64_t CountFood() const;

    uint32_t GetColonyCount() const {
        return colonyCount;
    }

    const ColonyNest& GetNest(uint32_t colony) const {
        return nests[colony];
    }
    ColonyNest& GetNest(uint32_t colony) {
        return nests[colony];
    }

    // Per cell, index y * GRID_WIDTH + x.
    const uint8_t* GetWalls() const {
        return walls;
    }
    const uint16_t* GetFood() const {
        return food;
    }
    uint16_t* GetFood() {
        return food;
    }
    const float* GetToHome(uint32_t colony) const {
        return toHome + colony * CELL_COUNT;
    }
    float* GetToHome(uint32_t colony) {
        return toHome + colony * CELL_COUNT;
    }
    const float* GetToFood(uint32_t colony) const {
        return toFood + colony * CELL_COUNT;
    }
    float* GetToFood(uint32_t colony) {
        return toFood + colony * CELL_COUNT;
    }

    static constexpr uint16_t MAX_FOOD = 1'000;

   private:
    // Cell column or row of a coordinate, clamped to the grid.
    static int32_t ColumnOf(float x);
    static int32_t RowOf(float y);

    uint8_t* walls = nullptr;
    uint16_t* food = nullptr;
    float* toHome = nullptr;  // MAX_COLONIES grids, one after the other
    float* toFood = nullptr;

    ColonyNest nests[MAX_COLONIES];
    uint32_t colonyCount = 0;
};

// Ant slots bucketed by cell (a counting sort, rebuilt every step by the AntGrid system). The slots in cell c are
// slots[cellStart[c]] up to slots[cellStart[c + 1]], in slot order.
class ColonyAntGrid {
   public:
    static constexpr float CELL_SIZE = 8.0f;
    static constexpr uint32_t GRID_WIDTH = 160;
    static constexpr uint32_t GRID_HEIGHT = 90;
    static constexpr uint32_t CELL_COUNT = GRID_WIDTH * GRID_HEIGHT;

    ColonyAntGrid() = default;
    ~ColonyAntGrid();

    ColonyAntGrid(const ColonyAntGrid&) = delete;
    ColonyAntGrid& operator=(const ColonyAntGrid&) = delete;

    // Allocates room for maxAnts slots, all cells empty. False if called twice.
    bool Init(uint32_t maxAnts);

    // The cell holding a point, clamped to the grid.
    static uint32_t CellAt(float x, float y);

    uint32_t* cellStart = nullptr;   // CELL_COUNT + 1 entries
    uint32_t* slots = nullptr;       // maxAnts entries
    uint32_t* cellOf = nullptr;      // per ant slot: its cell, written while building
    uint32_t* cellCursor = nullptr;  // CELL_COUNT entries: the next free entry per cell, while building
    uint32_t slotCount = 0;          // live ants in the grid
};

}  // namespace Samples
