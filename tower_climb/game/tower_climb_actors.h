/**
 * Tower Climb actors: plain data in fixed arrays. The scene draws hazard slot i with instance/collider slot i.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace Samples {

// One kind of hazard: its atlas tile and how fast it falls relative to the walls (world units per second).
class HazardType {
   public:
    uint16_t tile;
    float fallSpeed;
};

// Spider and slime cling to the wall; the anvil drops down its lane.
constexpr HazardType HAZARD_TYPES[] = {{122, 0.0f}, {108, 0.0f}, {74, 50.0f}};
constexpr size_t HAZARD_TYPE_COUNT = sizeof(HAZARD_TYPES) / sizeof(HAZARD_TYPES[0]);
constexpr uint8_t HAZARD_DROPPER = 2;

class Hazard {
   public:
    float x = 0.0f;
    float y = 0.0f;
    uint8_t type = 0;
    bool bActive = false;
};

}  // namespace Samples
