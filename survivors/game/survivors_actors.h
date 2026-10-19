/**
 * Survivors actors: plain data kept in ME::Pool. The scene draws pool slot i with sprite/collider slot i.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "moksha/shared/math/vec2.h"

namespace Samples {

// One kind of enemy: its atlas tile, speed (world units per second) and hits to kill.
class EnemyType {
   public:
    uint16_t tile;
    float speed;
    float health;
};

// Slime, cyclops, crab, bat, ghost, spider, rat.
constexpr EnemyType ENEMY_TYPES[] = {
    {108, 20.0f, 2.0f}, {109, 12.0f, 5.0f}, {110, 16.0f, 3.0f}, {120, 30.0f, 1.0f},
    {121, 24.0f, 2.0f}, {122, 26.0f, 1.0f}, {123, 22.0f, 1.0f},
};
constexpr size_t ENEMY_TYPE_COUNT = sizeof(ENEMY_TYPES) / sizeof(ENEMY_TYPES[0]);

class Enemy {
   public:
    ME::Vec2 position = ME::Vec2::Zero;
    uint8_t type = 0;
    float health = 1.0f;
    bool bDead = false;
};

class Bullet {
   public:
    ME::Vec2 position = ME::Vec2::Zero;
    ME::Vec2 velocity = ME::Vec2::Zero;
    bool bDead = false;
};

}  // namespace Samples
