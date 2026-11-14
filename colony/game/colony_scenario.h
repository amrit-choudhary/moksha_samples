/**
 * Colony scenario: a map (walls, food sources, one nest), the ant count, the seed and the step count, loaded from
 * a JSON file in resources/scenarios/. Coordinates are world units, Y up from the bottom left; a rect is
 * [x0, y0, x1, y1]. Timed events come later. The data is fixed capacity; extra walls or food sources are dropped.
 */

#pragma once

#include <cstdint>

namespace Samples {

class ColonyWorld;

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

class ColonyScenario {
   public:
    static constexpr uint32_t MAX_NAME = 32;
    static constexpr uint32_t MAX_WALLS = 32;
    static constexpr uint32_t MAX_FOOD_SOURCES = 16;

    // Reads a scenario from path (relative to the resources root). False, with a log line, if the file is missing or
    // a required field (name, nest) is absent; other fields keep their defaults.
    bool Load(const char* path);

    // Clears world and builds this map into it: walls, then food, then the nest.
    void Build(ColonyWorld& world) const;

    char name[MAX_NAME] = {};
    uint32_t seed = 1;
    uint32_t ants = 2'000;
    uint32_t steps = 0;  // 0: runs until stopped

    float nestX = 0.0f;
    float nestY = 0.0f;
    float nestRadius = 0.0f;

    ColonyRect walls[MAX_WALLS];
    uint32_t wallCount = 0;
    ColonyFoodSource foodSources[MAX_FOOD_SOURCES];
    uint32_t foodSourceCount = 0;
};

}  // namespace Samples
