#include "evolution/game/creature.h"

#include "moksha/shared/random/random_engine.h"

using namespace ME;

Samples::Creature::Creature() {
    position = Vec2::Zero;
    state = CreatureState::Moving;
    health = 100.0f;
    speed = 5.0f;
    moveCooldown = kMoveCooldownDuration;
    lifetime = 0.0f;
    maxLifetime = 10.0f;
    reproCooldown = 0.0f;
    maxReproCooldown = 10.0f;
}

Samples::Creature::~Creature() {}

Samples::Creature::Creature(const Creature& other) {
    position = other.position;
    state = other.state;
    health = other.health;
    speed = other.speed;
    moveCooldown = other.moveCooldown;
    lifetime = other.lifetime;
    maxLifetime = other.maxLifetime;
    reproCooldown = other.reproCooldown;
    maxReproCooldown = other.maxReproCooldown;
}

Samples::Creature& Samples::Creature::operator=(const Creature& other) {
    if (this != &other) {
        position = other.position;
        state = other.state;
        health = other.health;
        speed = other.speed;
        moveCooldown = other.moveCooldown;
        lifetime = other.lifetime;
        maxLifetime = other.maxLifetime;
        reproCooldown = other.reproCooldown;
        maxReproCooldown = other.maxReproCooldown;
    }
    return *this;
}

void Samples::Creature::Update(float deltaTime, const Grid<uint8_t>* walkableMap, Random& rnd) {
    if (state == CreatureState::Idle || state == CreatureState::Dying || state == CreatureState::Dead ||
        state == CreatureState::Reproducing) {
        return;
    }

    moveCooldown -= deltaTime;
    lifetime += deltaTime;
    reproCooldown += deltaTime;

    if (lifetime >= maxLifetime) {
        state = CreatureState::Dying;
    }

    if (reproCooldown > maxReproCooldown) {
        state = CreatureState::Reproducing;
    }

    if (moveCooldown <= 0.0f) {
        Move(walkableMap, rnd);
        moveCooldown = kMoveCooldownDuration;
    }
}

void Samples::Creature::Spawn(const Vec2& spawnPosition, Random& rnd) {
    position = spawnPosition;
    state = CreatureState::Moving;
    health = 100.0f;
    speed = 5.0f;
    moveCooldown = kMoveCooldownDuration;
    lifetime = 0.0f;
    maxLifetime = static_cast<float>(rnd.NextDouble() * 10.0 + 5.0);
    reproCooldown = 0.0f;
    maxReproCooldown = static_cast<float>(rnd.NextDouble() * 7.0 + 3.0);
}

void Samples::Creature::Move(const Grid<uint8_t>* walkableMap, Random& rnd) {
    // Matches Grid::GetNeighbors8 slot order: E, NE, N, NW, W, SW, S, SE.
    static const int kOffsetsX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    static const int kOffsetsY[8] = {0, 1, 1, 1, 0, -1, -1, -1};

    size_t x = static_cast<size_t>(position.x);
    size_t y = static_cast<size_t>(position.y);

    const uint8_t* neighbors[8];
    walkableMap->GetNeighbors8(x, y, neighbors);

    size_t validSlots[8];
    size_t validCount = 0;
    for (size_t i = 0; i < 8; ++i) {
        if (neighbors[i] != nullptr && *neighbors[i] == 1) {
            validSlots[validCount] = i;
            ++validCount;
        }
    }

    if (validCount == 0) {
        return;
    }

    size_t slot = validSlots[rnd.NextRange(0, static_cast<uint32_t>(validCount - 1))];
    position.x += static_cast<float>(kOffsetsX[slot]);
    position.y += static_cast<float>(kOffsetsY[slot]);
}
