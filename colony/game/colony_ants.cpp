#include "colony/game/colony_ants.h"

#include <cstring>

Samples::ColonyAnts::~ColonyAnts() {
    delete[] x;
    delete[] y;
    delete[] prevX;
    delete[] prevY;
    delete[] heading;
    delete[] walkTime;
    delete[] progressX;
    delete[] progressY;
    delete[] progressTime;
    delete[] stuckTime;
    delete[] energy;
    delete[] attackCooldown;
    delete[] fightTime;
    delete[] carrying;
    delete[] colony;
    delete[] role;
    delete[] task;
    delete[] caste;
    delete[] alive;
    delete[] freeSlots;
}

bool Samples::ColonyAnts::Init() {
    if (x != nullptr) {
        return false;
    }
    x = new float[MAX_ANTS];
    y = new float[MAX_ANTS];
    prevX = new float[MAX_ANTS];
    prevY = new float[MAX_ANTS];
    heading = new float[MAX_ANTS];
    walkTime = new float[MAX_ANTS];
    progressX = new float[MAX_ANTS];
    progressY = new float[MAX_ANTS];
    progressTime = new float[MAX_ANTS];
    stuckTime = new float[MAX_ANTS];
    energy = new float[MAX_ANTS];
    attackCooldown = new float[MAX_ANTS];
    fightTime = new float[MAX_ANTS];
    carrying = new uint16_t[MAX_ANTS];
    colony = new uint8_t[MAX_ANTS];
    role = new AntRole[MAX_ANTS];
    task = new AntTask[MAX_ANTS];
    caste = new AntCaste[MAX_ANTS];
    alive = new uint8_t[MAX_ANTS];
    freeSlots = new uint32_t[MAX_ANTS];
    Clear();
    return true;
}

void Samples::ColonyAnts::Clear() {
    std::memset(alive, 0, MAX_ANTS * sizeof(alive[0]));
    freeCount = 0;
    slotEnd = 0;
    count = 0;
    for (uint32_t i = 0; i < ColonyWorld::MAX_COLONIES; ++i) {
        colonyCounts[i] = 0;
        soldierCounts[i] = 0;
    }
}

uint32_t Samples::ColonyAnts::Spawn(uint8_t inColony, float inX, float inY, float inHeading, AntRole inRole,
                                    AntCaste inCaste) {
    if (inColony >= ColonyWorld::MAX_COLONIES) {
        return NO_SLOT;
    }
    uint32_t slot = NO_SLOT;
    while (freeCount > 0) {
        const uint32_t candidate = freeSlots[--freeCount];
        if (candidate < slotEnd && alive[candidate] == 0) {
            slot = candidate;
            break;
        }
    }
    if (slot == NO_SLOT) {
        if (slotEnd >= MAX_ANTS) {
            return NO_SLOT;
        }
        slot = slotEnd++;
    }
    x[slot] = inX;
    y[slot] = inY;
    prevX[slot] = inX;
    prevY[slot] = inY;
    heading[slot] = inHeading;
    walkTime[slot] = 0.0f;
    progressX[slot] = inX;
    progressY[slot] = inY;
    progressTime[slot] = 0.0f;
    stuckTime[slot] = 0.0f;
    const bool bSoldier = inCaste == AntCaste::Soldier;
    energy[slot] = bSoldier ? SOLDIER_ENERGY : WORKER_ENERGY;
    attackCooldown[slot] = 0.0f;
    fightTime[slot] = NO_FIGHT;
    carrying[slot] = 0;
    colony[slot] = inColony;
    role[slot] = inRole;
    task[slot] = bSoldier ? AntTask::Fight : AntTask::ToFood;
    caste[slot] = inCaste;
    alive[slot] = 1;
    ++count;
    ++colonyCounts[inColony];
    if (bSoldier) {
        ++soldierCounts[inColony];
    }
    return slot;
}

void Samples::ColonyAnts::Free(uint32_t slot) {
    if (slot >= slotEnd || alive[slot] == 0) {
        return;
    }
    alive[slot] = 0;
    --count;
    --colonyCounts[colony[slot]];
    if (caste[slot] == AntCaste::Soldier) {
        --soldierCounts[colony[slot]];
    }
    if (slot + 1 == slotEnd) {
        while (slotEnd > 0 && alive[slotEnd - 1] == 0) {
            --slotEnd;
        }
        return;
    }
    if (freeCount < MAX_ANTS) {
        freeSlots[freeCount++] = slot;
    } else {
        RebuildFreeSlots();
    }
}

void Samples::ColonyAnts::RebuildFreeSlots() {
    freeCount = 0;
    for (uint32_t slot = 0; slot < slotEnd; ++slot) {
        if (alive[slot] == 0) {
            freeSlots[freeCount++] = slot;
        }
    }
}
