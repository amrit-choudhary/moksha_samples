/**
 * Creature class.
 */

#pragma once

#include "moksha/shared/datastructure/grid.h"
#include "moksha/shared/math/vec2.h"
#include "moksha/shared/random/random_engine.h"

namespace Samples {

enum class CreatureState {
    None = 0,
    Idle = 1,
    Moving = 2,
    Attacking = 3,
    Reproducing = 4,
    Dying = 5,
    Dead = 6
};

class Creature {
   public:
    Creature();
    virtual ~Creature();
    // Copy constructor, used for pool management.
    Creature(const Creature& other);
    Creature& operator=(const Creature& other);

    ME::Vec2 position = ME::Vec2::Zero;
    CreatureState state = CreatureState::None;

    // Ticks the movement cooldown; once it elapses, tries to move to a random
    // walkable neighbor tile and resets the cooldown.
    void Update(float deltaTime, const ME::Grid<uint8_t>* walkableMap, ME::Random& rnd);

    void Spawn(const ME::Vec2& spawnPosition, ME::Random& rnd);

    float health = 100.0f;
    float speed = 5.0f;
    float moveCooldown = 1.0f;
    float lifetime = 0.0f;
    float maxLifetime = 10.0f;
    float reproCooldown = 0.0f;
    float maxReproCooldown = 10.0f;

    static constexpr float kMoveCooldownDuration = 1.0f;

    void Move(const ME::Grid<uint8_t>* walkableMap, ME::Random& rnd);
};

}  // namespace Samples
