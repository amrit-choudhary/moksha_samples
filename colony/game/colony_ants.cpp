#include "colony/game/colony_ants.h"

#include <cstring>

Samples::ColonyAnts::~ColonyAnts() {
    delete[] x;
    delete[] y;
    delete[] prevX;
    delete[] prevY;
    delete[] heading;
    delete[] walkTime;
    delete[] carrying;
    delete[] colony;
    delete[] role;
    delete[] task;
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
    carrying = new uint16_t[MAX_ANTS];
    colony = new uint8_t[MAX_ANTS];
    role = new AntRole[MAX_ANTS];
    task = new AntTask[MAX_ANTS];
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
}

uint32_t Samples::ColonyAnts::Spawn(uint8_t inColony, float inX, float inY, float inHeading, AntRole inRole) {
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
    carrying[slot] = 0;
    colony[slot] = inColony;
    role[slot] = inRole;
    task[slot] = AntTask::ToFood;
    alive[slot] = 1;
    ++count;
    return slot;
}

void Samples::ColonyAnts::Free(uint32_t slot) {
    if (slot >= slotEnd || alive[slot] == 0) {
        return;
    }
    alive[slot] = 0;
    --count;
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
