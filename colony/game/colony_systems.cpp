#include "colony/game/colony_systems.h"

#include <cmath>
#include <cstring>

#include "colony/game/colony_ants.h"
#include "colony/game/colony_world.h"

namespace {

constexpr float PI = 3.14159265f;
constexpr float TWO_PI = 2.0f * PI;

// In [0, 1), from the top 24 bits.
float RandomFloat(ME::Random& random) {
    return static_cast<float>(random.Next() >> 8) * (1.0f / 16777216.0f);
}

// Into (-PI, PI].
float WrapAngle(float angle) {
    while (angle > PI) {
        angle -= TWO_PI;
    }
    while (angle <= -PI) {
        angle += TWO_PI;
    }
    return angle;
}

bool InsideWorld(float x, float y) {
    return x >= 0.0f && x < Samples::ColonyWorld::WIDTH && y >= 0.0f && y < Samples::ColonyWorld::HEIGHT;
}

// Sensing samples' directions: CONE_STEPS evenly spaced offsets across the cone, as cos and sin, so a sample costs a
// rotation (four multiplies) instead of a cos and a sin.
constexpr uint32_t CONE_BITS = 6;
constexpr uint32_t CONE_STEPS = 1u << CONE_BITS;

class ConeTable {
   public:
    ConeTable() {
        for (uint32_t i = 0; i < CONE_STEPS; ++i) {
            const float offset = ((static_cast<float>(i) + 0.5f) / static_cast<float>(CONE_STEPS) - 0.5f) *
                                 Samples::ColonyTuning::SENSE_CONE;
            cosOffset[i] = std::cos(offset);
            sinOffset[i] = std::sin(offset);
        }
    }
    float cosOffset[CONE_STEPS];
    float sinOffset[CONE_STEPS];
};

const ConeTable CONE;  // filled at startup, read only after that

// One sensing sample from one random draw: the top CONE_BITS bits pick the direction in the cone around the heading
// (given as its cos and sin), the low 24 bits the distance in [SENSE_MIN, SENSE_RANGE).
void SamplePoint(ME::Random& random, float x, float y, float cosHeading, float sinHeading, float& pointX,
                 float& pointY) {
    using namespace Samples::ColonyTuning;
    const uint32_t bits = random.Next();
    const uint32_t step = bits >> (32 - CONE_BITS);
    const float fraction = static_cast<float>(bits & 0x00FFFFFFu) * (1.0f / 16777216.0f);
    const float distance = SENSE_MIN + fraction * (SENSE_RANGE - SENSE_MIN);
    const float directionX = cosHeading * CONE.cosOffset[step] - sinHeading * CONE.sinOffset[step];
    const float directionY = sinHeading * CONE.cosOffset[step] + cosHeading * CONE.sinOffset[step];
    pointX = x + directionX * distance;
    pointY = y + directionY * distance;
}

// A cell farther than this (in 4-neighbour steps) from every wall sees every sample point without a wall between: a
// point up to SENSE_RANGE away is at most SENSE_RANGE × √2 / CELL_SIZE + 2 steps from the ant's cell (14.1 + 2 for
// 40 units), and so is every cell the ray to it crosses.
constexpr uint8_t RAY_FREE_STEPS = 17;
static_assert(Samples::ColonyTuning::SENSE_RANGE * 1.4143f / Samples::ColonyWorld::CELL_SIZE + 2.0f < RAY_FREE_STEPS);

// True when no ray from (x, y) to a sample point can meet a wall, so the walks can be skipped (the wall distance field
// is ready and the ant is far from every wall).
bool IsRayFree(const Samples::ColonyWorld& world, float x, float y) {
    const uint8_t* wallDistance = world.GetWallDistance();
    return wallDistance != nullptr && wallDistance[Samples::ColonyWorld::CellAt(x, y)] >= RAY_FREE_STEPS;
}

// The nearest live enemy of `colony` (energy above 0) within range of (x, y), through the ant grid, with positions
// read from px and py (previous or current positions). NO_SLOT if none.
uint32_t FindEnemy(const Samples::ColonyAnts& ants, const Samples::ColonyAntGrid& grid, const float* px,
                   const float* py, uint32_t colony, float x, float y, float range) {
    using Samples::ColonyAntGrid;
    const int32_t reach = static_cast<int32_t>(std::ceil(range / ColonyAntGrid::CELL_SIZE));
    const uint32_t home = ColonyAntGrid::CellAt(x, y);
    const int32_t column = static_cast<int32_t>(home % ColonyAntGrid::GRID_WIDTH);
    const int32_t row = static_cast<int32_t>(home / ColonyAntGrid::GRID_WIDTH);
    uint32_t nearest = Samples::ColonyAnts::NO_SLOT;
    float nearestSquared = range * range;
    for (int32_t cellRow = row - reach; cellRow <= row + reach; ++cellRow) {
        if (cellRow < 0 || cellRow >= static_cast<int32_t>(ColonyAntGrid::GRID_HEIGHT)) {
            continue;
        }
        for (int32_t cellColumn = column - reach; cellColumn <= column + reach; ++cellColumn) {
            if (cellColumn < 0 || cellColumn >= static_cast<int32_t>(ColonyAntGrid::GRID_WIDTH)) {
                continue;
            }
            const uint32_t cell =
                static_cast<uint32_t>(cellRow) * ColonyAntGrid::GRID_WIDTH + static_cast<uint32_t>(cellColumn);
            for (uint32_t i = grid.cellStart[cell]; i < grid.cellStart[cell + 1]; ++i) {
                const uint32_t other = grid.slots[i];
                if (ants.alive[other] == 0 || ants.colony[other] == colony || ants.energy[other] <= 0.0f) {
                    continue;
                }
                const float dx = px[other] - x;
                const float dy = py[other] - y;
                const float distanceSquared = dx * dx + dy * dy;
                if (distanceSquared < nearestSquared) {
                    nearestSquared = distanceSquared;
                    nearest = other;
                }
            }
        }
    }
    return nearest;
}

// A soldier's target: the nearest enemy in sight; else, away from the rally point, the lowest neighbour cell of its
// colony's path field; else the strongest toEnemy marker of followerSamples points in its cone. False if none.
bool SenseSoldier(const Samples::ColonyWorld& world, const Samples::ColonyAnts& ants,
                  const Samples::ColonyAntGrid& grid, const Samples::ColonySettings& settings, ME::Random& random,
                  uint32_t slot, float& targetX, float& targetY) {
    using namespace Samples::ColonyTuning;
    using Samples::ColonyWorld;
    const float x = ants.x[slot];
    const float y = ants.y[slot];
    const uint32_t colony = ants.colony[slot];

    const uint32_t enemy = FindEnemy(ants, grid, ants.prevX, ants.prevY, colony, x, y, SOLDIER_SIGHT);
    if (enemy != Samples::ColonyAnts::NO_SLOT) {
        targetX = ants.prevX[enemy];
        targetY = ants.prevY[enemy];
        return true;
    }

    const Samples::ColonyNest& nest = world.GetNest(colony);
    const uint16_t* path = world.GetPathField(colony);
    const float rallyDx = x - nest.rallyX;
    const float rallyDy = y - nest.rallyY;
    if (path != nullptr && nest.bRally && rallyDx * rallyDx + rallyDy * rallyDy > RALLY_REACHED * RALLY_REACHED) {
        const uint32_t cell = ColonyWorld::CellAt(x, y);
        const int32_t column = static_cast<int32_t>(cell % ColonyWorld::GRID_WIDTH);
        const int32_t row = static_cast<int32_t>(cell / ColonyWorld::GRID_WIDTH);
        uint16_t lowest = path[cell];
        bool bFound = false;
        for (int32_t dy = -1; dy <= 1; ++dy) {
            for (int32_t dx = -1; dx <= 1; ++dx) {
                // Ants are never on the border ring, so the 8 neighbours are inside the grid.
                const uint32_t next =
                    static_cast<uint32_t>(row + dy) * ColonyWorld::GRID_WIDTH + static_cast<uint32_t>(column + dx);
                if (path[next] < lowest) {
                    lowest = path[next];
                    targetX = (static_cast<float>(column + dx) + 0.5f) * ColonyWorld::CELL_SIZE;
                    targetY = (static_cast<float>(row + dy) + 0.5f) * ColonyWorld::CELL_SIZE;
                    bFound = true;
                }
            }
        }
        if (bFound) {
            return true;
        }
    }

    const float* markers = world.GetToEnemy(colony);
    const float heading = ants.heading[slot];
    const float cosHeading = std::cos(heading);
    const float sinHeading = std::sin(heading);
    const bool bRayFree = IsRayFree(world, x, y);
    float best = 0.0f;
    bool bTarget = false;
    for (uint32_t sample = 0; sample < settings.followerSamples; ++sample) {
        float pointX = 0.0f;
        float pointY = 0.0f;
        SamplePoint(random, x, y, cosHeading, sinHeading, pointX, pointY);
        if (!InsideWorld(pointX, pointY)) {
            continue;
        }
        const float strength = markers[ColonyWorld::CellAt(pointX, pointY)];
        if (strength > best && (bRayFree || world.IsRayClear(x, y, pointX, pointY))) {
            best = strength;
            targetX = pointX;
            targetY = pointY;
            bTarget = true;
        }
    }
    return bTarget;
}

}  // namespace

void Samples::ColonySystems::UpdateAntGrid(const ColonyAnts& ants, ColonyAntGrid& grid, uint32_t begin, uint32_t end) {
    // Count per cell into cellStart[cell + 1], turn the counts into starts, then fill each cell from its start.
    std::memset(grid.cellStart, 0, (ColonyAntGrid::CELL_COUNT + 1) * sizeof(grid.cellStart[0]));
    for (uint32_t slot = begin; slot < end; ++slot) {
        if (ants.alive[slot] == 0) {
            continue;
        }
        const uint32_t cell = ColonyAntGrid::CellAt(ants.x[slot], ants.y[slot]);
        grid.cellOf[slot] = cell;
        ++grid.cellStart[cell + 1];
    }
    for (uint32_t cell = 0; cell < ColonyAntGrid::CELL_COUNT; ++cell) {
        grid.cellStart[cell + 1] += grid.cellStart[cell];
        grid.cellCursor[cell] = grid.cellStart[cell];
    }
    for (uint32_t slot = begin; slot < end; ++slot) {
        if (ants.alive[slot] != 0) {
            grid.slots[grid.cellCursor[grid.cellOf[slot]]++] = slot;
        }
    }
    grid.slotCount = grid.cellStart[ColonyAntGrid::CELL_COUNT];
}

void Samples::ColonySystems::UpdateSensing(const ColonyWorld& world, ColonyAnts& ants, const ColonyAntGrid& grid,
                                           const ColonySettings& settings, ME::Random& random, float deltaTime,
                                           uint32_t begin, uint32_t end) {
    using namespace ColonyTuning;
    const uint16_t* food = world.GetFood();
    const uint8_t* wallDistance = world.GetWallDistance();
    const float maxTurn = TURN_RATE * deltaTime;

    for (uint32_t slot = begin; slot < end; ++slot) {
        if (ants.alive[slot] == 0) {
            continue;
        }
        const float x = ants.x[slot];
        const float y = ants.y[slot];
        float heading = ants.heading[slot];

        // Progress check: a worker that barely moved since the last one is stuck, and turns a random way. Soldiers
        // aren't checked: guarding a rally point is meant to stay put.
        ants.progressTime[slot] += deltaTime;
        if (ants.progressTime[slot] >= STUCK_CHECK_TIME && ants.caste[slot] == AntCaste::Worker) {
            const float dx = x - ants.progressX[slot];
            const float dy = y - ants.progressY[slot];
            if (dx * dx + dy * dy < STUCK_DISTANCE * STUCK_DISTANCE) {
                ants.stuckTime[slot] = STUCK_WANDER_TIME;
                heading = RandomFloat(random) * TWO_PI - PI;
            }
            ants.progressX[slot] = x;
            ants.progressY[slot] = y;
            ants.progressTime[slot] = 0.0f;
        }
        if (ants.stuckTime[slot] > 0.0f) {
            ants.stuckTime[slot] = ants.stuckTime[slot] > deltaTime ? ants.stuckTime[slot] - deltaTime : 0.0f;
            heading += (RandomFloat(random) - 0.5f) * 2.0f * STEER_NOISE;
            ants.heading[slot] = WrapAngle(heading);
            continue;
        }

        float targetX = 0.0f;
        float targetY = 0.0f;
        bool bTarget = false;
        if (ants.caste[slot] == AntCaste::Soldier) {
            bTarget = SenseSoldier(world, ants, grid, settings, random, slot, targetX, targetY);
        } else {
            const uint32_t colony = ants.colony[slot];
            const bool bSearching = ants.task[slot] == AntTask::ToFood;
            const float* markers = bSearching ? world.GetToFood(colony) : world.GetToHome(colony);
            const ColonyNest& nest = world.GetNest(colony);
            const uint32_t samples =
                ants.role[slot] == AntRole::Follower ? settings.followerSamples : settings.explorerSamples;

            // The goal in sight wins at once; otherwise the strongest marker in sight. A ray is walked only for a
            // point that would win, which gives the same result as checking every point; and not at all far from
            // walls, where no ray can meet one.
            const float cosHeading = std::cos(heading);
            const float sinHeading = std::sin(heading);
            const bool bRayFree = IsRayFree(world, x, y);
            float best = 0.0f;
            for (uint32_t sample = 0; sample < samples; ++sample) {
                float pointX = 0.0f;
                float pointY = 0.0f;
                SamplePoint(random, x, y, cosHeading, sinHeading, pointX, pointY);
                if (!InsideWorld(pointX, pointY)) {
                    continue;
                }
                const uint32_t cell = ColonyWorld::CellAt(pointX, pointY);
                const bool bGoal = bSearching ? food[cell] != 0 : nest.Contains(pointX, pointY);
                if (bGoal) {
                    if (bRayFree || world.IsRayClear(x, y, pointX, pointY)) {
                        targetX = pointX;
                        targetY = pointY;
                        bTarget = true;
                        break;
                    }
                    continue;
                }
                if (markers[cell] > best && (bRayFree || world.IsRayClear(x, y, pointX, pointY))) {
                    best = markers[cell];
                    targetX = pointX;
                    targetY = pointY;
                    bTarget = true;
                }
            }
        }

        if (bTarget) {
            float turn = WrapAngle(std::atan2(targetY - y, targetX - x) - heading);
            turn = turn > maxTurn ? maxTurn : (turn < -maxTurn ? -maxTurn : turn);
            heading += turn;
        }

        // Next to a wall: turn up the wall distance field, away from it. Ants are never on the border ring, so the
        // four neighbours are inside the grid.
        if (wallDistance != nullptr) {
            const uint32_t cell = ColonyWorld::CellAt(x, y);
            if (wallDistance[cell] <= WALL_AVOID_CELLS) {
                const float gradientX =
                    static_cast<float>(wallDistance[cell + 1]) - static_cast<float>(wallDistance[cell - 1]);
                const float gradientY = static_cast<float>(wallDistance[cell + ColonyWorld::GRID_WIDTH]) -
                                        static_cast<float>(wallDistance[cell - ColonyWorld::GRID_WIDTH]);
                if (gradientX != 0.0f || gradientY != 0.0f) {
                    const float avoidTurn = WALL_AVOID_TURN * maxTurn;
                    float turn = WrapAngle(std::atan2(gradientY, gradientX) - heading);
                    turn = turn > avoidTurn ? avoidTurn : (turn < -avoidTurn ? -avoidTurn : turn);
                    heading += turn;
                }
            }
        }

        heading += (RandomFloat(random) - 0.5f) * 2.0f * STEER_NOISE;
        ants.heading[slot] = WrapAngle(heading);
    }
}

void Samples::ColonySystems::UpdateMovement(const ColonyWorld& world, ColonyAnts& ants, const ColonyAntGrid& grid,
                                            const ColonySettings& settings, float deltaTime, uint32_t begin,
                                            uint32_t end) {
    using namespace ColonyTuning;
    const float stepLength = ANT_SPEED * deltaTime;
    const float minDistanceSquared = COLLISION_DISTANCE * COLLISION_DISTANCE;
    for (uint32_t slot = begin; slot < end; ++slot) {
        if (ants.alive[slot] == 0) {
            continue;
        }
        float x = ants.x[slot];
        float y = ants.y[slot];
        const float heading = ants.heading[slot];
        const float nextX = x + std::cos(heading) * stepLength;
        const float nextY = y + std::sin(heading) * stepLength;
        if (world.IsWall(nextX, nextY)) {
            // Bounce: mirror the axis that hits the wall, or turn back in a corner. The ant stays put this step.
            const bool bWallX = world.IsWall(nextX, y);
            const bool bWallY = world.IsWall(x, nextY);
            float bounced = heading + PI;
            if (bWallX && !bWallY) {
                bounced = PI - heading;
            } else if (bWallY && !bWallX) {
                bounced = -heading;
            }
            ants.heading[slot] = WrapAngle(bounced);
        } else {
            x = nextX;
            y = nextY;
        }

        if (settings.bCollisions) {
            // Push away from every neighbour within COLLISION_DISTANCE, by half the overlap (the neighbour pushes
            // itself the other half). Neighbours are read at their previous positions, and only this ant is moved.
            float pushX = 0.0f;
            float pushY = 0.0f;
            const uint32_t home = grid.cellOf[slot];
            const int32_t column = static_cast<int32_t>(home % ColonyAntGrid::GRID_WIDTH);
            const int32_t row = static_cast<int32_t>(home / ColonyAntGrid::GRID_WIDTH);
            for (int32_t neighbourRow = row - 1; neighbourRow <= row + 1; ++neighbourRow) {
                if (neighbourRow < 0 || neighbourRow >= static_cast<int32_t>(ColonyAntGrid::GRID_HEIGHT)) {
                    continue;
                }
                for (int32_t neighbourColumn = column - 1; neighbourColumn <= column + 1; ++neighbourColumn) {
                    if (neighbourColumn < 0 || neighbourColumn >= static_cast<int32_t>(ColonyAntGrid::GRID_WIDTH)) {
                        continue;
                    }
                    const uint32_t cell = static_cast<uint32_t>(neighbourRow) * ColonyAntGrid::GRID_WIDTH +
                                          static_cast<uint32_t>(neighbourColumn);
                    for (uint32_t i = grid.cellStart[cell]; i < grid.cellStart[cell + 1]; ++i) {
                        const uint32_t other = grid.slots[i];
                        if (other == slot) {
                            continue;
                        }
                        const float dx = x - ants.prevX[other];
                        const float dy = y - ants.prevY[other];
                        const float distanceSquared = dx * dx + dy * dy;
                        if (distanceSquared >= minDistanceSquared) {
                            continue;
                        }
                        if (distanceSquared < 1e-8f) {
                            // On top of each other: a direction from the slot, so the two ants part different ways.
                            const float angle = static_cast<float>(slot) * 2.3999632f;
                            pushX += std::cos(angle) * 0.5f * COLLISION_DISTANCE;
                            pushY += std::sin(angle) * 0.5f * COLLISION_DISTANCE;
                            continue;
                        }
                        const float distance = std::sqrt(distanceSquared);
                        const float push = 0.5f * (COLLISION_DISTANCE - distance) / distance;
                        pushX += dx * push;
                        pushY += dy * push;
                    }
                }
            }
            // In a dense crowd the pushes add up; cap them so ants don't jump.
            const float pushSquared = pushX * pushX + pushY * pushY;
            if (pushSquared > MAX_PUSH * MAX_PUSH) {
                const float scale = MAX_PUSH / std::sqrt(pushSquared);
                pushX *= scale;
                pushY *= scale;
            }
            if (!world.IsWall(x + pushX, y + pushY)) {
                x += pushX;
                y += pushY;
            }
        }

        ants.x[slot] = x;
        ants.y[slot] = y;
        ants.walkTime[slot] += deltaTime;
    }
}

void Samples::ColonySystems::UpdateClaims(ColonyWorld& world, ColonyAnts& ants, uint32_t begin, uint32_t end) {
    uint16_t* food = world.GetFood();
    for (uint32_t slot = begin; slot < end; ++slot) {
        if (ants.alive[slot] == 0 || ants.task[slot] == AntTask::Fight) {
            continue;
        }
        const float x = ants.x[slot];
        const float y = ants.y[slot];
        ColonyNest& nest = world.GetNest(ants.colony[slot]);
        if (ants.task[slot] == AntTask::ToFood) {
            // Shared write: plain read, check, decrement (section 7 of the design).
            const uint32_t cell = ColonyWorld::CellAt(x, y);
            if (food[cell] > 0) {
                --food[cell];
                ants.carrying[slot] = 1;
                ants.task[slot] = AntTask::ToHome;
                ants.heading[slot] = WrapAngle(ants.heading[slot] + PI);
                ants.walkTime[slot] = 0.0f;
            } else if (nest.Contains(x, y)) {
                ants.walkTime[slot] = 0.0f;
            }
        } else if (nest.Contains(x, y)) {
            nest.stock += ants.carrying[slot];  // shared write
            ants.carrying[slot] = 0;
            ants.task[slot] = AntTask::ToFood;
            ants.heading[slot] = WrapAngle(ants.heading[slot] + PI);
            ants.walkTime[slot] = 0.0f;
        }
    }
}

void Samples::ColonySystems::UpdateFights(ColonyAnts& ants, const ColonyAntGrid& grid, uint32_t stepIndex,
                                          float deltaTime, uint32_t begin, uint32_t end) {
    using namespace ColonyTuning;
    for (uint32_t slot = begin; slot < end; ++slot) {
        if (ants.alive[slot] == 0) {
            continue;
        }
        const float fightTime = ants.fightTime[slot] + deltaTime;
        ants.fightTime[slot] = fightTime < ColonyAnts::NO_FIGHT ? fightTime : ColonyAnts::NO_FIGHT;
        if (ants.attackCooldown[slot] > 0.0f) {
            ants.attackCooldown[slot] -= deltaTime;
            continue;
        }
        // Ready ants look for an enemy only every FIGHT_SEARCH_STEPS steps (staggered by slot): most find none, and
        // a search is a neighbour scan.
        if (ants.energy[slot] <= 0.0f || (stepIndex + slot) % FIGHT_SEARCH_STEPS != 0) {
            continue;
        }
        const uint32_t enemy =
            FindEnemy(ants, grid, ants.x, ants.y, ants.colony[slot], ants.x[slot], ants.y[slot], FIGHT_DISTANCE);
        if (enemy == ColonyAnts::NO_SLOT) {
            continue;
        }
        // Shared write: the enemy's energy.
        ants.energy[enemy] -= ants.caste[slot] == AntCaste::Soldier ? SOLDIER_DAMAGE : WORKER_DAMAGE;
        ants.attackCooldown[slot] = ATTACK_PERIOD;
        ants.fightTime[slot] = 0.0f;
    }
}

void Samples::ColonySystems::UpdateMarkers(ColonyWorld& world, const ColonyAnts& ants, uint32_t stepIndex,
                                           uint32_t begin, uint32_t end) {
    using namespace ColonyTuning;
    for (uint32_t slot = begin; slot < end; ++slot) {
        if (ants.alive[slot] == 0) {
            continue;
        }
        const uint32_t colony = ants.colony[slot];
        const uint32_t cell = ColonyWorld::CellAt(ants.x[slot], ants.y[slot]);
        if (ants.stuckTime[slot] > 0.0f) {
            // Shared write: the wipe.
            world.GetToHome(colony)[cell] *= STUCK_WIPE;
            world.GetToFood(colony)[cell] *= STUCK_WIPE;
            world.GetToEnemy(colony)[cell] *= STUCK_WIPE;
            continue;
        }
        if ((stepIndex + slot) % MARK_PERIOD_STEPS != 0) {
            continue;
        }

        // Shared writes below: cell = max(cell, strength).
        if (ants.fightTime[slot] < ENEMY_MARK_TIME) {
            const float strength = 1.0f - ants.fightTime[slot] / ENEMY_MARK_TIME;
            float& enemyCell = world.GetToEnemy(colony)[cell];
            if (enemyCell < strength) {
                enemyCell = strength;
            }
        }
        if (ants.caste[slot] == AntCaste::Soldier) {
            continue;
        }
        const float strength = std::exp(-MARKER_FALLOFF * ants.walkTime[slot]);
        if (strength < MARKER_MIN) {
            continue;
        }
        float* markers = ants.task[slot] == AntTask::ToFood ? world.GetToHome(colony) : world.GetToFood(colony);
        if (markers[cell] < strength) {
            markers[cell] = strength;
        }
    }
}

void Samples::ColonySystems::UpdateFade(ColonyWorld& world, float decay, uint32_t begin, uint32_t end) {
    for (uint32_t colony = 0; colony < world.GetColonyCount(); ++colony) {
        float* const grids[] = {world.GetToHome(colony), world.GetToFood(colony), world.GetToEnemy(colony)};
        // Faded markers snap to 0, so they never reach denormals (slow on most CPUs).
        for (float* markers : grids) {
            for (uint32_t cell = begin; cell < end; ++cell) {
                const float faded = markers[cell] * decay;
                markers[cell] = faded < ColonyTuning::MARKER_FLOOR ? 0.0f : faded;
            }
        }
    }
}

void Samples::ColonySystems::CollectDeaths(ColonyWorld& world, const ColonyAnts& ants, ColonyPending& pending,
                                           uint32_t begin, uint32_t end) {
    for (uint32_t slot = begin; slot < end && pending.deathCount < ColonyPending::MAX_DEATHS; ++slot) {
        if (ants.alive[slot] == 0 || ants.energy[slot] > 0.0f) {
            continue;
        }
        const uint32_t colony = ants.colony[slot];
        pending.deaths[pending.deathCount++] = slot;
        ++pending.killsOf[colony];
        if (ants.caste[slot] == AntCaste::Soldier) {
            ++pending.soldierKillsOf[colony];
        }
        ++world.GetNest(colony).losses;
    }
}

void Samples::ColonySystems::UpdateColonies(ColonyWorld& world, const ColonyAnts& ants, const ColonySettings& settings,
                                            ME::Random& random, ColonyPending& pending, uint32_t begin, uint32_t end) {
    using namespace ColonyTuning;
    for (uint32_t colony = begin; colony < end; ++colony) {
        ColonyNest& nest = world.GetNest(colony);

        // Ants dying in this step's fights don't count.
        const uint32_t count = ants.GetCountOf(colony) - pending.killsOf[colony];
        uint32_t soldiers = ants.GetSoldiersOf(colony) - pending.soldierKillsOf[colony];
        const uint32_t target = settings.TargetOf(colony, world.GetColonyCount());
        if (count < target) {
            const uint32_t soldierTarget = static_cast<uint32_t>(nest.soldierShare * static_cast<float>(target) + 0.5f);
            uint32_t wanted = target - count;
            wanted = wanted < MAX_SPAWNS_PER_STEP ? wanted : MAX_SPAWNS_PER_STEP;
            for (uint32_t i = 0; i < wanted && pending.spawnCount < ColonyPending::MAX_SPAWNS; ++i) {
                const bool bSoldier = soldiers < soldierTarget;
                const uint32_t cost = bSoldier ? SOLDIER_COST : SPAWN_COST;
                if (nest.stock >= cost) {
                    nest.stock -= cost;
                    nest.foodSpent += cost;
                } else {
                    ++nest.freeSpawns;
                }
                // Over the inner half of the nest, facing every way.
                const float angle = RandomFloat(random) * TWO_PI;
                const float distance = RandomFloat(random) * 0.5f * nest.radius;
                ColonySpawn& spawn = pending.spawns[pending.spawnCount++];
                spawn.x = nest.x + std::cos(angle) * distance;
                spawn.y = nest.y + std::sin(angle) * distance;
                spawn.heading = RandomFloat(random) * TWO_PI - PI;
                spawn.colony = static_cast<uint8_t>(colony);
                spawn.role = RandomFloat(random) < settings.explorerShare ? AntRole::Explorer : AntRole::Follower;
                spawn.caste = bSoldier ? AntCaste::Soldier : AntCaste::Worker;
                if (bSoldier) {
                    ++soldiers;
                }
            }
        } else if (count > target) {
            // The highest slots go first, so the slot end shrinks with the count. Ants already dying are skipped.
            uint32_t wanted = count - target;
            wanted = wanted < MAX_DEATHS_PER_STEP ? wanted : MAX_DEATHS_PER_STEP;
            for (uint32_t slot = ants.GetSlotEnd(); slot > 0 && wanted > 0; --slot) {
                if (pending.deathCount >= ColonyPending::MAX_DEATHS) {
                    break;
                }
                const uint32_t candidate = slot - 1;
                if (ants.alive[candidate] != 0 && ants.colony[candidate] == colony && ants.energy[candidate] > 0.0f) {
                    pending.deaths[pending.deathCount++] = candidate;
                    --wanted;
                }
            }
        }
    }
}

void Samples::ColonySystems::ApplyPending(ColonyWorld& world, ColonyAnts& ants, const ColonyPending& pending,
                                          uint32_t begin, uint32_t end) {
    uint16_t* food = world.GetFood();
    for (uint32_t entry = begin; entry < end; ++entry) {
        if (entry < pending.deathCount) {
            const uint32_t slot = pending.deaths[entry];
            if (ants.alive[slot] == 0) {
                continue;
            }
            // A dead ant drops its food on its cell (or, if the cell is full, it goes to its nest).
            const uint16_t carried = ants.carrying[slot];
            if (carried > 0) {
                const uint32_t cell = ColonyWorld::CellAt(ants.x[slot], ants.y[slot]);
                if (static_cast<uint32_t>(food[cell]) + carried <= ColonyWorld::MAX_FOOD) {
                    food[cell] = static_cast<uint16_t>(food[cell] + carried);
                } else {
                    world.GetNest(ants.colony[slot]).stock += carried;
                }
            }
            ants.Free(slot);
        } else {
            const ColonySpawn& spawn = pending.spawns[entry - pending.deathCount];
            ants.Spawn(spawn.colony, spawn.x, spawn.y, spawn.heading, spawn.role, spawn.caste);
        }
    }
}

void Samples::ColonySystems::UpdatePrevPositions(ColonyAnts& ants, uint32_t begin, uint32_t end) {
    for (uint32_t slot = begin; slot < end; ++slot) {
        ants.prevX[slot] = ants.x[slot];
        ants.prevY[slot] = ants.y[slot];
    }
}
