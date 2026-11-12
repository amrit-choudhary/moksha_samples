#include "particle_life/game/game_particle_life.h"

#include <chrono>
#include <cmath>
#include <cstdio>

#include "moksha/client/app/engine.h"
#include "moksha/client/input/input_manager.h"
#include "moksha/logging/logging.h"
#include "moksha/shared/time/time_manager.h"

namespace {

// The particle counts - and + step through. The top one is ParticleLifeSim::MAX_PARTICLES.
constexpr uint32_t COUNTS[] = {1'000,  2'000,  3'000,  5'000,  7'500,  10'000, 15'000,
                               20'000, 30'000, 40'000, 50'000, 75'000, 100'000};
constexpr size_t COUNT_STEPS = sizeof(COUNTS) / sizeof(COUNTS[0]);
static_assert(COUNTS[COUNT_STEPS - 1] == Samples::ParticleLifeSim::MAX_PARTICLES);

constexpr uint32_t SPAWN_SEED = 7;

}  // namespace

void Samples::GameParticleLife::Init(ME::Engine& inEngine) {
    Game::Init(inEngine);

    lifeScene = new SceneParticleLife();
    scene = lifeScene;
    scene->Init();
    const float aspect = static_cast<float>(engine->window->width) / static_cast<float>(engine->window->height);
    scene->spriteCamera->aspectRatio = aspect;

    lifeUI = new SceneUIParticleLife();
    uiScene = lifeUI;
    uiScene->Init();

    // The world is exactly the view.
    const float height = 2.0f * SceneParticleLife::VIEW_HALF_HEIGHT;
    if (!sim.Init(height * aspect, height)) {
        ME::LogError("Particle Life: bad window size ", engine->window->width, " x ", engine->window->height);
    }
    sim.RandomizeRules(ruleSeed);
    bBenchmark = engine->IsHeadless();
    if (bBenchmark) {
        countIndex = 0;
        ME::Log("Particle Life benchmark: ", BENCH_STEPS, " steps of 1/60 s per count, simulation only");
    }
    ApplyCount();

    ME::Delegate::Bind<GameParticleLife, &GameParticleLife::OnCountDown>(lifeUI->GetDownButton()->onClick, this);
    ME::Delegate::Bind<GameParticleLife, &GameParticleLife::OnCountUp>(lifeUI->GetUpButton()->onClick, this);
}

void Samples::GameParticleLife::Update(double deltaTime) {
    Game::Update(deltaTime);

    using Key = ME::Input::KeyCode;
    if (engine->input->GetKeyPressed(Key::Escape)) {
        engine->Quit();
    }
    if (engine->input->GetKeyPressed(Key::Minus) || engine->input->GetKeyPressed(Key::Subtract) ||
        engine->input->GetKeyPressed(Key::DownArrow)) {
        OnCountDown();
    }
    if (engine->input->GetKeyPressed(Key::Equal) || engine->input->GetKeyPressed(Key::Add) ||
        engine->input->GetKeyPressed(Key::UpArrow)) {
        OnCountUp();
    }
    if (engine->input->GetKeyPressed(Key::R)) {
        NextRules();
    }

    if (bBenchmark) {
        Benchmark();
        return;
    }

    ruleTime += deltaTime;
    if (ruleTime >= RULE_PERIOD) {
        NextRules();
    }

    // One step per frame, so a slow machine shows a slower world rather than piling up steps.
    const auto start = std::chrono::steady_clock::now();
    sim.Step(static_cast<float>(deltaTime) * SIM_SPEED);
    const auto stop = std::chrono::steady_clock::now();
    lifeScene->Sync(sim);

    simTimeSum += std::chrono::duration<double, std::milli>(stop - start).count();
    ++simSteps;
    infoTime += deltaTime;
    if (infoTime >= INFO_PERIOD) {
        lifeUI->SetInfo(sim.GetCount(), simTimeSum / simSteps);
        infoTime = 0.0;
        simTimeSum = 0.0;
        simSteps = 0;
    }

    const double fps = engine->time->GetCurrentFPS();
    const int32_t roundedFps = static_cast<int32_t>(std::lround(fps));
    if (roundedFps != shownFps) {
        shownFps = roundedFps;
        lifeUI->SetFps(fps);
    }
}

void Samples::GameParticleLife::End() {
    delete scene;
    scene = nullptr;
    lifeScene = nullptr;
    delete uiScene;
    uiScene = nullptr;
    lifeUI = nullptr;

    Game::End();
}

const char* Samples::GameParticleLife::GetDisplayName() const {
    return "Particle Life";
}

void Samples::GameParticleLife::OnCountDown() {
    if (countIndex > 0) {
        --countIndex;
        ApplyCount();
    }
}

void Samples::GameParticleLife::OnCountUp() {
    if (countIndex + 1 < COUNT_STEPS) {
        ++countIndex;
        ApplyCount();
    }
}

void Samples::GameParticleLife::ApplyCount() {
    sim.Reset(COUNTS[countIndex], SPAWN_SEED);
    lifeScene->Sync(sim);
    lifeUI->SetInfo(sim.GetCount(), 0.0);
    infoTime = 0.0;
    simTimeSum = 0.0;
    simSteps = 0;
    ME::Log("Particle Life: ", sim.GetCount(), " particles, radius ", sim.GetRadius());
}

void Samples::GameParticleLife::Benchmark() {
    // Fixed steps, so every machine simulates the same world and only the time differs.
    const auto start = std::chrono::steady_clock::now();
    sim.Step(1.0f / 60.0f);
    const auto stop = std::chrono::steady_clock::now();
    simTimeSum += std::chrono::duration<double, std::milli>(stop - start).count();
    ++simSteps;
    if (simSteps < BENCH_STEPS) {
        return;
    }

    char line[96];
    snprintf(line, sizeof(line), "%6u particles: sim %7.2f ms per step, checksum %08X", sim.GetCount(),
             simTimeSum / simSteps, sim.Checksum());
    ME::Log("Particle Life benchmark: ", line);
    if (countIndex + 1 < COUNT_STEPS) {
        OnCountUp();
    } else {
        engine->Quit();
    }
}

void Samples::GameParticleLife::NextRules() {
    ++ruleSeed;
    sim.RandomizeRules(ruleSeed);
    ruleTime = 0.0;
    ME::Log("Particle Life: rules ", ruleSeed);
}
