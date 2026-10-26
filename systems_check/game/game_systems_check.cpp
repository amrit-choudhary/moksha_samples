#include "systems_check/game/game_systems_check.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "moksha/client/app/app_config.h"
#include "moksha/client/app/engine.h"
#include "moksha/client/audio/audio_system.h"
#include "moksha/client/input/input_manager.h"
#include "moksha/client/net/game_client.h"
#include "moksha/logging/logging.h"
#include "moksha/shared/file_io/vfs.h"
#include "moksha/shared/math/fp_24_8.h"
#include "moksha/shared/misc/delegate.h"
#include "moksha/shared/physics/physics_system.h"
#include "moksha/shared/random/random_engine.h"
#include "moksha/shared/serialization/byte_reader.h"
#include "moksha/shared/serialization/byte_writer.h"

using ME::Input::InputManager;
using ME::Input::KeyCode;

static constexpr const char* CHECK_NAMES[] = {"Config",  "Time",    "Timers", "Files", "Random",
                                              "Physics", "Network", "Input",  "Audio"};
static constexpr const char* RESULT_NAMES[] = {"WAIT", "PASS", "FAIL", "SKIP", "LISTEN"};

// Run counter kept in user data, so a second launch shows the file survived.
static constexpr const char* SAVE_PATH = "systems_check/state.bin";
static constexpr uint32_t SAVE_MAGIC = 0x4B434853;  // "SHCK"

// Fixed seed: every platform must log the same random checksum and fixed-point sum.
static constexpr uint32_t RANDOM_SEED = 12345u;
static constexpr int RANDOM_ROUNDS = 1000;

Samples::GameSystemsCheck::GameSystemsCheck() : Game() {}

Samples::GameSystemsCheck::~GameSystemsCheck() {}

void Samples::GameSystemsCheck::Init(ME::Engine& inEngine) {
    Game::Init(inEngine);

    // Nothing is drawn: the scene only carries colliders and a sound, and the UI scene stays empty.
    systemsScene = new SceneSystemsCheck();
    scene = systemsScene;
    scene->Init();

    uiScene = new ME::SceneUI();
    uiScene->Init();

    physicsScene = new ME::PhysicsScene();
    physicsScene->Init(scene->staticColliders, scene->dynamicColliders);
    engine->physics->SetCollisionListener(this);
    engine->physics->SetScene(physicsScene);

    ME::GameClient* gameClient = engine->gameClient;
    ME::Delegate::Bind<GameSystemsCheck, &GameSystemsCheck::OnConnected>(gameClient->onConnected, this);
    ME::Delegate::Bind<GameSystemsCheck, &GameSystemsCheck::OnPong>(gameClient->onPong, this);
    gameClient->SendConnectRequest();

    CheckConfig();
    CheckFiles();
    CheckRandom();

    if (engine->IsHeadless()) {
        SetResult(Check::Input, Result::Skip, "no input in the headless build");
        SetResult(Check::Audio, Result::Skip, "no audio device in the headless build");
    } else {
        SetResult(Check::Input, Result::Waiting, "press any key or mouse button");
    }

    ME::Log("Systems Check Start!", engine->IsHeadless() ? " (headless)" : "");
}

void Samples::GameSystemsCheck::Start() {
    Game::Start();
    startTime = engine->time->GetTimeSinceStartup();

    ME::Time::TimerManager* timers = engine->timers;
    timeoutTimer = timers->SetTimeout(ME::Delegate::Make<GameSystemsCheck, &GameSystemsCheck::OnTimeoutTimer>(this),
                                      TIMEOUT_SECONDS);
    intervalTimer = timers->SetInterval(ME::Delegate::Make<GameSystemsCheck, &GameSystemsCheck::OnIntervalTimer>(this),
                                        INTERVAL_SECONDS);
    clearedTimer = timers->SetTimeout(ME::Delegate::Make<GameSystemsCheck, &GameSystemsCheck::OnClearedTimer>(this),
                                      TIMEOUT_SECONDS);
    timers->Clear(clearedTimer);
    reportTimer = timers->SetTimeout(ME::Delegate::Make<GameSystemsCheck, &GameSystemsCheck::OnReportTimer>(this),
                                     REPORT_SECONDS);

    if (!engine->IsHeadless()) {
        engine->audio->PlayAudio(SystemsCheckSfx::BEEP, false);
        SetResult(Check::Audio, Result::Listen, "a beep played at start; Space plays it again");
    }
}

void Samples::GameSystemsCheck::Update(double deltaTime) {
    Game::Update(deltaTime);

    if (!engine->IsHeadless()) {
        PollInput();
    }
}

void Samples::GameSystemsCheck::FixedUpdate(double fixedDeltaTime) {
    Game::FixedUpdate(fixedDeltaTime);
    ++fixedSteps;

    // Physics runs right after this step, against the moved collider.
    if (!bMoverStopped) {
        systemsScene->MoveMover(MOVER_SPEED * static_cast<float>(fixedDeltaTime));
    }
}

void Samples::GameSystemsCheck::CollisionCallback(ME::ColliderAABB* a, ME::ColliderAABB* b,
                                                  ME::CollisionResultAABB* result) {
    delete result;

    const ME::ColliderAABB* colliders = scene->dynamicColliders.data;
    const size_t indexA = static_cast<size_t>(a - colliders);
    const size_t indexB = static_cast<size_t>(b - colliders);

    if (indexA == SceneSystemsCheck::DECOY_COLLIDER || indexB == SceneSystemsCheck::DECOY_COLLIDER) {
        ++decoyHits;
        return;
    }
    if (targetHits == 0) {
        firstHitStep = fixedSteps;
    }
    ++targetHits;
    bMoverStopped = true;
}

void Samples::GameSystemsCheck::OnConnected() {
    bConnected = true;
    engine->gameClient->SendPing();
    pingSentAt = engine->time->GetTimeSinceStartup();
}

void Samples::GameSystemsCheck::OnPong() {
    if (bPongReceived) {
        return;
    }
    bPongReceived = true;
    pingRtt = engine->time->GetTimeSinceStartup() - pingSentAt;

    // A pong after the report updates the final one.
    if (bReported) {
        FinishNetwork();
    }
}

void Samples::GameSystemsCheck::OnTimeoutTimer() {
    ++timeoutFires;
}

void Samples::GameSystemsCheck::OnIntervalTimer() {
    ++intervalFires;
    if (intervalFires == 1) {
        firstIntervalStep = fixedSteps;
    }
    lastIntervalStep = fixedSteps;
    if (intervalFires == INTERVAL_REPEATS) {
        engine->timers->Clear(intervalTimer);
    }
}

void Samples::GameSystemsCheck::OnClearedTimer() {
    ++clearedFires;
}

void Samples::GameSystemsCheck::OnReportTimer() {
    FinishTime();
    FinishTimers();
    FinishPhysics();
    FinishNetwork();
    bReported = true;

    LogReport("SYSTEMS CHECK");
    if (engine->IsHeadless()) {
        engine->Quit();
    } else {
        ME::Log("Interactive checks: press keys and mouse buttons, Space beeps, Escape quits.");
    }
}

void Samples::GameSystemsCheck::SetResult(Check check, Result result, const char* note) {
    const size_t index = static_cast<size_t>(check);
    results[index] = result;
    snprintf(notes[index], NOTE_SIZE, "%s", note);
}

void Samples::GameSystemsCheck::CheckConfig() {
    std::string ini;
    const bool bIniRead = ME::Vfs::ReadText(ME::FileRoot::Resources, "config/settings.ini", ini);
    const ME::AppConfig* config = engine->config;
    const bool bTitle = config->windowTitle == INI_WINDOW_TITLE;

    char note[NOTE_SIZE];
    snprintf(note, sizeof(note), "settings.ini %s, title \"%s\", window %ux%u, fixed step %.0f Hz",
             bIniRead ? "read" : "missing", config->windowTitle.c_str(), static_cast<unsigned>(config->windowWidth),
             static_cast<unsigned>(config->windowHeight), config->fixedStepFPS);
    SetResult(Check::Config, bIniRead && bTitle ? Result::Pass : Result::Fail, note);
}

void Samples::GameSystemsCheck::CheckFiles() {
    uint8_t buffer[16];
    size_t size = 0;
    uint32_t runCount = 0;
    if (ME::Vfs::ReadBytes(ME::FileRoot::UserData, SAVE_PATH, buffer, sizeof(buffer), size)) {
        ME::ByteReader reader(buffer, size);
        uint32_t magic = 0;
        if (reader.ReadU32(magic) && magic == SAVE_MAGIC) {
            reader.ReadU32(runCount);
        }
    }
    ++runCount;

    ME::ByteWriter writer(buffer, sizeof(buffer));
    writer.WriteU32(SAVE_MAGIC);
    writer.WriteU32(runCount);
    const bool bWritten = ME::Vfs::WriteBytes(ME::FileRoot::UserData, SAVE_PATH, buffer, writer.GetSize());

    uint8_t readBack[16];
    size_t readSize = 0;
    const bool bReadBack =
        ME::Vfs::ReadBytes(ME::FileRoot::UserData, SAVE_PATH, readBack, sizeof(readBack), readSize) &&
        readSize == writer.GetSize() && std::memcmp(buffer, readBack, readSize) == 0;

    char note[NOTE_SIZE];
    snprintf(note, sizeof(note), "run #%u (launch again to see it count up), %s", runCount,
             !bWritten ? "write failed" : (bReadBack ? "read back intact" : "read back differs"));
    SetResult(Check::Files, bWritten && bReadBack ? Result::Pass : Result::Fail, note);
}

void Samples::GameSystemsCheck::CheckRandom() {
    ME::Random a(RANDOM_SEED);
    ME::Random b(RANDOM_SEED);
    bool bSame = true;
    bool bInRange = true;
    uint32_t checksum = 0;
    ME::FP_24_8 fpSum(0);

    for (int i = 0; i < RANDOM_ROUNDS; ++i) {
        const uint32_t value = a.Next();
        bSame = bSame && value == b.Next();
        checksum = ((checksum << 5) | (checksum >> 27)) ^ value;

        const uint32_t roll = a.NextRange(10, 20);
        bSame = bSame && roll == b.NextRange(10, 20);
        bInRange = bInRange && roll >= 10 && roll <= 20;
        fpSum += ME::FP_24_8(static_cast<int32_t>(roll)) / ME::FP_24_8(3);
    }

    char note[NOTE_SIZE];
    snprintf(note, sizeof(note), "checksum %08X, fixed-point sum raw %d (same on every platform)", checksum,
             fpSum.GetRaw());
    SetResult(Check::Random, bSame && bInRange ? Result::Pass : Result::Fail, note);
}

void Samples::GameSystemsCheck::FinishTime() {
    const double elapsed = engine->time->GetTimeSinceStartup() - startTime;
    const double expected = elapsed * engine->time->GetFixedStepFPS();
    const double ratio = expected > 0.0 ? static_cast<double>(fixedSteps) / expected : 0.0;

    char note[NOTE_SIZE];
    snprintf(note, sizeof(note), "%llu fixed steps in %.2f s (expected %.0f), %llu frames",
             static_cast<unsigned long long>(fixedSteps), elapsed, expected,
             static_cast<unsigned long long>(engine->time->GetFrameCount()));
    SetResult(Check::Time, ratio > 0.9 && ratio < 1.1 ? Result::Pass : Result::Fail, note);
}

void Samples::GameSystemsCheck::FinishTimers() {
    // Same rounding as TimerManager: whole fixed steps, at least one.
    const double intervalSteps = INTERVAL_SECONDS * engine->time->GetFixedStepFPS() + 0.5;
    const uint64_t stepsPerInterval = intervalSteps < 1.0 ? 1 : static_cast<uint64_t>(intervalSteps);
    const uint64_t expectedSpan = (INTERVAL_REPEATS - 1) * stepsPerInterval;
    const uint64_t span = lastIntervalStep - firstIntervalStep;

    const bool bPass = timeoutFires == 1 && intervalFires == INTERVAL_REPEATS && clearedFires == 0 &&
                       span == expectedSpan && !engine->timers->IsActive(intervalTimer);

    char note[NOTE_SIZE];
    snprintf(note, sizeof(note), "timeout x%u, interval x%u over %llu steps (expected %llu), cleared x%u", timeoutFires,
             intervalFires, static_cast<unsigned long long>(span), static_cast<unsigned long long>(expectedSpan),
             clearedFires);
    SetResult(Check::Timers, bPass ? Result::Pass : Result::Fail, note);
}

void Samples::GameSystemsCheck::FinishPhysics() {
    const float gap = SceneSystemsCheck::TARGET_X - SceneSystemsCheck::MOVER_START_X - 1.0f;
    const double expectedSteps = gap / MOVER_SPEED * engine->time->GetFixedStepFPS();

    char note[NOTE_SIZE];
    if (targetHits == 0) {
        snprintf(note, sizeof(note), "mover never hit the target (expected after ~%.0f steps)", expectedSteps);
    } else {
        snprintf(note, sizeof(note), "hit after %llu steps (expected ~%.0f), decoy reports %u",
                 static_cast<unsigned long long>(firstHitStep), expectedSteps, decoyHits);
    }
    SetResult(Check::Physics, targetHits > 0 && decoyHits == 0 ? Result::Pass : Result::Fail, note);
}

void Samples::GameSystemsCheck::FinishNetwork() {
    const ME::AppConfig* config = engine->config;
    char note[NOTE_SIZE];
    if (bPongReceived) {
        snprintf(note, sizeof(note), "client %d on %s:%u, ping %.1f ms", engine->gameClient->GetClientID(),
                 config->serverIP.c_str(), static_cast<unsigned>(config->serverPort), pingRtt * 1000.0);
    } else if (bConnected) {
        snprintf(note, sizeof(note), "connected to %s:%u, no pong yet", config->serverIP.c_str(),
                 static_cast<unsigned>(config->serverPort));
    } else {
        snprintf(note, sizeof(note), "no reply from %s:%u (is Samples_Server running?)", config->serverIP.c_str(),
                 static_cast<unsigned>(config->serverPort));
    }
    SetResult(Check::Network, bPongReceived ? Result::Pass : Result::Fail, note);
}

void Samples::GameSystemsCheck::PollInput() {
    const uint16_t firstKey = static_cast<uint16_t>(KeyCode::Backspace);
    const uint16_t lastKey = static_cast<uint16_t>(KeyCode::Apostrophe);
    for (uint16_t code = firstKey; code <= lastKey; ++code) {
        if (InputManager::GetKeyPressed(static_cast<KeyCode>(code))) {
            ++keysPressed;
            ME::Log("Input: key ", code, " pressed");
        }
    }
    for (uint8_t button = 0; button <= static_cast<uint8_t>(ME::Input::MouseButton::Button5); ++button) {
        if (InputManager::GetMouseButtonPressed(static_cast<ME::Input::MouseButton>(button))) {
            ++mouseClicks;
            const ME::Vec2i pos = InputManager::GetMousePos();
            ME::Log("Input: mouse button ", static_cast<int>(button), " at ", pos.x, ", ", pos.y);
        }
    }

    if (keysPressed + mouseClicks > 0) {
        char note[NOTE_SIZE];
        snprintf(note, sizeof(note), "%u key presses, %u mouse clicks", keysPressed, mouseClicks);
        SetResult(Check::Input, Result::Pass, note);
    }

    if (InputManager::GetKeyPressed(KeyCode::Space)) {
        engine->audio->PlayAudio(SystemsCheckSfx::BEEP, false);
    }
    if (InputManager::GetKeyPressed(KeyCode::Escape)) {
        engine->Quit();
    }
}

void Samples::GameSystemsCheck::LogReport(const char* heading) {
    ME::Log("==== ", heading, " ====");
    uint32_t counts[5] = {};
    for (size_t i = 0; i < CHECK_COUNT; ++i) {
        const uint8_t result = static_cast<uint8_t>(results[i]);
        ++counts[result];

        char line[160];
        snprintf(line, sizeof(line), "%-8s %-6s %s", CHECK_NAMES[i], RESULT_NAMES[result], notes[i]);
        if (results[i] == Result::Pass) {
            ME::LogSuccess(line);
        } else if (results[i] == Result::Fail) {
            ME::LogError(line);
        } else {
            ME::Log(line);
        }
    }

    char summary[96];
    snprintf(summary, sizeof(summary), "pass %u, fail %u, skip %u, listen %u, waiting %u", counts[1], counts[2],
             counts[3], counts[4], counts[0]);
    ME::Log(summary);
}

void Samples::GameSystemsCheck::End() {
    // Headless already logged the report right before quitting.
    if (!engine->IsHeadless() && bReported) {
        LogReport("SYSTEMS CHECK (final)");
    }

    engine->timers->Clear(reportTimer);
    engine->timers->Clear(timeoutTimer);
    engine->timers->Clear(intervalTimer);
    engine->timers->Clear(clearedTimer);

    delete uiScene;
    delete scene;
    delete physicsScene;
    physicsScene = nullptr;

    Game::End();
    ME::Log("Systems Check End!");
}
