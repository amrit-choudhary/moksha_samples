#include "colony/game/colony_world.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

Samples::ColonyWorld::~ColonyWorld() {
    delete[] walls;
    delete[] food;
    delete[] toHome;
    delete[] toFood;
}

bool Samples::ColonyWorld::Init() {
    if (walls != nullptr) {
        return false;
    }
    walls = new uint8_t[CELL_COUNT];
    food = new uint16_t[CELL_COUNT];
    toHome = new float[MAX_COLONIES * CELL_COUNT];
    toFood = new float[MAX_COLONIES * CELL_COUNT];
    Clear();
    return true;
}

void Samples::ColonyWorld::Clear() {
    std::memset(food, 0, CELL_COUNT * sizeof(food[0]));
    std::memset(toHome, 0, MAX_COLONIES * CELL_COUNT * sizeof(toHome[0]));
    std::memset(toFood, 0, MAX_COLONIES * CELL_COUNT * sizeof(toFood[0]));
    for (uint32_t row = 0; row < GRID_HEIGHT; ++row) {
        for (uint32_t column = 0; column < GRID_WIDTH; ++column) {
            const bool bBorder = row == 0 || column == 0 || row == GRID_HEIGHT - 1 || column == GRID_WIDTH - 1;
            walls[row * GRID_WIDTH + column] = bBorder ? 1 : 0;
        }
    }
    for (ColonyNest& nest : nests) {
        nest = ColonyNest();
    }
    colonyCount = 0;
}

void Samples::ColonyWorld::SetWalls(float x0, float y0, float x1, float y1, bool bWall) {
    // Cells whose centres lie in the rectangle, inside the border ring.
    const int32_t firstColumn = static_cast<int32_t>(std::ceil(x0 / CELL_SIZE - 0.5f));
    const int32_t lastColumn = static_cast<int32_t>(std::floor(x1 / CELL_SIZE - 0.5f));
    const int32_t firstRow = static_cast<int32_t>(std::ceil(y0 / CELL_SIZE - 0.5f));
    const int32_t lastRow = static_cast<int32_t>(std::floor(y1 / CELL_SIZE - 0.5f));
    const int32_t minColumn = firstColumn > 1 ? firstColumn : 1;
    const int32_t maxColumn =
        lastColumn < static_cast<int32_t>(GRID_WIDTH) - 2 ? lastColumn : static_cast<int32_t>(GRID_WIDTH) - 2;
    const int32_t minRow = firstRow > 1 ? firstRow : 1;
    const int32_t maxRow =
        lastRow < static_cast<int32_t>(GRID_HEIGHT) - 2 ? lastRow : static_cast<int32_t>(GRID_HEIGHT) - 2;
    for (int32_t row = minRow; row <= maxRow; ++row) {
        for (int32_t column = minColumn; column <= maxColumn; ++column) {
            const uint32_t cell = static_cast<uint32_t>(row) * GRID_WIDTH + static_cast<uint32_t>(column);
            walls[cell] = bWall ? 1 : 0;
            if (bWall) {
                food[cell] = 0;
                for (uint32_t colony = 0; colony < MAX_COLONIES; ++colony) {
                    toHome[colony * CELL_COUNT + cell] = 0.0f;
                    toFood[colony * CELL_COUNT + cell] = 0.0f;
                }
            }
        }
    }
}

void Samples::ColonyWorld::AddFood(float x, float y, float radius, uint16_t amount) {
    const int32_t minColumn = ColumnOf(x - radius);
    const int32_t maxColumn = ColumnOf(x + radius);
    const int32_t minRow = RowOf(y - radius);
    const int32_t maxRow = RowOf(y + radius);
    for (int32_t row = minRow; row <= maxRow; ++row) {
        for (int32_t column = minColumn; column <= maxColumn; ++column) {
            const float dx = (static_cast<float>(column) + 0.5f) * CELL_SIZE - x;
            const float dy = (static_cast<float>(row) + 0.5f) * CELL_SIZE - y;
            const uint32_t cell = static_cast<uint32_t>(row) * GRID_WIDTH + static_cast<uint32_t>(column);
            if (dx * dx + dy * dy > radius * radius || walls[cell] != 0) {
                continue;
            }
            const uint32_t total = static_cast<uint32_t>(food[cell]) + amount;
            food[cell] = static_cast<uint16_t>(total < MAX_FOOD ? total : MAX_FOOD);
        }
    }
}

bool Samples::ColonyWorld::AddColony(float x, float y, float radius) {
    if (colonyCount >= MAX_COLONIES) {
        return false;
    }
    nests[colonyCount].x = x;
    nests[colonyCount].y = y;
    nests[colonyCount].radius = radius;
    nests[colonyCount].stock = 0;
    ++colonyCount;
    return true;
}

uint32_t Samples::ColonyWorld::CellAt(float x, float y) {
    return static_cast<uint32_t>(RowOf(y)) * GRID_WIDTH + static_cast<uint32_t>(ColumnOf(x));
}

bool Samples::ColonyWorld::IsWall(float x, float y) const {
    if (!(x >= 0.0f && x < WIDTH && y >= 0.0f && y < HEIGHT)) {
        return true;
    }
    return walls[CellAt(x, y)] != 0;
}

bool Samples::ColonyWorld::IsRayClear(float x0, float y0, float x1, float y1) const {
    // Grid traversal (Amanatides and Woo): step to whichever cell border the segment reaches first. The step count is
    // fixed by the end cells, so rounding can't make the walk miss its end or leave the grid.
    int32_t column = ColumnOf(x0);
    int32_t row = RowOf(y0);
    const int32_t endColumn = ColumnOf(x1);
    const int32_t endRow = RowOf(y1);
    if (walls[static_cast<uint32_t>(row) * GRID_WIDTH + static_cast<uint32_t>(column)] != 0) {
        return false;
    }

    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const int32_t stepColumn = endColumn > column ? 1 : -1;
    const int32_t stepRow = endRow > row ? 1 : -1;
    // Ray parameter t in [0, 1] at the next column and row border, and the t one cell spans.
    const float noBorder = 1e30f;
    const float deltaX = dx != 0.0f ? CELL_SIZE / std::fabs(dx) : noBorder;
    const float deltaY = dy != 0.0f ? CELL_SIZE / std::fabs(dy) : noBorder;
    const float borderX = static_cast<float>(stepColumn > 0 ? column + 1 : column) * CELL_SIZE;
    const float borderY = static_cast<float>(stepRow > 0 ? row + 1 : row) * CELL_SIZE;
    float nextX = dx != 0.0f ? (borderX - x0) / dx : noBorder;
    float nextY = dy != 0.0f ? (borderY - y0) / dy : noBorder;

    int32_t columnsLeft = std::abs(endColumn - column);
    int32_t rowsLeft = std::abs(endRow - row);
    while (columnsLeft + rowsLeft > 0) {
        if (rowsLeft == 0 || (columnsLeft > 0 && nextX < nextY)) {
            column += stepColumn;
            nextX += deltaX;
            --columnsLeft;
        } else {
            row += stepRow;
            nextY += deltaY;
            --rowsLeft;
        }
        if (walls[static_cast<uint32_t>(row) * GRID_WIDTH + static_cast<uint32_t>(column)] != 0) {
            return false;
        }
    }
    return true;
}

uint64_t Samples::ColonyWorld::CountFood() const {
    uint64_t total = 0;
    for (uint32_t cell = 0; cell < CELL_COUNT; ++cell) {
        total += food[cell];
    }
    return total;
}

int32_t Samples::ColonyWorld::ColumnOf(float x) {
    const int32_t column = static_cast<int32_t>(std::floor(x / CELL_SIZE));
    return column < 0 ? 0
                      : (column >= static_cast<int32_t>(GRID_WIDTH) ? static_cast<int32_t>(GRID_WIDTH) - 1 : column);
}

int32_t Samples::ColonyWorld::RowOf(float y) {
    const int32_t row = static_cast<int32_t>(std::floor(y / CELL_SIZE));
    return row < 0 ? 0 : (row >= static_cast<int32_t>(GRID_HEIGHT) ? static_cast<int32_t>(GRID_HEIGHT) - 1 : row);
}

Samples::ColonyAntGrid::~ColonyAntGrid() {
    delete[] cellStart;
    delete[] slots;
    delete[] cellOf;
    delete[] cellCursor;
}

bool Samples::ColonyAntGrid::Init(uint32_t maxAnts) {
    if (cellStart != nullptr) {
        return false;
    }
    cellStart = new uint32_t[CELL_COUNT + 1];
    slots = new uint32_t[maxAnts];
    cellOf = new uint32_t[maxAnts];
    cellCursor = new uint32_t[CELL_COUNT];
    std::memset(cellStart, 0, (CELL_COUNT + 1) * sizeof(cellStart[0]));
    slotCount = 0;
    return true;
}

uint32_t Samples::ColonyAntGrid::CellAt(float x, float y) {
    int32_t column = static_cast<int32_t>(std::floor(x / CELL_SIZE));
    int32_t row = static_cast<int32_t>(std::floor(y / CELL_SIZE));
    column =
        column < 0 ? 0 : (column >= static_cast<int32_t>(GRID_WIDTH) ? static_cast<int32_t>(GRID_WIDTH) - 1 : column);
    row = row < 0 ? 0 : (row >= static_cast<int32_t>(GRID_HEIGHT) ? static_cast<int32_t>(GRID_HEIGHT) - 1 : row);
    return static_cast<uint32_t>(row) * GRID_WIDTH + static_cast<uint32_t>(column);
}
