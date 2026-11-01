#include "graphics_stress/game/game_graphics_stress.h"

#include <cmath>
#include <cstdio>

#include "moksha/client/app/engine.h"
#include "moksha/client/input/input_manager.h"
#include "moksha/shared/math/math.h"
#include "moksha/shared/time/time_manager.h"

namespace {

enum Load : size_t {
    STARS,
    MOVING_STARS,
    CLOUDS,
    CLOUD_SIZE,
    UI_PANELS,
    UI_TEXT,
    LOAD_COUNT,
};

class LoadSetting {
   public:
    const char* name;
    const char* suffix;
    uint32_t min;
    uint32_t start;
    uint32_t minStep;  // the step grows tenfold as the value passes 20 steps
    uint32_t max;
};

// In Load order. Moving stars can exceed the stars shown; only shown ones move.
constexpr LoadSetting LOADS[] = {
    {"Stars", "", 0, 10'000, 1'000, Samples::SceneGraphicsStress::MAX_STARS},
    {"Moving stars", "", 0, 5'000, 1'000, Samples::SceneGraphicsStress::MAX_STARS},
    {"Clouds", "", 0, 100, 25, Samples::SceneGraphicsStress::MAX_CLOUDS},
    // Top size: nearly every cloud fills the screen. Larger risks frames past the renderer's 1 s GPU wait.
    {"Cloud size", "%", 100, 100, 50, 400},
    {"UI panels", "", 0, 0, 20, Samples::SceneUIGraphicsStress::MAX_STRESS_PANELS},
    {"UI text", "", 0, 0, 20, Samples::SceneUIGraphicsStress::MAX_STRESS_LABELS},
};
static_assert(sizeof(LOADS) / sizeof(LOADS[0]) == LOAD_COUNT);
static_assert(LOAD_COUNT == Samples::SceneUIGraphicsStress::ROW_COUNT);

// The step that applies above value: minStep, ten times that from 20 steps up, and so on.
uint32_t StepAbove(uint32_t value, uint32_t minStep) {
    uint32_t step = minStep;
    while (step * 20 <= value) {
        step *= 10;
    }
    return step;
}

uint32_t StepUp(uint32_t value, const LoadSetting& setting) {
    const uint32_t raised = value + StepAbove(value, setting.minStep);
    return raised < setting.max ? raised : setting.max;
}

// Undoes StepUp: the step below value is the one that reached it.
uint32_t StepDown(uint32_t value, const LoadSetting& setting) {
    if (value <= setting.min) {
        return setting.min;
    }
    const uint32_t step = StepAbove(value - 1, setting.minStep);
    return value >= setting.min + step ? value - step : setting.min;
}

}  // namespace

void Samples::GameGraphicsStress::Init(ME::Engine& inEngine) {
    Game::Init(inEngine);

    stressScene = new SceneGraphicsStress();
    scene = stressScene;
    scene->Init();
    scene->spriteCamera->aspectRatio =
        static_cast<float>(engine->window->width) / static_cast<float>(engine->window->height);

    stressUI = new SceneUIGraphicsStress();
    uiScene = stressUI;
    uiScene->Init();

    for (size_t i = 0; i < LOAD_COUNT; ++i) {
        loadValues[i] = LOADS[i].start;
        ApplyLoad(i);
    }
    ShowLoads();

    ME::Delegate::Bind<GameGraphicsStress, &GameGraphicsStress::OnSelectPrevious>(
        stressUI->GetPreviousButton()->onClick, this);
    ME::Delegate::Bind<GameGraphicsStress, &GameGraphicsStress::OnSelectNext>(stressUI->GetNextButton()->onClick, this);
    ME::Delegate::Bind<GameGraphicsStress, &GameGraphicsStress::OnLoadDown>(stressUI->GetDownButton()->onClick, this);
    ME::Delegate::Bind<GameGraphicsStress, &GameGraphicsStress::OnLoadUp>(stressUI->GetUpButton()->onClick, this);
}

void Samples::GameGraphicsStress::Update(double deltaTime) {
    Game::Update(deltaTime);

    if (engine->input->GetKeyPressed(ME::Input::KeyCode::Escape)) {
        engine->Quit();
    }

    roll = std::fmod(roll + ROLL_SPEED * deltaTime, ME::TAU);
    stressScene->SetRoll(static_cast<float>(roll));

    const float dt = static_cast<float>(deltaTime);
    stressScene->MoveStars(loadValues[MOVING_STARS], dt);
    stressScene->MoveGiants(dt);

    const double fps = engine->time->GetCurrentFPS();
    const int32_t roundedFps = static_cast<int32_t>(std::lround(fps));
    if (roundedFps != shownFps) {
        shownFps = roundedFps;
        stressUI->SetFps(fps);
    }
}

void Samples::GameGraphicsStress::End() {
    delete scene;
    scene = nullptr;
    stressScene = nullptr;
    delete uiScene;
    uiScene = nullptr;
    stressUI = nullptr;

    Game::End();
}

const char* Samples::GameGraphicsStress::GetDisplayName() const {
    return "Graphics Stress";
}

void Samples::GameGraphicsStress::OnSelectPrevious() {
    selectedLoad = selectedLoad == 0 ? LOAD_COUNT - 1 : selectedLoad - 1;
    ShowLoads();
}

void Samples::GameGraphicsStress::OnSelectNext() {
    selectedLoad = selectedLoad + 1 == LOAD_COUNT ? 0 : selectedLoad + 1;
    ShowLoads();
}

void Samples::GameGraphicsStress::OnLoadDown() {
    loadValues[selectedLoad] = StepDown(loadValues[selectedLoad], LOADS[selectedLoad]);
    ApplyLoad(selectedLoad);
    ShowLoads();
}

void Samples::GameGraphicsStress::OnLoadUp() {
    loadValues[selectedLoad] = StepUp(loadValues[selectedLoad], LOADS[selectedLoad]);
    ApplyLoad(selectedLoad);
    ShowLoads();
}

void Samples::GameGraphicsStress::ApplyLoad(size_t index) {
    const uint32_t value = loadValues[index];
    switch (index) {
        case STARS:
            stressScene->SetStarCount(value);
            break;
        case CLOUDS:
            stressScene->SetCloudCount(value);
            break;
        case CLOUD_SIZE:
            stressScene->SetCloudScale(static_cast<float>(value) / 100.0f);
            break;
        case UI_PANELS:
            stressUI->SetStressPanelCount(value);
            break;
        case UI_TEXT:
            stressUI->SetStressLabelCount(value);
            break;
        default:
            break;  // moving stars are read each frame in Update
    }
}

void Samples::GameGraphicsStress::ShowLoads() {
    char buffer[40];
    for (size_t i = 0; i < LOAD_COUNT; ++i) {
        snprintf(buffer, sizeof(buffer), "%s%s %u%s", i == selectedLoad ? "> " : "  ", LOADS[i].name, loadValues[i],
                 LOADS[i].suffix);
        stressUI->SetRow(i, buffer, i == selectedLoad);
    }
}
