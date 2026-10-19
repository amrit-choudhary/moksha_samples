/**
 * Survivors game: the player moves with WASD and auto-fires at the nearest enemy; waves of enemies chase the player.
 * G toggles god mode and B spawns a burst of enemies, for pushing the enemy count up on demand.
 * Each wave is bigger than the last, so the enemy count climbs until the engine struggles. Touching an enemy costs
 * health; at zero the arena is cleared and the waves restart.
 */

#pragma once

#include <cstdint>

#include "moksha/client/game/game.h"
#include "moksha/client/scene/scene_ui.h"
#include "moksha/shared/datastructure/pool.h"
#include "moksha/shared/physics/physics_scene.h"
#include "moksha/shared/random/random_engine.h"
#include "moksha/shared/time/timer_manager.h"
#include "survivors/game/survivors_actors.h"
#include "survivors/scene/scene_survivors.h"

namespace Samples {

class GameSurvivors : public ME::Game {
   public:
    virtual void Init(ME::Engine& inEngine) override;
    virtual void Start() override;
    virtual void Update(double deltaTime) override;
    virtual void End() override;

    // Player vs enemy: costs health. Enemy vs bullet: damages the enemy and spends the bullet.
    virtual void CollisionCallback(ME::ColliderAABB* a, ME::ColliderAABB* b, ME::CollisionResultAABB* result) override;

    virtual const char* GetDisplayName() const override;

   private:
    // Spawns count enemies of one type along the arena's edges; positions come from seed.
    void SpawnWave(uint8_t enemyType, uint16_t count, uint32_t seed);

    // Timer callbacks.
    void OnWaveTimer();
    void OnFireTimer();

    void MovePlayer(float deltaTime);
    void MoveEnemies(float deltaTime);
    void MoveBullets(float deltaTime);
    void ReleaseDead();
    void ResetRound();
    void PrintStats() const;

    static constexpr double WAVE_INTERVAL_SECONDS = 2.0;
    static constexpr double FIRE_INTERVAL_SECONDS = 0.1;
    static constexpr uint32_t WAVE_BASE_COUNT = 3;  // wave n spawns base + n * step enemies
    static constexpr uint32_t ENEMIES_PER_WAVE_STEP = 3;
    static constexpr uint16_t BURST_COUNT = 1'000;  // B spawns this many at once, for stress testing
    static constexpr float PLAYER_SPEED = 60.0f;
    static constexpr float BULLET_SPEED = 200.0f;
    static constexpr uint32_t PLAYER_MAX_HEALTH = 20;
    static constexpr float PLAYER_INVULNERABLE_SECONDS = 1.0f;

    SceneSurvivors* survivorsScene = nullptr;
    ME::PhysicsScene* physicsScene = nullptr;
    ME::Pool<Enemy>* enemies = nullptr;
    ME::Pool<Bullet>* bullets = nullptr;

    ME::Time::TimerHandle waveTimer;
    ME::Time::TimerHandle fireTimer;
    ME::Random waveRandom{"survivorsWave", true};

    uint32_t wave = 0;
    uint32_t kills = 0;
    uint32_t bestWave = 0;
    uint32_t playerHealth = PLAYER_MAX_HEALTH;
    float invulnerableTime = 0.0f;
    bool bPlayerTouched = false;  // set by the physics callback, applied in Update
    bool bGodMode = false;        // G toggles; lets the waves grow without the round resetting
};

}  // namespace Samples
