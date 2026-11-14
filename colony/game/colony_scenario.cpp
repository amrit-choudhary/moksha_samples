#include "colony/game/colony_scenario.h"

#include <cstdio>
#include <string>

#include "colony/game/colony_step.h"
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

// A food source object: x, y, radius, amount per cell (capped at MAX_FOOD).
void ReadFoodSource(const cJSON* object, Samples::ColonyFoodSource& out) {
    out.x = ReadFloat(object, "x", 0.0f);
    out.y = ReadFloat(object, "y", 0.0f);
    out.radius = ReadFloat(object, "radius", 0.0f);
    const uint32_t amount = ReadUint(object, "amount", 0);
    out.amount =
        static_cast<uint16_t>(amount < Samples::ColonyWorld::MAX_FOOD ? amount : Samples::ColonyWorld::MAX_FOOD);
}

// A nest object: x, y, radius.
void ReadNest(const cJSON* object, Samples::ColonyNestSpot& out) {
    out.x = ReadFloat(object, "x", out.x);
    out.y = ReadFloat(object, "y", out.y);
    out.radius = ReadFloat(object, "radius", out.radius);
}

// An event object: step, event (addWall, removeWall, addFood), and its rect or food fields. False if malformed.
bool ReadEvent(const cJSON* object, Samples::ColonyEvent& out) {
    const cJSON* type = cJSON_GetObjectItem(object, "event");
    if (!cJSON_IsString(type)) {
        return false;
    }
    out.step = ReadUint(object, "step", 0);
    const std::string name = type->valuestring;
    if (name == "addWall" || name == "removeWall") {
        out.type = name == "addWall" ? Samples::ColonyEventType::AddWall : Samples::ColonyEventType::RemoveWall;
        return ReadRect(cJSON_GetObjectItem(object, "rect"), out.rect);
    }
    if (name == "addFood") {
        out.type = Samples::ColonyEventType::AddFood;
        ReadFoodSource(object, out.food);
        return true;
    }
    return false;
}

}  // namespace

bool Samples::ColonyScenario::Load(const char* path) {
    // Every field starts from its default, so nothing carries over from the scenario loaded before.
    *this = ColonyScenario();
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

    // One "nest" object, or a "nests" array.
    nestCount = 0;
    const cJSON* nest = cJSON_GetObjectItem(json, "nest");
    if (cJSON_IsObject(nest)) {
        ReadNest(nest, nests[nestCount++]);
    }
    const cJSON* nestArray = cJSON_GetObjectItem(json, "nests");
    const cJSON* nestItem = nullptr;
    cJSON_ArrayForEach(nestItem, nestArray) {
        if (nestCount < ColonyWorld::MAX_COLONIES && cJSON_IsObject(nestItem)) {
            ReadNest(nestItem, nests[nestCount++]);
        }
    }

    bool bValid = true;
    const cJSON* nameItem = cJSON_GetObjectItem(json, "name");
    if (!cJSON_IsString(nameItem) || nestCount == 0) {
        ME::LogError("Colony: scenario ", path, " needs a name and a nest");
        bValid = false;
    } else {
        snprintf(name, sizeof(name), "%s", nameItem->valuestring);
        seed = ReadUint(json, "seed", seed);
        ants = ReadUint(json, "ants", ants);
        steps = ReadUint(json, "steps", steps);
        const float share = ReadFloat(json, "soldiers", soldiers);
        soldiers = share < 0.0f ? 0.0f : (share > 1.0f ? 1.0f : share);

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
            ReadFoodSource(source, foodSources[foodSourceCount]);
            ++foodSourceCount;
        }

        eventCount = 0;
        const cJSON* eventArray = cJSON_GetObjectItem(json, "events");
        const cJSON* event = nullptr;
        cJSON_ArrayForEach(event, eventArray) {
            if (eventCount >= MAX_EVENTS) {
                break;
            }
            if (ReadEvent(event, events[eventCount])) {
                ++eventCount;
            } else {
                ME::LogWarning("Colony: scenario ", path, " has an event it can't read; skipped");
            }
        }
    }

    cJSON_Delete(json);
    return bValid;
}

void Samples::ColonyScenario::Build(ColonyWorld& world, uint32_t colonyCount) const {
    world.Clear();
    for (uint32_t i = 0; i < wallCount; ++i) {
        world.SetWalls(walls[i].x0, walls[i].y0, walls[i].x1, walls[i].y1, true);
    }
    if (colonyCount == 0) {
        colonyCount = nestCount;
    }
    colonyCount = colonyCount < ColonyWorld::MAX_COLONIES ? colonyCount : ColonyWorld::MAX_COLONIES;
    for (uint32_t colony = 0; colony < colonyCount; ++colony) {
        const ColonyNestSpot& spot = colony < nestCount ? nests[colony] : SPARE_NESTS[colony - 1];
        world.SetWalls(spot.x - spot.radius, spot.y - spot.radius, spot.x + spot.radius, spot.y + spot.radius, false);
        world.AddColony(spot.x, spot.y, spot.radius);
        world.GetNest(colony).soldierShare = soldiers;
    }
    for (uint32_t i = 0; i < foodSourceCount; ++i) {
        const ColonyFoodSource& source = foodSources[i];
        world.AddFood(source.x, source.y, source.radius, source.amount);
    }
}

void Samples::ColonyScenario::ApplyEvents(uint32_t step, ColonySim& sim) const {
    for (uint32_t i = 0; i < eventCount; ++i) {
        const ColonyEvent& event = events[i];
        if (event.step != step) {
            continue;
        }
        switch (event.type) {
            case ColonyEventType::AddWall:
            case ColonyEventType::RemoveWall: {
                const bool bWall = event.type == ColonyEventType::AddWall;
                sim.SetWalls(event.rect.x0, event.rect.y0, event.rect.x1, event.rect.y1, bWall);
                ME::Log("Colony: step ", step, ", ", bWall ? "wall added" : "wall removed");
                break;
            }
            case ColonyEventType::AddFood:
                sim.AddFood(event.food.x, event.food.y, event.food.radius, event.food.amount);
                ME::Log("Colony: step ", step, ", food added");
                break;
        }
    }
}
