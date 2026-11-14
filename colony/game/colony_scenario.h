/**
 * Colony scenario: a map (walls, food sources, one nest per colony), the ant count (all colonies together), the
 * starting soldier share, the seed, the step count and timed events (addWall, removeWall, addFood at a step), loaded
 * from a JSON file in resources/scenarios/. Nests are "nests": [{x, y, radius}, ...] (up to MAX_COLONIES), or one
 * "nest" object. Coordinates are world units, Y up from the bottom left; a rect is [x0, y0, x1, y1]. The data is
 * fixed capacity; extras are dropped.
 */

#pragma once

#include <cstdint>

#include "colony/game/colony_world.h"

namespace Samples {

class ColonySim;

class ColonyRect {
   public:
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
};

class ColonyFoodSource {
   public:
    float x = 0.0f;
    float y = 0.0f;
    float radius = 0.0f;
    uint16_t amount = 0;  // per cell
};

class ColonyNestSpot {
   public:
    float x = 0.0f;
    float y = 0.0f;
    float radius = 20.0f;
};

// Where colonies 1-3 go when the Colonies setting asks for more than a scenario has (colony c takes entry c - 1): the
// far corner first, then the other two.
constexpr ColonyNestSpot SPARE_NESTS[ColonyWorld::MAX_COLONIES - 1] = {
    {1120.0f, 580.0f, 20.0f}, {160.0f, 140.0f, 20.0f}, {1120.0f, 140.0f, 20.0f}};

enum class ColonyEventType : uint8_t {
    AddWall,     // rect
    RemoveWall,  // rect
    AddFood,     // food
};

class ColonyEvent {
   public:
    uint32_t step = 0;
    ColonyEventType type = ColonyEventType::AddWall;
    ColonyRect rect;
    ColonyFoodSource food;
};

class ColonyScenario {
   public:
    static constexpr uint32_t MAX_NAME = 32;
    static constexpr uint32_t MAX_WALLS = 32;
    static constexpr uint32_t MAX_FOOD_SOURCES = 16;
    static constexpr uint32_t MAX_EVENTS = 16;

    // Reads a scenario from path (relative to the resources root), replacing every field. False, with a log line, if
    // the file is missing or a required field (name, a nest) is absent; missing optional fields get their defaults.
    bool Load(const char* path);

    // Clears world and builds this map into it: walls, then food, then colonyCount nests (0: the scenario's own),
    // each with the scenario's soldier share. Nests beyond the scenario's come from SPARE_NESTS. Walls under a nest
    // are cleared.
    void Build(ColonyWorld& world, uint32_t colonyCount = 0) const;

    // Applies the events due at step (before that step runs), logging each.
    void ApplyEvents(uint32_t step, ColonySim& sim) const;

    char name[MAX_NAME] = {};
    uint32_t seed = 1;
    uint32_t ants = 2'000;
    uint32_t steps = 0;     // 0: runs until stopped
    float soldiers = 0.0f;  // starting soldier share of every colony (AI think changes it when it runs)

    ColonyNestSpot nests[ColonyWorld::MAX_COLONIES];
    uint32_t nestCount = 0;

    ColonyRect walls[MAX_WALLS];
    uint32_t wallCount = 0;
    ColonyFoodSource foodSources[MAX_FOOD_SOURCES];
    uint32_t foodSourceCount = 0;
    ColonyEvent events[MAX_EVENTS];
    uint32_t eventCount = 0;
};

}  // namespace Samples
