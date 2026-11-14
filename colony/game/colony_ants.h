/**
 * Colony ants: MAX_ANTS fixed slots as a structure of arrays. A slot keeps its index for life, so an index identifies
 * an ant; freed slots are reused. Systems loop over [0, GetSlotEnd()) and skip slots that aren't alive. Freeing the
 * highest slots shrinks the slot end, so removing ants also shrinks every system's range.
 * Pure code, no engine systems. Arrays are allocated once in Init.
 */

#pragma once

#include <cstdint>

namespace Samples {

enum class AntRole : uint8_t {
    Follower,  // samples many points: follows trails closely
    Explorer,  // samples few points: wanders off trails and finds new routes
};

enum class AntTask : uint8_t {
    ToFood,  // searching: follows toFood markers, drops toHome
    ToHome,  // carrying: follows toHome markers, drops toFood
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

    // A new searching ant at (x, y), its previous position the same. NO_SLOT when every slot is taken.
    uint32_t Spawn(uint8_t colony, float x, float y, float heading, AntRole role);

    // Frees a live slot for reuse; free slots at the top are dropped from the slot end.
    void Free(uint32_t slot);

    // Live ants.
    uint32_t GetCount() const {
        return count;
    }

    // One past the highest live slot: the range systems loop over.
    uint32_t GetSlotEnd() const {
        return slotEnd;
    }

    // Per slot. Positions now and at the start of the step (neighbour reads use prev).
    float* x = nullptr;
    float* y = nullptr;
    float* prevX = nullptr;
    float* prevY = nullptr;
    float* heading = nullptr;   // radians
    float* walkTime = nullptr;  // seconds since leaving home or food; sets marker strength
    uint16_t* carrying = nullptr;
    uint8_t* colony = nullptr;
    AntRole* role = nullptr;
    AntTask* task = nullptr;
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
};

}  // namespace Samples
