#include "colony/game/colony_requests.h"

#include <chrono>
#include <cmath>
#include <cstring>

#include "colony/game/colony_ants.h"

namespace {

using Clock = std::chrono::steady_clock;

constexpr const char* REQUEST_NAMES[] = {"wall_distance", "path", "think"};
static_assert(sizeof(REQUEST_NAMES) / sizeof(REQUEST_NAMES[0]) == Samples::COLONY_REQUEST_KINDS);

constexpr uint32_t W = Samples::ColonyWorld::GRID_WIDTH;
constexpr uint32_t H = Samples::ColonyWorld::GRID_HEIGHT;
constexpr uint32_t CELLS = Samples::ColonyWorld::CELL_COUNT;

float MillisecondsSince(Clock::time_point start) {
    return std::chrono::duration<float, std::milli>(Clock::now() - start).count();
}

float DistanceSquared(float ax, float ay, float bx, float by) {
    const float dx = ax - bx;
    const float dy = ay - by;
    return dx * dx + dy * dy;
}

// Calls visit(neighbour) for each of the cell's 4 neighbours inside the grid.
template <typename Visit>
void ForEachNeighbour(uint32_t cell, Visit visit) {
    const uint32_t column = cell % W;
    const uint32_t row = cell / W;
    if (column > 0) {
        visit(cell - 1);
    }
    if (column + 1 < W) {
        visit(cell + 1);
    }
    if (row > 0) {
        visit(cell - W);
    }
    if (row + 1 < H) {
        visit(cell + W);
    }
}

}  // namespace

const char* Samples::GetColonyRequestName(ColonyRequestKind kind) {
    const uint32_t index = static_cast<uint32_t>(kind);
    return index < COLONY_REQUEST_KINDS ? REQUEST_NAMES[index] : "?";
}

// --- The work -------------------------------------------------------------------------------------------------------

void Samples::ColonyJobs::ComputeWallDistance(const uint8_t* walls, uint8_t* distance, uint32_t* queue) {
    constexpr uint8_t UNSEEN = 255;
    uint32_t head = 0;
    uint32_t tail = 0;
    for (uint32_t cell = 0; cell < CELLS; ++cell) {
        if (walls[cell] != 0) {
            distance[cell] = 0;
            queue[tail++] = cell;
        } else {
            distance[cell] = UNSEEN;
        }
    }
    while (head < tail) {
        const uint32_t cell = queue[head++];
        const uint8_t next = distance[cell] < UNSEEN - 1 ? static_cast<uint8_t>(distance[cell] + 1) : UNSEEN;
        if (next == UNSEEN) {
            continue;  // capped: farther cells keep 255
        }
        ForEachNeighbour(cell, [&](uint32_t neighbour) {
            if (distance[neighbour] == UNSEEN) {
                distance[neighbour] = next;
                queue[tail++] = neighbour;
            }
        });
    }
}

void Samples::ColonyJobs::ComputePathField(const uint8_t* walls, uint32_t targetCell, uint16_t* field,
                                           uint32_t* queue) {
    for (uint32_t cell = 0; cell < CELLS; ++cell) {
        field[cell] = ColonyWorld::NO_PATH;
    }
    if (targetCell >= CELLS || walls[targetCell] != 0) {
        return;
    }
    uint32_t head = 0;
    uint32_t tail = 0;
    field[targetCell] = 0;
    queue[tail++] = targetCell;
    while (head < tail) {
        const uint32_t cell = queue[head++];
        const uint16_t next = static_cast<uint16_t>(field[cell] + 1);
        ForEachNeighbour(cell, [&](uint32_t neighbour) {
            if (walls[neighbour] == 0 && field[neighbour] == ColonyWorld::NO_PATH) {
                field[neighbour] = next;
                queue[tail++] = neighbour;
            }
        });
    }
}

void Samples::ColonyJobs::Think(const ColonyThinkSnapshot& snapshot, ColonyThinkResult& out) {
    using namespace ColonyAI;
    const uint32_t own = snapshot.colony;
    const float ownAnts = static_cast<float>(snapshot.ants[own] > 0 ? snapshot.ants[own] : 1);

    // More soldiers the more enemies are near the nest, for the colony's size.
    float share = BASE_SOLDIERS + THREAT_SOLDIERS * static_cast<float>(snapshot.threatCount) / ownAnts;
    out.soldierShare = share > MAX_SOLDIERS ? MAX_SOLDIERS : share;

    float x = snapshot.nestX[own];
    float y = snapshot.nestY[own];
    if (snapshot.threatCount >= THREAT_MIN) {
        // Defend: meet the enemies near the nest.
        x = snapshot.threatX;
        y = snapshot.threatY;
    } else {
        // The hottest fight: the block of cells with the most toEnemy marker.
        float hottest = 0.0f;
        uint32_t hottestColumn = 0;
        uint32_t hottestRow = 0;
        if (snapshot.toEnemy != nullptr) {
            for (uint32_t blockRow = 0; blockRow < H; blockRow += HOT_BLOCK_CELLS) {
                for (uint32_t blockColumn = 0; blockColumn < W; blockColumn += HOT_BLOCK_CELLS) {
                    float sum = 0.0f;
                    for (uint32_t row = blockRow; row < blockRow + HOT_BLOCK_CELLS && row < H; ++row) {
                        const float* line = snapshot.toEnemy + row * W;
                        for (uint32_t column = blockColumn; column < blockColumn + HOT_BLOCK_CELLS && column < W;
                             ++column) {
                            sum += line[column];
                        }
                    }
                    if (sum > hottest) {
                        hottest = sum;
                        hottestColumn = blockColumn;
                        hottestRow = blockRow;
                    }
                }
            }
        }
        if (hottest >= HOT_MIN) {
            const float half = 0.5f * static_cast<float>(HOT_BLOCK_CELLS);
            x = (static_cast<float>(hottestColumn) + half) * ColonyWorld::CELL_SIZE;
            y = (static_cast<float>(hottestRow) + half) * ColonyWorld::CELL_SIZE;
        } else {
            // Attack the weakest enemy nest when well ahead of it and stocked.
            uint32_t weakest = ColonyWorld::MAX_COLONIES;
            for (uint32_t colony = 0; colony < snapshot.colonyCount; ++colony) {
                if (colony != own && snapshot.ants[colony] > 0 &&
                    (weakest == ColonyWorld::MAX_COLONIES || snapshot.ants[colony] < snapshot.ants[weakest])) {
                    weakest = colony;
                }
            }
            if (weakest != ColonyWorld::MAX_COLONIES && snapshot.stock >= ATTACK_STOCK &&
                ownAnts >= ATTACK_ADVANTAGE * static_cast<float>(snapshot.ants[weakest])) {
                x = snapshot.nestX[weakest];
                y = snapshot.nestY[weakest];
            }
        }
    }

    // A small move keeps the old point, so the path field isn't searched again for nothing.
    out.bRally = true;
    if (snapshot.bRally && DistanceSquared(x, y, snapshot.rallyX, snapshot.rallyY) < RALLY_MOVE * RALLY_MOVE) {
        out.rallyX = snapshot.rallyX;
        out.rallyY = snapshot.rallyY;
    } else {
        out.rallyX = x;
        out.rallyY = y;
    }
}

// --- ColonyRequests -------------------------------------------------------------------------------------------------

Samples::ColonyRequests::~ColonyRequests() {
    delete[] wallDistance.walls;
    delete[] wallDistance.distance;
    delete[] wallDistance.queue;
    for (PathJob& job : paths) {
        delete[] job.walls;
        delete[] job.field;
        delete[] job.queue;
    }
    for (ThinkJob& job : thinks) {
        delete[] job.snapshot.toEnemy;
    }
}

bool Samples::ColonyRequests::Init() {
    if (wallDistance.walls != nullptr) {
        return false;
    }
    wallDistance.walls = new uint8_t[CELLS];
    wallDistance.distance = new uint8_t[CELLS];
    wallDistance.queue = new uint32_t[CELLS];
    for (PathJob& job : paths) {
        job.walls = new uint8_t[CELLS];
        job.field = new uint16_t[CELLS];
        job.queue = new uint32_t[CELLS];
    }
    for (ThinkJob& job : thinks) {
        job.snapshot.toEnemy = new float[CELLS];
    }
    Clear();
    return true;
}

void Samples::ColonyRequests::Clear() {
    wallDistance.state = ColonyJobState::Idle;
    for (PathJob& job : paths) {
        job.state = ColonyJobState::Idle;
    }
    for (ThinkJob& job : thinks) {
        job.state = ColonyJobState::Idle;
    }
    for (ColonyRequestStats& entry : stats) {
        entry = ColonyRequestStats();
    }
}

void Samples::ColonyRequests::Queue(ColonyJobState& state, ColonyRequestKind kind) {
    if (state != ColonyJobState::Idle) {
        ++stats[static_cast<uint32_t>(kind)].superseded;
    }
    state = ColonyJobState::Queued;
    ++stats[static_cast<uint32_t>(kind)].requested;
}

void Samples::ColonyRequests::RecordRun(ColonyRequestKind kind, float ms) {
    ColonyRequestStats& entry = stats[static_cast<uint32_t>(kind)];
    entry.lastMs = ms;
    entry.maxMs = ms > entry.maxMs ? ms : entry.maxMs;
}

void Samples::ColonyRequests::RequestWallDistance(const ColonyWorld& world) {
    std::memcpy(wallDistance.walls, world.GetWalls(), CELLS * sizeof(uint8_t));
    Queue(wallDistance.state, ColonyRequestKind::WallDistance);
}

void Samples::ColonyRequests::RequestPath(const ColonyWorld& world, uint32_t colony, float x, float y) {
    if (colony >= ColonyWorld::MAX_COLONIES) {
        return;
    }
    PathJob& job = paths[colony];
    std::memcpy(job.walls, world.GetWalls(), CELLS * sizeof(uint8_t));
    job.targetCell = ColonyWorld::CellAt(x, y);
    Queue(job.state, ColonyRequestKind::Path);
}

void Samples::ColonyRequests::RequestThink(const ColonyWorld& world, const ColonyAnts& ants, const ColonyAntGrid& grid,
                                           uint32_t colony) {
    if (colony >= world.GetColonyCount()) {
        return;
    }
    ThinkJob& job = thinks[colony];
    if (job.state != ColonyJobState::Idle) {
        return;  // the last think hasn't been applied: no point asking again
    }

    // The snapshot: everything the think reads, copied now.
    ColonyThinkSnapshot& snapshot = job.snapshot;
    snapshot.colony = colony;
    snapshot.colonyCount = world.GetColonyCount();
    for (uint32_t other = 0; other < ColonyWorld::MAX_COLONIES; ++other) {
        const bool bExists = other < world.GetColonyCount();
        snapshot.nestX[other] = bExists ? world.GetNest(other).x : 0.0f;
        snapshot.nestY[other] = bExists ? world.GetNest(other).y : 0.0f;
        snapshot.ants[other] = bExists ? ants.GetCountOf(other) : 0;
    }
    const ColonyNest& nest = world.GetNest(colony);
    snapshot.soldiers = ants.GetSoldiersOf(colony);
    snapshot.stock = nest.stock;
    snapshot.bRally = nest.bRally;
    snapshot.rallyX = nest.rallyX;
    snapshot.rallyY = nest.rallyY;
    std::memcpy(snapshot.toEnemy, world.GetToEnemy(colony), CELLS * sizeof(float));

    // Enemies near the nest, through the ant grid (its cells are from the start of the step; positions are now).
    using ColonyAI::THREAT_RADIUS;
    const int32_t minColumn =
        static_cast<int32_t>(ColonyAntGrid::CellAt(nest.x - THREAT_RADIUS, nest.y) % ColonyAntGrid::GRID_WIDTH);
    const int32_t maxColumn =
        static_cast<int32_t>(ColonyAntGrid::CellAt(nest.x + THREAT_RADIUS, nest.y) % ColonyAntGrid::GRID_WIDTH);
    const int32_t minRow =
        static_cast<int32_t>(ColonyAntGrid::CellAt(nest.x, nest.y - THREAT_RADIUS) / ColonyAntGrid::GRID_WIDTH);
    const int32_t maxRow =
        static_cast<int32_t>(ColonyAntGrid::CellAt(nest.x, nest.y + THREAT_RADIUS) / ColonyAntGrid::GRID_WIDTH);
    uint32_t threats = 0;
    float sumX = 0.0f;
    float sumY = 0.0f;
    for (int32_t row = minRow; row <= maxRow; ++row) {
        for (int32_t column = minColumn; column <= maxColumn; ++column) {
            const uint32_t cell =
                static_cast<uint32_t>(row) * ColonyAntGrid::GRID_WIDTH + static_cast<uint32_t>(column);
            for (uint32_t i = grid.cellStart[cell]; i < grid.cellStart[cell + 1]; ++i) {
                const uint32_t slot = grid.slots[i];
                if (ants.alive[slot] == 0 || ants.colony[slot] == colony ||
                    DistanceSquared(ants.x[slot], ants.y[slot], nest.x, nest.y) > THREAT_RADIUS * THREAT_RADIUS) {
                    continue;
                }
                ++threats;
                sumX += ants.x[slot];
                sumY += ants.y[slot];
            }
        }
    }
    snapshot.threatCount = threats;
    snapshot.threatX = threats > 0 ? sumX / static_cast<float>(threats) : nest.x;
    snapshot.threatY = threats > 0 ? sumY / static_cast<float>(threats) : nest.y;

    Queue(job.state, ColonyRequestKind::Think);
}

void Samples::ColonyRequests::RunQueued() {
    if (wallDistance.state == ColonyJobState::Queued) {
        const Clock::time_point start = Clock::now();
        ColonyJobs::ComputeWallDistance(wallDistance.walls, wallDistance.distance, wallDistance.queue);
        wallDistance.state = ColonyJobState::Done;
        RecordRun(ColonyRequestKind::WallDistance, MillisecondsSince(start));
    }
    for (PathJob& job : paths) {
        if (job.state == ColonyJobState::Queued) {
            const Clock::time_point start = Clock::now();
            ColonyJobs::ComputePathField(job.walls, job.targetCell, job.field, job.queue);
            job.state = ColonyJobState::Done;
            RecordRun(ColonyRequestKind::Path, MillisecondsSince(start));
        }
    }
    for (ThinkJob& job : thinks) {
        if (job.state == ColonyJobState::Queued) {
            const Clock::time_point start = Clock::now();
            ColonyJobs::Think(job.snapshot, job.result);
            job.state = ColonyJobState::Done;
            RecordRun(ColonyRequestKind::Think, MillisecondsSince(start));
        }
    }
}

void Samples::ColonyRequests::ApplyReady(ColonyWorld& world) {
    if (wallDistance.state == ColonyJobState::Done) {
        world.SetWallDistance(wallDistance.distance);
        wallDistance.state = ColonyJobState::Idle;
        ++stats[static_cast<uint32_t>(ColonyRequestKind::WallDistance)].applied;
    }
    for (uint32_t colony = 0; colony < ColonyWorld::MAX_COLONIES; ++colony) {
        PathJob& job = paths[colony];
        if (job.state == ColonyJobState::Done) {
            world.SetPathField(colony, job.field);
            job.state = ColonyJobState::Idle;
            ++stats[static_cast<uint32_t>(ColonyRequestKind::Path)].applied;
        }
    }
    for (uint32_t colony = 0; colony < world.GetColonyCount(); ++colony) {
        ThinkJob& job = thinks[colony];
        if (job.state != ColonyJobState::Done) {
            continue;
        }
        job.state = ColonyJobState::Idle;
        ++stats[static_cast<uint32_t>(ColonyRequestKind::Think)].applied;
        ColonyNest& nest = world.GetNest(colony);
        const ColonyThinkResult& result = job.result;
        nest.soldierShare = result.soldierShare;
        const bool bMoved =
            result.bRally != nest.bRally || result.rallyX != nest.rallyX || result.rallyY != nest.rallyY;
        nest.bRally = result.bRally;
        nest.rallyX = result.rallyX;
        nest.rallyY = result.rallyY;
        if (bMoved && result.bRally) {
            RequestPath(world, colony, result.rallyX, result.rallyY);
        }
    }
}
