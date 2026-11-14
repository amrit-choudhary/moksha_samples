#include "colony/game/game_colony.h"

#include <cmath>

#include "moksha/client/app/engine.h"
#include "moksha/client/input/input_manager.h"
#include "moksha/logging/logging.h"
#include "moksha/shared/time/time_manager.h"

static constexpr const char* FIRST_SCENARIO = "scenarios/open.json";

// The design's ant ladder (section 10). The top one is ColonyAnts::MAX_ANTS.
static constexpr uint32_t ANT_LADDER[] = {1'000, 2'000, 4'000, 8'000, 12'000, 20'000, 40'000, 70'000, 100'000};
static constexpr int32_t ANT_RUNGS = static_cast<int32_t>(sizeof(ANT_LADDER) / sizeof(ANT_LADDER[0]));
static_assert(ANT_LADDER[ANT_RUNGS - 1] == Samples::ColonyAnts::MAX_ANTS);

void Samples::GameColony::Init(ME::Engine& inEngine) {
    Game::Init(inEngine);

    colonyScene = new SceneColony();
    scene = colonyScene;
    scene->Init();
    const float aspect = static_cast<float>(engine->window->width) / static_cast<float>(engine->window->height);
    scene->spriteCamera->aspectRatio = aspect;

    colonyUI = new SceneUIColony();
    uiScene = colonyUI;
    uiScene->Init();

    sim.Init();
    StartScenario(FIRST_SCENARIO);

    bBenchmark = engine->IsHeadless();
}

void Samples::GameColony::Update(double deltaTime) {
    Game::Update(deltaTime);

    if (bBenchmark) {
        // No benchmark list yet: nothing to run.
        ME::Log("Colony benchmark: nothing to run yet");
        engine->Quit();
        return;
    }

    using Key = ME::Input::KeyCode;
    if (engine->input->GetKeyPressed(Key::Escape)) {
        engine->Quit();
    }
    if (engine->input->GetKeyPressed(Key::Minus) || engine->input->GetKeyPressed(Key::Subtract)) {
        StepAntTarget(-1);
    }
    if (engine->input->GetKeyPressed(Key::Equal) || engine->input->GetKeyPressed(Key::Add)) {
        StepAntTarget(1);
    }
    const float wheel = engine->input->GetMouseWheelDelta();
    if (wheel != 0.0f) {
        const ME::Vec2 pointer = engine->input->GetMouseWorldPosNorm();
        colonyScene->Zoom(wheel, pointer.x, pointer.y);
    }
    // WASD pans; held keys move the view PAN_SPEED view heights per second.
    float panX = 0.0f;
    float panY = 0.0f;
    panX -= engine->input->GetKeyDown(Key::A) ? 1.0f : 0.0f;
    panX += engine->input->GetKeyDown(Key::D) ? 1.0f : 0.0f;
    panY -= engine->input->GetKeyDown(Key::S) ? 1.0f : 0.0f;
    panY += engine->input->GetKeyDown(Key::W) ? 1.0f : 0.0f;
    if (panX != 0.0f || panY != 0.0f) {
        const float distance = PAN_SPEED * static_cast<float>(deltaTime);
        colonyScene->Pan(panX * distance, panY * distance);
    }
    if (engine->input->GetKeyPressed(Key::C)) {
        ColonySettings& settings = sim.GetSettings();
        settings.bCollisions = !settings.bCollisions;
        ME::Log("Colony: collisions ", settings.bCollisions ? "on" : "off");
    }

    // A set number of steps per frame, not the engine's fixed step, so the load doesn't grow as frames slow down.
    for (uint32_t i = 0; i < STEPS_PER_FRAME; ++i) {
        sim.Step();
    }
    if (sim.GetInvariantFailure() != nullptr && !bInvariantLogged) {
        bInvariantLogged = true;
        ME::LogError("Colony: invariant failed at step ", sim.GetStepIndex(), ": ", sim.GetInvariantFailure());
    }
    colonyScene->SyncCells(sim.GetWorld());
    colonyScene->SyncAnts(sim.GetAnts());

    const double fps = engine->time->GetCurrentFPS();
    const int32_t roundedFps = static_cast<int32_t>(std::lround(fps));
    if (roundedFps != shownFps) {
        shownFps = roundedFps;
        colonyUI->SetFps(fps);
    }
}

void Samples::GameColony::End() {
    delete scene;
    scene = nullptr;
    colonyScene = nullptr;
    delete uiScene;
    uiScene = nullptr;
    colonyUI = nullptr;

    Game::End();
}

const char* Samples::GameColony::GetDisplayName() const {
    return "Colony";
}

void Samples::GameColony::StartScenario(const char* path) {
    ColonyWorld& world = sim.GetWorld();
    if (!scenario.Load(path)) {
        world.Clear();
        sim.Start(0, 1);
        return;
    }
    scenario.Build(world);
    sim.Start(scenario.ants, scenario.seed);
    bInvariantLogged = false;
    ME::Log("Colony: scenario ", scenario.name, ", ", scenario.wallCount, " walls, ", world.CountFood(), " food, ",
            sim.GetAnts().GetCount(), " ants, seed ", scenario.seed);
}

void Samples::GameColony::StepAntTarget(int32_t direction) {
    // The nearest rung at or above the current target, then one step.
    ColonySettings& settings = sim.GetSettings();
    int32_t rung = 0;
    while (rung < ANT_RUNGS - 1 && ANT_LADDER[rung] < settings.antTarget) {
        ++rung;
    }
    if (direction < 0 && rung > 0 && ANT_LADDER[rung] >= settings.antTarget) {
        --rung;
    } else if (direction > 0 && ANT_LADDER[rung] <= settings.antTarget && rung < ANT_RUNGS - 1) {
        ++rung;
    }
    settings.antTarget = ANT_LADDER[rung];
    const ColonyNest& nest = sim.GetWorld().GetNest(0);
    ME::Log("Colony: ant target ", settings.antTarget, " (now ", sim.GetAnts().GetCount(), "; stock ", nest.stock,
            ", spent ", nest.foodSpent, ", free spawns ", nest.freeSpawns, ")");
}
