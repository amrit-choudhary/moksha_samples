#pragma once

/**
 * Systems check: runs the engine systems that need no graphics and logs a PASS/FAIL/SKIP line for each. It draws
 * nothing, so it is the first sample to bring up on a platform whose renderer isn't ported yet.
 *
 * Automatic checks (all builds), reported after REPORT_SECONDS: config (settings.ini applied), time (fixed step
 * rate), timers, files (a run counter saved in user data), random and fixed point (a checksum to compare across
 * platforms), physics (layered collision) and network (connect and ping Samples_Server). The headless build
 * (Sample_SystemsCheck_Headless) then quits. Windowed, the interactive checks stay open: keys and mouse buttons
 * are logged, a beep plays at start and on Space, and Escape logs the final report and quits.
 */

#include <cstddef>
#include <cstdint>

#include "moksha/client/game/game.h"
#include "moksha/shared/physics/physics_scene.h"
#include "moksha/shared/time/timer_manager.h"
#include "systems_check/scene/scene_systems_check.h"

namespace Samples {

class GameSystemsCheck : public ME::Game {
   public:
    GameSystemsCheck();
    ~GameSystemsCheck();

    virtual void Init(ME::Engine& inEngine) override;
    virtual void Start() override;
    virtual void Update(double deltaTime) override;
    virtual void FixedUpdate(double fixedDeltaTime) override;
    virtual void End() override;

    virtual void CollisionCallback(ME::ColliderAABB* a, ME::ColliderAABB* b, ME::CollisionResultAABB* result) override;

    void OnConnected();
    void OnPong();
    void OnTimeoutTimer();
    void OnIntervalTimer();
    void OnClearedTimer();
    void OnReportTimer();

    // The window title settings.ini sets; the config check expects it.
    static constexpr const char* INI_WINDOW_TITLE = "Moksha Systems Check";

   private:
    enum class Check : uint8_t {
        Config = 0,
        Time,
        Timers,
        Files,
        Random,
        Physics,
        Network,
        Input,
        Audio,
        Count
    };

    enum class Result : uint8_t {
        Waiting = 0,  // not decided yet
        Pass = 1,     // worked
        Fail = 2,     // ran and gave a wrong result
        Skip = 3,     // not available in this build
        Listen = 4,   // ran, but only a person can tell (audio)
    };

    void SetResult(Check check, Result result, const char* note);

    // Checks that finish in Init.
    void CheckConfig();
    void CheckFiles();
    void CheckRandom();

    // Checks decided when the report timer fires.
    void FinishTime();
    void FinishTimers();
    void FinishPhysics();
    void FinishNetwork();

    void PollInput();
    void LogReport(const char* heading);

    SceneSystemsCheck* systemsScene = nullptr;
    ME::PhysicsScene* physicsScene = nullptr;

    static constexpr size_t CHECK_COUNT = static_cast<size_t>(Check::Count);
    static constexpr size_t NOTE_SIZE = 96;
    Result results[CHECK_COUNT] = {};
    char notes[CHECK_COUNT][NOTE_SIZE] = {};

    static constexpr double REPORT_SECONDS = 4.0;
    ME::Time::TimerHandle reportTimer;
    bool bReported = false;

    // Time: fixed steps counted here against real time since Start.
    uint64_t fixedSteps = 0;
    double startTime = 0.0;

    // Timers: a one-shot, an interval that clears itself after INTERVAL_REPEATS, and a timeout cleared at once.
    static constexpr double TIMEOUT_SECONDS = 0.5;
    static constexpr double INTERVAL_SECONDS = 0.25;
    static constexpr uint32_t INTERVAL_REPEATS = 4;
    ME::Time::TimerHandle timeoutTimer;
    ME::Time::TimerHandle intervalTimer;
    ME::Time::TimerHandle clearedTimer;
    uint32_t timeoutFires = 0;
    uint32_t intervalFires = 0;
    uint32_t clearedFires = 0;
    uint64_t firstIntervalStep = 0;
    uint64_t lastIntervalStep = 0;

    // Physics: the mover slides into the target; only that pair may be reported.
    static constexpr float MOVER_SPEED = 4.0f;  // units per second
    bool bMoverStopped = false;
    uint32_t targetHits = 0;
    uint32_t decoyHits = 0;
    uint64_t firstHitStep = 0;

    // Network: one ping after connecting.
    bool bConnected = false;
    bool bPongReceived = false;
    double pingSentAt = 0.0;
    double pingRtt = 0.0;

    // Input and audio (windowed only).
    uint32_t keysPressed = 0;
    uint32_t mouseClicks = 0;
};

}  // namespace Samples
