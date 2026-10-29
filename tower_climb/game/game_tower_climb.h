/**
 * Tower Climb game: the climber clings to one wall while the tower scrolls down. A tap on the other half of the
 * screen (a mouse click on desktop, or Left/Right/A/D) leaps to that wall. Crawlers on the walls and anvils dropping
 * down a wall's lane end the climb on touch. Speed and hazard density ramp with height up to a cap; the best height
 * is saved to UserData.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "moksha/client/game/game.h"
#include "moksha/shared/physics/physics_scene.h"
#include "moksha/shared/random/random_engine.h"
#include "tower_climb/game/tower_climb_actors.h"
#include "tower_climb/scene/scene_tower_climb.h"
#include "tower_climb/scene/scene_ui_tower_climb.h"

namespace Samples {

class GameTowerClimb : public ME::Game {
   public:
    virtual void Init(ME::Engine& inEngine) override;
    virtual void Start() override;
    virtual void Update(double deltaTime) override;
    virtual void FixedUpdate(double fixedDeltaTime) override;
    virtual void End() override;

    // Climber vs hazard: ends the climb at the next fixed step.
    virtual void CollisionCallback(ME::ColliderAABB* a, ME::ColliderAABB* b, ME::CollisionResultAABB* result) override;

    virtual const char* GetDisplayName() const override;

   private:
    enum class State : uint8_t {
        Ready,    // title screen, waiting for the first tap
        Playing,  // climbing
        Dead,     // game-over screen
    };

    enum class Side : uint8_t {
        None,
        Left,
        Right,
    };

    // The side tapped or pressed this frame, if any. The one place that reads input, so touch can replace it.
    Side ReadTap() const;

    void StartClimb();
    void Die();
    void LeapTo(Side side);

    void MoveClimber(float deltaTime);
    void MoveHazards(float scroll, float deltaTime);
    void SpawnHazard();

    float ClimbSpeed() const;
    float SpawnGap() const;
    uint32_t Meters() const;

    void LoadBest();
    void SaveBest() const;

    static constexpr float START_SPEED = 40.0f;  // world units per second; 8 units is one metre
    static constexpr float MAX_SPEED = 100.0f;
    static constexpr float SPEED_PER_METER = 0.25f;
    static constexpr float START_SPAWN_GAP = 64.0f;  // world units climbed between hazards
    static constexpr float MIN_SPAWN_GAP = 32.0f;
    static constexpr float SPAWN_GAP_PER_METER = 0.15f;
    static constexpr uint32_t DROPPER_FROM_METERS = 50;  // anvils start falling above this height
    static constexpr uint32_t DROPPER_CHANCE = 25;       // percent of spawns
    static constexpr float LEAP_SECONDS = 0.18f;
    static constexpr float LEAP_ARC = 6.0f;          // world units the leap rises at its middle
    static constexpr double RETRY_DELAY_SECONDS = 0.5;  // ignores taps right after dying
    static constexpr const char* SAVE_PATH = "tower_climb/best.bin";

    SceneTowerClimb* towerScene = nullptr;
    SceneUITowerClimb* towerUI = nullptr;
    ME::PhysicsScene* physicsScene = nullptr;

    Hazard hazards[SceneTowerClimb::MAX_HAZARDS];
    ME::Random hazardRandom{"towerHazards", true};

    State state = State::Ready;
    Side wall = Side::Left;
    Side leapTarget = Side::None;  // None while clinging
    float leapTime = 0.0f;
    float climbTime = 0.0f;        // drives the clinging bob
    double climbed = 0.0;          // world units climbed this run
    float untilNextSpawn = 0.0f;   // world units left before the next hazard
    double deadTime = 0.0;
    uint32_t bestMeters = 0;
    bool bHit = false;  // set by the physics callback, applied in the next FixedUpdate
};

}  // namespace Samples
