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

void Samples::ColonySystems::UpdateSensing(const ColonyWorld& world, ColonyAnts& ants, const ColonySettings& settings,
                                           ME::Random& random, float deltaTime, uint32_t begin, uint32_t end) {
    using namespace ColonyTuning;
    const uint16_t* food = world.GetFood();
    const float maxTurn = TURN_RATE * deltaTime;

    for (uint32_t slot = begin; slot < end; ++slot) {
        if (ants.alive[slot] == 0) {
            continue;
        }
        const float x = ants.x[slot];
        const float y = ants.y[slot];
        float heading = ants.heading[slot];

        // Progress check: an ant that barely moved since the last one is stuck, and turns a random way.
        ants.progressTime[slot] += deltaTime;
        if (ants.progressTime[slot] >= STUCK_CHECK_TIME) {
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

        const uint32_t colony = ants.colony[slot];
        const bool bSearching = ants.task[slot] == AntTask::ToFood;
        const float* markers = bSearching ? world.GetToFood(colony) : world.GetToHome(colony);
        const ColonyNest& nest = world.GetNest(colony);
        const uint32_t samples =
            ants.role[slot] == AntRole::Follower ? settings.followerSamples : settings.explorerSamples;

        // The goal in sight wins at once; otherwise the strongest marker in sight. A ray is walked only for a point
        // that would win, which gives the same result as checking every point.
        float best = 0.0f;
        float targetX = 0.0f;
        float targetY = 0.0f;
        bool bTarget = false;
        for (uint32_t sample = 0; sample < samples; ++sample) {
            const float angle = heading + (RandomFloat(random) - 0.5f) * SENSE_CONE;
            const float distance = SENSE_MIN + RandomFloat(random) * (SENSE_RANGE - SENSE_MIN);
            const float pointX = x + std::cos(angle) * distance;
            const float pointY = y + std::sin(angle) * distance;
            if (!InsideWorld(pointX, pointY)) {
                continue;
            }
            const uint32_t cell = ColonyWorld::CellAt(pointX, pointY);
            const bool bGoal = bSearching ? food[cell] != 0 : nest.Contains(pointX, pointY);
            if (bGoal) {
                if (world.IsRayClear(x, y, pointX, pointY)) {
                    targetX = pointX;
                    targetY = pointY;
                    bTarget = true;
                    break;
                }
                continue;
            }
            if (markers[cell] > best && world.IsRayClear(x, y, pointX, pointY)) {
                best = markers[cell];
                targetX = pointX;
                targetY = pointY;
                bTarget = true;
            }
        }

        if (bTarget) {
            float turn = WrapAngle(std::atan2(targetY - y, targetX - x) - heading);
            turn = turn > maxTurn ? maxTurn : (turn < -maxTurn ? -maxTurn : turn);
            heading += turn;
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
        if (ants.alive[slot] == 0) {
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

void Samples::ColonySystems::UpdateMarkers(ColonyWorld& world, const ColonyAnts& ants, uint32_t stepIndex,
                                           uint32_t begin, uint32_t end) {
    using namespace ColonyTuning;
    for (uint32_t slot = begin; slot < end; ++slot) {
        if (ants.alive[slot] == 0) {
            continue;
        }
        if (ants.stuckTime[slot] > 0.0f) {
            // Shared write: the wipe.
            const uint32_t colony = ants.colony[slot];
            const uint32_t cell = ColonyWorld::CellAt(ants.x[slot], ants.y[slot]);
            world.GetToHome(colony)[cell] *= STUCK_WIPE;
            world.GetToFood(colony)[cell] *= STUCK_WIPE;
            continue;
        }
        if ((stepIndex + slot) % MARK_PERIOD_STEPS != 0) {
            continue;
        }
        const float strength = std::exp(-MARKER_FALLOFF * ants.walkTime[slot]);
        if (strength < MARKER_MIN) {
            continue;
        }
        const uint32_t colony = ants.colony[slot];
        float* markers = ants.task[slot] == AntTask::ToFood ? world.GetToHome(colony) : world.GetToFood(colony);
        // Shared write: cell = max(cell, strength).
        float& cell = markers[ColonyWorld::CellAt(ants.x[slot], ants.y[slot])];
        if (cell < strength) {
            cell = strength;
        }
    }
}

void Samples::ColonySystems::UpdateFade(ColonyWorld& world, float decay, uint32_t begin, uint32_t end) {
    for (uint32_t colony = 0; colony < ColonyWorld::MAX_COLONIES; ++colony) {
        float* toHome = world.GetToHome(colony);
        float* toFood = world.GetToFood(colony);
        // Faded markers snap to 0, so they never reach denormals (slow on most CPUs).
        for (uint32_t cell = begin; cell < end; ++cell) {
            const float home = toHome[cell] * decay;
            const float found = toFood[cell] * decay;
            toHome[cell] = home < ColonyTuning::MARKER_FLOOR ? 0.0f : home;
            toFood[cell] = found < ColonyTuning::MARKER_FLOOR ? 0.0f : found;
        }
    }
}

void Samples::ColonySystems::UpdateColonies(ColonyWorld& world, const ColonyAnts& ants, const ColonySettings& settings,
                                            ME::Random& random, ColonyPending& pending, uint32_t begin, uint32_t end) {
    using namespace ColonyTuning;
    for (uint32_t colony = begin; colony < end; ++colony) {
        ColonyNest& nest = world.GetNest(colony);

        // v1 has one colony, so the live count is the colony's.
        const uint32_t count = ants.GetCount();
        if (count < settings.antTarget) {
            uint32_t wanted = settings.antTarget - count;
            wanted = wanted < MAX_SPAWNS_PER_STEP ? wanted : MAX_SPAWNS_PER_STEP;
            for (uint32_t i = 0; i < wanted && pending.spawnCount < ColonyPending::MAX_SPAWNS; ++i) {
                if (nest.stock >= SPAWN_COST) {
                    nest.stock -= SPAWN_COST;
                    nest.foodSpent += SPAWN_COST;
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
            }
        } else if (count > settings.antTarget) {
            // The highest slots go first, so the slot end shrinks with the count.
            uint32_t wanted = count - settings.antTarget;
            wanted = wanted < MAX_DEATHS_PER_STEP ? wanted : MAX_DEATHS_PER_STEP;
            for (uint32_t slot = ants.GetSlotEnd(); slot > 0 && wanted > 0; --slot) {
                if (pending.deathCount >= ColonyPending::MAX_DEATHS) {
                    break;
                }
                if (ants.alive[slot - 1] != 0 && ants.colony[slot - 1] == colony) {
                    pending.deaths[pending.deathCount++] = slot - 1;
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
            ants.Spawn(spawn.colony, spawn.x, spawn.y, spawn.heading, spawn.role);
        }
    }
}

void Samples::ColonySystems::UpdatePrevPositions(ColonyAnts& ants, uint32_t begin, uint32_t end) {
    for (uint32_t slot = begin; slot < end; ++slot) {
        ants.prevX[slot] = ants.x[slot];
        ants.prevY[slot] = ants.y[slot];
    }
}
