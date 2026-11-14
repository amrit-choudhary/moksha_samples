#include "colony/game/colony_scenario.h"

#include <cstdio>
#include <string>

#include "colony/game/colony_world.h"
#include "moksha/logging/logging.h"
#include "moksha/shared/file_io/vfs.h"
#include "moksha/shared/third_party/json/cJSON.h"

namespace {

float ReadFloat(const cJSON* object, const char* key, float fallback) {
    const cJSON* item = cJSON_GetObjectItem(object, key);
    return cJSON_IsNumber(item) ? static_cast<float>(item->valuedouble) : fallback;
}

uint32_t ReadUint(const cJSON* object, const char* key, uint32_t fallback) {
    const cJSON* item = cJSON_GetObjectItem(object, key);
    return cJSON_IsNumber(item) && item->valuedouble >= 0.0 ? static_cast<uint32_t>(item->valuedouble) : fallback;
}

// A [x0, y0, x1, y1] array, corners in any order.
bool ReadRect(const cJSON* array, Samples::ColonyRect& out) {
    if (!cJSON_IsArray(array) || cJSON_GetArraySize(array) != 4) {
        return false;
    }
    float values[4];
    for (int i = 0; i < 4; ++i) {
        const cJSON* item = cJSON_GetArrayItem(array, i);
        if (!cJSON_IsNumber(item)) {
            return false;
        }
        values[i] = static_cast<float>(item->valuedouble);
    }
    out.x0 = values[0] < values[2] ? values[0] : values[2];
    out.x1 = values[0] < values[2] ? values[2] : values[0];
    out.y0 = values[1] < values[3] ? values[1] : values[3];
    out.y1 = values[1] < values[3] ? values[3] : values[1];
    return true;
}

}  // namespace

bool Samples::ColonyScenario::Load(const char* path) {
    std::string text;
    if (!ME::Vfs::ReadText(ME::FileRoot::Resources, path, text)) {
        ME::LogError("Colony: can't read scenario ", path);
        return false;
    }
    cJSON* json = cJSON_Parse(text.c_str());
    if (json == nullptr) {
        ME::LogError("Colony: scenario ", path, " is not valid JSON");
        return false;
    }

    bool bValid = true;
    const cJSON* nameItem = cJSON_GetObjectItem(json, "name");
    const cJSON* nest = cJSON_GetObjectItem(json, "nest");
    if (!cJSON_IsString(nameItem) || !cJSON_IsObject(nest)) {
        ME::LogError("Colony: scenario ", path, " needs a name and a nest");
        bValid = false;
    } else {
        snprintf(name, sizeof(name), "%s", nameItem->valuestring);
        seed = ReadUint(json, "seed", seed);
        ants = ReadUint(json, "ants", ants);
        steps = ReadUint(json, "steps", steps);
        nestX = ReadFloat(nest, "x", nestX);
        nestY = ReadFloat(nest, "y", nestY);
        nestRadius = ReadFloat(nest, "radius", nestRadius);

        wallCount = 0;
        const cJSON* wallArray = cJSON_GetObjectItem(json, "walls");
        const cJSON* wall = nullptr;
        cJSON_ArrayForEach(wall, wallArray) {
            if (wallCount < MAX_WALLS && ReadRect(wall, walls[wallCount])) {
                ++wallCount;
            }
        }

        foodSourceCount = 0;
        const cJSON* foodArray = cJSON_GetObjectItem(json, "food");
        const cJSON* source = nullptr;
        cJSON_ArrayForEach(source, foodArray) {
            if (foodSourceCount >= MAX_FOOD_SOURCES) {
                break;
            }
            ColonyFoodSource& out = foodSources[foodSourceCount];
            out.x = ReadFloat(source, "x", 0.0f);
            out.y = ReadFloat(source, "y", 0.0f);
            out.radius = ReadFloat(source, "radius", 0.0f);
            const uint32_t amount = ReadUint(source, "amount", 0);
            out.amount = static_cast<uint16_t>(amount < ColonyWorld::MAX_FOOD ? amount : ColonyWorld::MAX_FOOD);
            ++foodSourceCount;
        }
    }

    cJSON_Delete(json);
    return bValid;
}

void Samples::ColonyScenario::Build(ColonyWorld& world) const {
    world.Clear();
    for (uint32_t i = 0; i < wallCount; ++i) {
        world.SetWalls(walls[i].x0, walls[i].y0, walls[i].x1, walls[i].y1, true);
    }
    for (uint32_t i = 0; i < foodSourceCount; ++i) {
        const ColonyFoodSource& source = foodSources[i];
        world.AddFood(source.x, source.y, source.radius, source.amount);
    }
    world.AddColony(nestX, nestY, nestRadius);
}
