/**
 * Colony game: an ant colony in Simulation mode, the test bed for the engine's multithreading (colony_design.md).
 * The simulation runs `steps per frame` steps of 1/60 s in Update, never in FixedUpdate, so the load is what the
 * settings say and a slow frame shows as a lower frame rate. A scenario's timed events fire before their step; at its
 * last step it starts again; the harvest step (all food gone) is logged.
 * Every system is timed (median and max per step, share of the frame, sorted by cost; engine = frame - game) after a
 * 2 s warm-up, which any change of scenario or setting starts again.
 * Keys: Up/Down pick a load row and -/+ change it (or the ^ v - + buttons); Space pauses; R restarts the scenario and
 * N starts the next; Tab shows or hides the stats; L logs them; the scroll wheel zooms about the pointer and WASD
 * pans; Escape quits. Debug builds check the invariants after every step and log the first failure.
 * Headless, it is the benchmark: every v1 scenario at 2k, 12k and 40k ants, then the whole ant ladder on Open, at
 * BENCH_STEPS_PER_FRAME; each run starts its ants at once, warms up, measures BENCH_STEPS steps (BENCH_STEPS_LARGE
 * from BENCH_LARGE_ANTS up) and logs the design's colony_bench block; then it quits. Per-system times are exact; frame
 * and engine times include the headless driver's sleep to the next fixed step (shorten it with --fixedStepFPS=1000).
 * Frame time is measured between Update calls, not taken from deltaTime, which the engine clamps.
 */

#pragma once

#include <chrono>
#include <cstdint>

#include "colony/game/colony_scenario.h"
#include "colony/game/colony_step.h"
#include "colony/game/colony_timing.h"
#include "colony/scene/scene_colony.h"
#include "colony/scene/scene_ui_colony.h"
#include "moksha/client/game/game.h"

namespace Samples {

class GameColony : public ME::Game {
   public:
    virtual void Init(ME::Engine& inEngine) override;
    virtual void Update(double deltaTime) override;
    virtual void End() override;

    virtual const char* GetDisplayName() const override;

   private:
    // The load panel's rows, in order.
    enum class LoadRow : uint8_t {
        Scenario,
        Ants,
        StepsPerFrame,
        FollowerSamples,
        Explorers,
        Collisions,
        Drawing,
        Count
    };

    // Button and key callbacks.
    void OnSelectPrevious();
    void OnSelectNext();
    void OnLoadDown();
    void OnLoadUp();

    // Changes the selected row's setting one step down (-1) or up (+1), and starts the timings again.
    void ChangeSelectedRow(int32_t direction);

    // A benchmark run: a scenario, its ant count and how many steps to measure.
    class BenchRun {
       public:
        uint32_t scenario = 0;
        uint32_t ants = 0;
        uint32_t steps = 0;
    };

    // Loads SCENARIOS[index], builds its map and spawns its ants (antCount, or the scenario's when 0). On failure the
    // world is an empty map.
    void StartScenario(uint32_t index, uint32_t antCount = 0);

    // Fills the benchmark's run list and starts the first run.
    void StartBenchmark();

    // Logs the current run once it has its measured steps, then starts the next or quits after the last.
    void AdvanceBenchmark();

    // Moves the ant target one rung down or up the ladder.
    void StepAntTarget(int32_t direction);

    // Handles keys, the wheel and WASD.
    void HandleInput(double deltaTime);

    // Runs this frame's steps, firing timed events before each.
    void RunSteps();

    // Rewrites the load panel's rows.
    void RefreshLoadPanel();

    // Rewrites the stats panel from the current timings.
    void RefreshStats();

    // Logs the timings in the design's benchmark format (section 10), tagged with what is running.
    void LogTimings();

    static constexpr double STATS_PERIOD = 0.25;        // seconds between stats refreshes
    static constexpr float PAN_SPEED = 0.8f;            // view heights per second, WASD
    static constexpr uint32_t BENCH_STEPS = 600;        // measured steps per run, after the warm-up
    static constexpr uint32_t BENCH_STEPS_LARGE = 300;  // for the slowest runs, so the list takes a few minutes
    static constexpr uint32_t BENCH_LARGE_ANTS = 70'000;
    static constexpr uint32_t BENCH_STEPS_PER_FRAME = 4;
    static constexpr uint32_t MAX_BENCH_RUNS = 32;

    // Rolling windows for the timings: 16 steps a frame for 4 s, and 4 s of frames at 240 Hz.
    static constexpr uint32_t STEP_WINDOW = 16 * 60 * 4;
    static constexpr uint32_t FRAME_WINDOW = 240 * 4;

    SceneColony* colonyScene = nullptr;
    SceneUIColony* colonyUI = nullptr;

    ColonySim sim;
    ColonyScenario scenario;
    ColonyTimings timings;
    ColonyTimingSummary summary;

    uint32_t scenarioIndex = 0;
    uint32_t stepsPerFrame = 1;
    bool bDrawing = true;
    bool bPaused = false;
    LoadRow selectedRow = LoadRow::Ants;

    // The game's own time in the last frame, paired with the next frame's length (which contains it). Frame length is
    // the time between Update calls: the engine clamps deltaTime (to 0.25 s), which would hide slow frames.
    std::chrono::steady_clock::time_point lastUpdateStart;
    bool bHasLastUpdate = false;
    float lastGameMs = 0.0f;
    float lastAntSpritesMs = 0.0f;
    float lastCellSpritesMs = 0.0f;
    double statsTime = 0.0;

    bool bBenchmark = false;
    BenchRun benchRuns[MAX_BENCH_RUNS];
    uint32_t benchRunCount = 0;
    uint32_t benchRun = 0;
    std::chrono::steady_clock::time_point benchStart;
    bool bInvariantLogged = false;
    bool bHarvestLogged = false;
    int32_t shownFps = -1;  // the rate on screen; the label is rewritten only when it changes
};

}  // namespace Samples
