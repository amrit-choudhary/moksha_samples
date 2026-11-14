#include "colony/game/game_colony.h"

#include <chrono>
#include <cmath>
#include <cstdio>

#include "moksha/client/app/engine.h"
#include "moksha/client/input/input_manager.h"
#include "moksha/logging/logging.h"
#include "moksha/shared/time/time_manager.h"

namespace {

using Clock = std::chrono::steady_clock;

// The v1 scenarios, in the order N and the Scenario row step through them.
constexpr const char* SCENARIOS[] = {
    "scenarios/open.json",
    "scenarios/four_food.json",
    "scenarios/wall.json",
    "scenarios/shortcut.json",
};
constexpr uint32_t SCENARIO_COUNT = sizeof(SCENARIOS) / sizeof(SCENARIOS[0]);

// The design's load ladders (section 10). The top ant count is ColonyAnts::MAX_ANTS.
constexpr uint32_t ANT_LADDER[] = {1'000, 2'000, 4'000, 8'000, 12'000, 20'000, 40'000, 70'000, 100'000};
constexpr uint32_t STEPS_LADDER[] = {1, 2, 4, 8, 16};
constexpr uint32_t SAMPLES_LADDER[] = {8, 16, 32, 64};
constexpr float EXPLORER_LADDER[] = {0.0f, 0.1f, 0.3f};
static_assert(ANT_LADDER[sizeof(ANT_LADDER) / sizeof(ANT_LADDER[0]) - 1] == Samples::ColonyAnts::MAX_ANTS);

constexpr const char* ROW_NAMES[] = {"Scenario",  "Ants",       "Steps/frame", "Follower samples",
                                     "Explorers", "Collisions", "Drawing"};

constexpr ME::Color STATS_TEXT{0.92f, 0.92f, 0.92f, 1.0f};
constexpr ME::Color STATS_DIM{0.60f, 0.60f, 0.60f, 1.0f};
constexpr ME::Color STATS_TOP{1.0f, 0.85f, 0.30f, 1.0f};     // the costliest system
constexpr ME::Color STATS_ENGINE{0.55f, 0.80f, 1.0f, 1.0f};  // the derived engine row

// The ladder value one step from current: the nearest rung at or above it, then one step down or up, clamped.
template <typename T, uint32_t N>
T StepLadder(const T (&ladder)[N], T current, int32_t direction) {
    uint32_t rung = 0;
    while (rung + 1 < N && ladder[rung] < current) {
        ++rung;
    }
    if (direction < 0 && rung > 0 && !(current > ladder[rung])) {
        --rung;
    } else if (direction > 0 && !(ladder[rung] > current) && rung + 1 < N) {
        ++rung;
    }
    return ladder[rung];
}

// A count with thousands separated, as 12,000.
void FormatCount(uint32_t value, char* out, size_t capacity) {
    if (value >= 1'000'000) {
        snprintf(out, capacity, "%u,%03u,%03u", value / 1'000'000, (value / 1'000) % 1'000, value % 1'000);
    } else if (value >= 1'000) {
        snprintf(out, capacity, "%u,%03u", value / 1'000, value % 1'000);
    } else {
        snprintf(out, capacity, "%u", value);
    }
}

// A step count as simulated minutes and seconds, as "12:34".
void FormatSimTime(uint32_t step, char* out, size_t capacity) {
    const uint32_t seconds = static_cast<uint32_t>(static_cast<float>(step) * Samples::ColonySim::STEP_TIME);
    snprintf(out, capacity, "%u:%02u", seconds / 60, seconds % 60);
}

float MillisecondsSince(Clock::time_point start) {
    return std::chrono::duration<float, std::milli>(Clock::now() - start).count();
}

int32_t Percent(float share) {
    return static_cast<int32_t>(std::lround(share * 100.0f));
}

}  // namespace

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
    timings.Init(STEP_WINDOW, FRAME_WINDOW);
    bBenchmark = engine->IsHeadless();
    if (bBenchmark) {
        StartBenchmark();
    } else {
        StartScenario(0);
    }

    ME::Delegate::Bind<GameColony, &GameColony::OnSelectPrevious>(colonyUI->GetPreviousButton()->onClick, this);
    ME::Delegate::Bind<GameColony, &GameColony::OnSelectNext>(colonyUI->GetNextButton()->onClick, this);
    ME::Delegate::Bind<GameColony, &GameColony::OnLoadDown>(colonyUI->GetDownButton()->onClick, this);
    ME::Delegate::Bind<GameColony, &GameColony::OnLoadUp>(colonyUI->GetUpButton()->onClick, this);
}

void Samples::GameColony::Update(double deltaTime) {
    Game::Update(deltaTime);

    // This frame's length (since the last Update) holds the game work measured in the last one.
    const Clock::time_point updateStart = Clock::now();
    const float frameMs = bHasLastUpdate
                              ? std::chrono::duration<float, std::milli>(updateStart - lastUpdateStart).count()
                              : static_cast<float>(deltaTime * 1000.0);
    lastUpdateStart = updateStart;
    bHasLastUpdate = true;
    timings.AddFrame(frameMs, lastGameMs, lastAntSpritesMs, lastCellSpritesMs);

    if (!bBenchmark) {
        HandleInput(deltaTime);
    }
    if (!bPaused) {
        RunSteps();
    }

    lastAntSpritesMs = 0.0f;
    lastCellSpritesMs = 0.0f;
    if (bDrawing) {
        const Clock::time_point cellsStart = Clock::now();
        colonyScene->SyncCells(sim.GetWorld());
        lastCellSpritesMs = MillisecondsSince(cellsStart);
        const Clock::time_point antsStart = Clock::now();
        colonyScene->SyncAnts(sim.GetAnts());
        lastAntSpritesMs = MillisecondsSince(antsStart);
    } else {
        colonyScene->HideAll();
    }

    if (bBenchmark) {
        lastGameMs = MillisecondsSince(updateStart);
        AdvanceBenchmark();
        return;
    }

    statsTime += deltaTime;
    if (statsTime >= STATS_PERIOD) {
        statsTime = 0.0;
        RefreshStats();
        RefreshLoadPanel();  // the ant count moves while the colony grows or shrinks
    }
    const double fps = engine->time->GetCurrentFPS();
    const int32_t roundedFps = static_cast<int32_t>(std::lround(fps));
    if (roundedFps != shownFps) {
        shownFps = roundedFps;
        colonyUI->SetFps(fps);
    }

    lastGameMs = MillisecondsSince(updateStart);
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

void Samples::GameColony::HandleInput(double deltaTime) {
    using Key = ME::Input::KeyCode;
    const ME::Input::InputManager& input = *engine->input;
    if (input.GetKeyPressed(Key::Escape)) {
        engine->Quit();
    }
    if (input.GetKeyPressed(Key::UpArrow)) {
        OnSelectPrevious();
    }
    if (input.GetKeyPressed(Key::DownArrow)) {
        OnSelectNext();
    }
    if (input.GetKeyPressed(Key::Minus) || input.GetKeyPressed(Key::Subtract)) {
        OnLoadDown();
    }
    if (input.GetKeyPressed(Key::Equal) || input.GetKeyPressed(Key::Add)) {
        OnLoadUp();
    }
    if (input.GetKeyPressed(Key::Space)) {
        bPaused = !bPaused;
        timings.Restart();
        ME::Log("Colony: ", bPaused ? "paused" : "running");
    }
    if (input.GetKeyPressed(Key::R)) {
        StartScenario(scenarioIndex);
    }
    if (input.GetKeyPressed(Key::N)) {
        StartScenario((scenarioIndex + 1) % SCENARIO_COUNT);
    }
    if (input.GetKeyPressed(Key::Tab)) {
        colonyUI->SetStatsVisible(!colonyUI->IsStatsVisible());
    }
    if (input.GetKeyPressed(Key::L)) {
        LogTimings();
    }

    // The camera: the wheel zooms about the pointer, WASD pans PAN_SPEED view heights per second.
    const float wheel = input.GetMouseWheelDelta();
    if (wheel != 0.0f) {
        const ME::Vec2 pointer = input.GetMouseWorldPosNorm();
        colonyScene->Zoom(wheel, pointer.x, pointer.y);
    }
    float panX = 0.0f;
    float panY = 0.0f;
    panX -= input.GetKeyDown(Key::A) ? 1.0f : 0.0f;
    panX += input.GetKeyDown(Key::D) ? 1.0f : 0.0f;
    panY -= input.GetKeyDown(Key::S) ? 1.0f : 0.0f;
    panY += input.GetKeyDown(Key::W) ? 1.0f : 0.0f;
    if (panX != 0.0f || panY != 0.0f) {
        const float distance = PAN_SPEED * static_cast<float>(deltaTime);
        colonyScene->Pan(panX * distance, panY * distance);
    }
}

void Samples::GameColony::RunSteps() {
    // A set number of steps per frame, not the engine's fixed step, so the load doesn't grow as frames slow down.
    for (uint32_t i = 0; i < stepsPerFrame; ++i) {
        if (scenario.steps != 0 && sim.GetStepIndex() >= scenario.steps) {
            ME::Log("Colony: scenario ", scenario.name, " ended at step ", sim.GetStepIndex(), "; starting again");
            StartScenario(scenarioIndex);
        }
        scenario.ApplyEvents(sim.GetStepIndex(), sim);
        sim.Step();
        timings.AddStep(sim.GetStepTimes());
    }

    if (sim.GetInvariantFailure() != nullptr && !bInvariantLogged) {
        bInvariantLogged = true;
        ME::LogError("Colony: invariant failed at step ", sim.GetStepIndex(), ": ", sim.GetInvariantFailure());
    }
    if (sim.GetHarvestStep() != ColonySim::NO_STEP && !bHarvestLogged) {
        bHarvestLogged = true;
        char time[16];
        FormatSimTime(sim.GetHarvestStep(), time, sizeof(time));
        ME::Log("Colony: scenario ", scenario.name, " harvested at step ", sim.GetHarvestStep(), " (", time, ")");
    }
}

void Samples::GameColony::OnSelectPrevious() {
    const uint32_t row = static_cast<uint32_t>(selectedRow);
    selectedRow = static_cast<LoadRow>(row > 0 ? row - 1 : static_cast<uint32_t>(LoadRow::Count) - 1);
    RefreshLoadPanel();
}

void Samples::GameColony::OnSelectNext() {
    const uint32_t row = static_cast<uint32_t>(selectedRow) + 1;
    selectedRow = static_cast<LoadRow>(row < static_cast<uint32_t>(LoadRow::Count) ? row : 0);
    RefreshLoadPanel();
}

void Samples::GameColony::OnLoadDown() {
    ChangeSelectedRow(-1);
}

void Samples::GameColony::OnLoadUp() {
    ChangeSelectedRow(1);
}

void Samples::GameColony::ChangeSelectedRow(int32_t direction) {
    ColonySettings& settings = sim.GetSettings();
    switch (selectedRow) {
        case LoadRow::Scenario:
            StartScenario((scenarioIndex + SCENARIO_COUNT + (direction < 0 ? SCENARIO_COUNT - 1 : 1)) % SCENARIO_COUNT);
            break;
        case LoadRow::Ants:
            StepAntTarget(direction);
            break;
        case LoadRow::StepsPerFrame:
            stepsPerFrame = StepLadder(STEPS_LADDER, stepsPerFrame, direction);
            ME::Log("Colony: steps per frame ", stepsPerFrame);
            break;
        case LoadRow::FollowerSamples:
            settings.followerSamples = StepLadder(SAMPLES_LADDER, settings.followerSamples, direction);
            ME::Log("Colony: follower samples ", settings.followerSamples);
            break;
        case LoadRow::Explorers:
            sim.SetExplorerShare(StepLadder(EXPLORER_LADDER, settings.explorerShare, direction));
            ME::Log("Colony: explorer share ", Percent(settings.explorerShare), "%");
            break;
        case LoadRow::Collisions:
            settings.bCollisions = direction > 0;
            ME::Log("Colony: collisions ", settings.bCollisions ? "on" : "off");
            break;
        case LoadRow::Drawing:
            bDrawing = direction > 0;
            ME::Log("Colony: drawing ", bDrawing ? "on" : "off");
            break;
        case LoadRow::Count:
            break;
    }
    timings.Restart();
    RefreshLoadPanel();
}

void Samples::GameColony::StartScenario(uint32_t index, uint32_t antCount) {
    scenarioIndex = index;
    ColonyWorld& world = sim.GetWorld();
    if (!scenario.Load(SCENARIOS[index])) {
        world.Clear();
        sim.Start(0, 1);
    } else {
        scenario.Build(world);
        sim.Start(antCount != 0 ? antCount : scenario.ants, scenario.seed);
        ME::Log("Colony: scenario ", scenario.name, ", ", scenario.wallCount, " walls, ", world.CountFood(), " food, ",
                sim.GetAnts().GetCount(), " ants, ", scenario.eventCount, " events, ", scenario.steps, " steps, seed ",
                scenario.seed);
    }
    bInvariantLogged = false;
    bHarvestLogged = false;
    timings.Restart();
    RefreshLoadPanel();
}

void Samples::GameColony::StepAntTarget(int32_t direction) {
    ColonySettings& settings = sim.GetSettings();
    settings.antTarget = StepLadder(ANT_LADDER, settings.antTarget, direction);
    const ColonyNest& nest = sim.GetWorld().GetNest(0);
    ME::Log("Colony: ant target ", settings.antTarget, " (now ", sim.GetAnts().GetCount(), "; stock ", nest.stock,
            ", spent ", nest.foodSpent, ", free spawns ", nest.freeSpawns, ")");
}

void Samples::GameColony::RefreshLoadPanel() {
    if (colonyUI == nullptr) {
        return;
    }
    const ColonySettings& settings = sim.GetSettings();
    char value[32];
    for (uint32_t row = 0; row < static_cast<uint32_t>(LoadRow::Count); ++row) {
        switch (static_cast<LoadRow>(row)) {
            case LoadRow::Scenario:
                snprintf(value, sizeof(value), "%s", scenario.name);
                break;
            case LoadRow::Ants: {
                // The target, and the live count while the colony is still growing or shrinking toward it.
                char target[16];
                char live[16];
                FormatCount(settings.antTarget, target, sizeof(target));
                FormatCount(sim.GetAnts().GetCount(), live, sizeof(live));
                if (sim.GetAnts().GetCount() == settings.antTarget) {
                    snprintf(value, sizeof(value), "%s", target);
                } else {
                    snprintf(value, sizeof(value), "%s (now %s)", target, live);
                }
                break;
            }
            case LoadRow::StepsPerFrame:
                snprintf(value, sizeof(value), "%u", stepsPerFrame);
                break;
            case LoadRow::FollowerSamples:
                snprintf(value, sizeof(value), "%u", settings.followerSamples);
                break;
            case LoadRow::Explorers:
                snprintf(value, sizeof(value), "%d%%", Percent(settings.explorerShare));
                break;
            case LoadRow::Collisions:
                snprintf(value, sizeof(value), "%s", settings.bCollisions ? "on" : "off");
                break;
            case LoadRow::Drawing:
                snprintf(value, sizeof(value), "%s", bDrawing ? "on" : "off");
                break;
            case LoadRow::Count:
                value[0] = '\0';
                break;
        }
        const bool bSelected = static_cast<LoadRow>(row) == selectedRow;
        char text[SceneUIColony::LOAD_CHARS + 1];
        snprintf(text, sizeof(text), "%c %-16s %s", bSelected ? '>' : ' ', ROW_NAMES[row], value);
        colonyUI->SetLoadRow(row, text, bSelected);
    }
    colonyUI->SetLoadRow(static_cast<uint32_t>(LoadRow::Count), "Space pause  R restart  Tab stats  L log", false);
}

void Samples::GameColony::RefreshStats() {
    if (!colonyUI->IsStatsVisible()) {
        return;
    }
    timings.Summarize(stepsPerFrame, summary);
    char text[SceneUIColony::STATS_CHARS + 1];
    uint32_t row = 0;
    auto put = [&](const ME::Color& color) { colonyUI->SetStatsRow(row++, text, color); };

    snprintf(text, sizeof(text), "frame %.2f ms median, worst %.2f (1 s)", summary.frameMedian, summary.frameWorst);
    put(STATS_TEXT);
    snprintf(text, sizeof(text), "game %.2f  engine %.2f  ms/frame", summary.gameMedian,
             summary.median[static_cast<uint32_t>(ColonyTimer::Engine)]);
    put(STATS_TEXT);
    snprintf(text, sizeof(text), "step %.3f ms median, %.3f max", summary.stepMedian, summary.stepMax);
    put(STATS_TEXT);
    snprintf(text, sizeof(text), "serial %.3f ms/step, %.3f ms/frame", summary.serialMedian,
             summary.serialMedian * static_cast<float>(stepsPerFrame));
    put(STATS_TEXT);

    char ants[16];
    char time[16];
    FormatCount(sim.GetAnts().GetCount(), ants, sizeof(ants));
    FormatSimTime(sim.GetStepIndex(), time, sizeof(time));
    if (timings.IsWarmingUp()) {
        snprintf(text, sizeof(text), "ants %s  sim %s  warming up", ants, time);
        put(STATS_TOP);
    } else {
        snprintf(text, sizeof(text), "ants %s  sim %s  1 thread%s", ants, time, bPaused ? "  PAUSED" : "");
        put(STATS_DIM);
    }
    snprintf(text, sizeof(text), "%-13s %8s %8s %6s", "system", "median", "max", "frame");
    put(STATS_DIM);

    for (uint32_t i = 0; i < COLONY_TIMERS; ++i) {
        const ColonyTimer timer = summary.order[i];
        const uint32_t index = static_cast<uint32_t>(timer);
        snprintf(text, sizeof(text), "%-13s %8.3f %8.3f %5.1f%%", GetColonyTimerName(timer), summary.median[index],
                 summary.max[index], summary.share[index]);
        put(i == 0 ? STATS_TOP : (timer == ColonyTimer::Engine ? STATS_ENGINE : STATS_TEXT));
    }
    snprintf(text, sizeof(text), "ms per step; sprites and engine per frame");
    put(STATS_DIM);
}

void Samples::GameColony::LogTimings() {
    timings.Summarize(stepsPerFrame, summary);
    const ColonySettings& settings = sim.GetSettings();
    char line[512];
    snprintf(line, sizeof(line),
             "colony_bench scenario=%s ants=%u spf=%u samples=%u explorers=%d collisions=%d drawing=%d steps=%u "
             "threads=1",
             scenario.name, sim.GetAnts().GetCount(), stepsPerFrame, settings.followerSamples,
             Percent(settings.explorerShare), settings.bCollisions ? 1 : 0, bDrawing ? 1 : 0, summary.stepSamples);
    ME::Log(line);

    size_t length =
        static_cast<size_t>(snprintf(line, sizeof(line), "  ms/step median total=%.3f", summary.stepMedian));
    for (uint32_t i = 0; i < COLONY_STEP_TIMERS && length < sizeof(line); ++i) {
        length += static_cast<size_t>(snprintf(line + length, sizeof(line) - length, " %s=%.3f",
                                               GetColonyTimerName(static_cast<ColonyTimer>(i)), summary.median[i]));
    }
    ME::Log(line);

    snprintf(line, sizeof(line),
             "  ms/step max total=%.3f serial=%.3f game_ms/frame=%.2f engine_ms/frame=%.2f frame_ms=%.2f worst=%.2f",
             summary.stepMax, summary.serialMedian, summary.gameMedian,
             summary.median[static_cast<uint32_t>(ColonyTimer::Engine)], summary.frameMedian, summary.frameWorst);
    ME::Log(line);

    length = static_cast<size_t>(snprintf(line, sizeof(line), "  top"));
    for (uint32_t i = 0; i < COLONY_TIMERS && length < sizeof(line); ++i) {
        const ColonyTimer timer = summary.order[i];
        length += static_cast<size_t>(snprintf(line + length, sizeof(line) - length, " %s=%.0f%%",
                                               GetColonyTimerName(timer), summary.share[static_cast<uint32_t>(timer)]));
    }
    ME::Log(line);

    // Food brought home (stored or spent on spawns) per simulated minute since the scenario started.
    const ColonyNest& nest = sim.GetWorld().GetNest(0);
    const float minutes = static_cast<float>(sim.GetStepIndex()) * ColonySim::STEP_TIME / 60.0f;
    const float foodPerMinute = minutes > 0.0f ? static_cast<float>(nest.stock + nest.foodSpent) / minutes : 0.0f;
    const char* failure = sim.GetInvariantFailure();
    const char* invariants = !sim.IsCheckingInvariants() ? "off" : (failure == nullptr ? "ok" : failure);
    snprintf(line, sizeof(line), "  harvest_step=%d food_per_min=%.0f alive=%u invariants=%s",
             sim.GetHarvestStep() == ColonySim::NO_STEP ? -1 : static_cast<int32_t>(sim.GetHarvestStep()),
             foodPerMinute, sim.GetAnts().GetCount(), invariants);
    ME::Log(line);
}

void Samples::GameColony::StartBenchmark() {
    // Every v1 scenario at three loads, then the whole ant ladder on Open.
    constexpr uint32_t SCENARIO_LOADS[] = {2'000, 12'000, 40'000};
    constexpr uint32_t LOAD_COUNT = sizeof(SCENARIO_LOADS) / sizeof(SCENARIO_LOADS[0]);
    constexpr uint32_t LADDER_COUNT = sizeof(ANT_LADDER) / sizeof(ANT_LADDER[0]);
    static_assert(SCENARIO_COUNT * LOAD_COUNT + LADDER_COUNT <= MAX_BENCH_RUNS);
    benchRunCount = 0;
    for (uint32_t index = 0; index < SCENARIO_COUNT; ++index) {
        for (uint32_t ants : SCENARIO_LOADS) {
            benchRuns[benchRunCount].scenario = index;
            benchRuns[benchRunCount].ants = ants;
            benchRuns[benchRunCount].steps = ants >= BENCH_LARGE_ANTS ? BENCH_STEPS_LARGE : BENCH_STEPS;
            ++benchRunCount;
        }
    }
    for (uint32_t ants : ANT_LADDER) {
        benchRuns[benchRunCount].scenario = 0;
        benchRuns[benchRunCount].ants = ants;
        benchRuns[benchRunCount].steps = ants >= BENCH_LARGE_ANTS ? BENCH_STEPS_LARGE : BENCH_STEPS;
        ++benchRunCount;
    }

    stepsPerFrame = BENCH_STEPS_PER_FRAME;
    benchRun = 0;
    benchStart = Clock::now();
    ME::Log("Colony benchmark: ", benchRunCount, " runs at ", stepsPerFrame, " steps per frame, ", BENCH_STEPS,
            " measured steps each (", BENCH_STEPS_LARGE, " from ", BENCH_LARGE_ANTS, " ants) after a ",
            static_cast<int32_t>(ColonyTimings::WARM_UP_MS), " ms warm-up; invariants ",
            sim.IsCheckingInvariants() ? "checked" : "not checked", " (Debug checks them)");
    StartScenario(benchRuns[0].scenario, benchRuns[0].ants);
}

void Samples::GameColony::AdvanceBenchmark() {
    if (timings.IsWarmingUp() || timings.GetStepCount() < benchRuns[benchRun].steps) {
        return;
    }
    LogTimings();

    ++benchRun;
    if (benchRun < benchRunCount) {
        StartScenario(benchRuns[benchRun].scenario, benchRuns[benchRun].ants);
        return;
    }
    const float seconds = MillisecondsSince(benchStart) / 1000.0f;
    ME::Log("Colony benchmark: done, ", benchRunCount, " runs in ", static_cast<int32_t>(std::lround(seconds)), " s");
    engine->Quit();
}
