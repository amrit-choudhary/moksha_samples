/**
 * Colony ants: MAX_ANTS fixed slots as a structure of arrays. A slot keeps its index for life, so an index identifies
 * an ant; freed slots are reused. Systems loop over [0, GetSlotEnd()) and skip slots that aren't alive. Freeing the
 * highest slots shrinks the slot end, so removing ants also shrinks every system's range.
 * Pure code, no engine systems. Arrays are allocated once in Init.
 */

#pragma once

#include <cstdint>

#include "colony/game/colony_world.h"

namespace Samples {

enum class AntRole : uint8_t {
    Follower,  // samples many points: follows trails closely
    Explorer,  // samples few points: wanders off trails and finds new routes
};

enum class AntTask : uint8_t {
    ToFood,  // searching: follows toFood markers, drops toHome
    ToHome,  // carrying: follows toHome markers, drops toFood
    Fight,   // soldiers: chase enemies in sight, else the rally point, else toEnemy markers
};

enum class AntCaste : uint8_t {
    Worker,   // forages; fights back when an enemy is in reach
    Soldier,  // never forages; more energy and damage
};

class ColonyAnts {
   public:
    static constexpr uint32_t MAX_ANTS = 100'000;
    static constexpr uint32_t NO_SLOT = 0xFFFFFFFF;

    ColonyAnts() = default;
    ~ColonyAnts();

    ColonyAnts(const ColonyAnts&) = delete;
    ColonyAnts& operator=(const ColonyAnts&) = delete;

    // Allocates the slots, all free. False if called twice.
    bool Init();

    // Frees every slot.
    void Clear();

    // A new ant at (x, y), its previous position the same, at full energy for its caste: a worker searching, a soldier
    // fighting. NO_SLOT when every slot is taken or colony is not below ColonyWorld::MAX_COLONIES.
    uint32_t Spawn(uint8_t colony, float x, float y, float heading, AntRole role, AntCaste caste = AntCaste::Worker);

    // Frees a live slot for reuse; free slots at the top are dropped from the slot end.
    void Free(uint32_t slot);

    // Live ants.
    uint32_t GetCount() const {
        return count;
    }

    // Live ants of one colony (below ColonyWorld::MAX_COLONIES).
    uint32_t GetCountOf(uint32_t inColony) const {
        return colonyCounts[inColony];
    }

    // Live soldiers of one colony.
    uint32_t GetSoldiersOf(uint32_t inColony) const {
        return soldierCounts[inColony];
    }

    static constexpr float WORKER_ENERGY = 3.0f;
    static constexpr float SOLDIER_ENERGY = 10.0f;
    static constexpr float NO_FIGHT = 1'000.0f;  // fightTime of an ant that hasn't fought yet

    // One past the highest live slot: the range systems loop over.
    uint32_t GetSlotEnd() const {
        return slotEnd;
    }

    // Per slot. Positions now and at the start of the step (neighbour reads use prev).
    float* x = nullptr;
    float* y = nullptr;
    float* prevX = nullptr;
    float* prevY = nullptr;
    float* heading = nullptr;    // radians
    float* walkTime = nullptr;   // seconds since leaving home or food; sets marker strength
    float* progressX = nullptr;  // where the ant was at the last progress check
    float* progressY = nullptr;
    float* progressTime = nullptr;    // seconds since the last progress check
    float* stuckTime = nullptr;       // seconds left wandering and wiping markers; 0 when not stuck
    float* energy = nullptr;          // fights take it; at 0 or below the ant dies at the end of the step
    float* attackCooldown = nullptr;  // seconds until the ant can attack again
    float* fightTime = nullptr;       // seconds since the ant last attacked (NO_FIGHT at first, capped there)
    uint16_t* carrying = nullptr;
    uint8_t* colony = nullptr;
    AntRole* role = nullptr;
    AntTask* task = nullptr;
    AntCaste* caste = nullptr;
    uint8_t* alive = nullptr;

   private:
    // Refills freeSlots with exactly the free slots below slotEnd.
    void RebuildFreeSlots();

    // Free slots below slotEnd. May also hold stale entries (slots since dropped from the end, or reused), which
    // Spawn skips; a full stack is rebuilt.
    uint32_t* freeSlots = nullptr;
    uint32_t freeCount = 0;
    uint32_t slotEnd = 0;
    uint32_t count = 0;
    uint32_t colonyCounts[ColonyWorld::MAX_COLONIES] = {};
    uint32_t soldierCounts[ColonyWorld::MAX_COLONIES] = {};
};

}  // namespace Samples
