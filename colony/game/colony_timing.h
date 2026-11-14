/**
 * Colony timings: rolling windows of the sample's own per-system times (measured by the caller with
 * std::chrono::steady_clock), summarized as median and maximum (threads add variance, so not only the average), each
 * system's share of the frame, and the systems sorted by that share. Engine time is derived per frame: frame time -
 * game time. The first WARM_UP_MS of a run are skipped. Pure code, no engine systems.
 */

#pragma once

#include <cstdint>

namespace Samples {

enum class ColonyTimer : uint8_t {
    // Per step, in the step's order.
    AntGrid,
    Sense,
    Move,
    Claims,
    Fight,
    Mark,
    Fade,
    Colony,
    ApplySerial,  // Apply's spawns and deaths
    ApplyCopy,    // Apply's prev = now copy
    Requests,     // background requests: applying results, snapshots, and (inline today) their work
    // Per frame.
    AntSprites,
    CellSprites,
    Engine,  // frame time - game time
    Count
};

constexpr uint32_t COLONY_STEP_TIMERS = 11;  // the timers before AntSprites
constexpr uint32_t COLONY_TIMERS = static_cast<uint32_t>(ColonyTimer::Count);

// A short lowercase name, as the design's log format uses ("sense", "apply_serial", ...).
const char* GetColonyTimerName(ColonyTimer timer);

// A rolling window of the latest values, up to its capacity.
class ColonySeries {
   public:
    ColonySeries() = default;
    ~ColonySeries();

    ColonySeries(const ColonySeries&) = delete;
    ColonySeries& operator=(const ColonySeries&) = delete;

    // Allocates room for capacity values. False if called twice or capacity is 0.
    bool Init(uint32_t capacity);

    void Clear();
    void Add(float value);

    uint32_t GetCount() const {
        return count;
    }
    uint32_t GetCapacity() const {
        return capacity;
    }

    // The largest value, or 0 when empty.
    float Max() const;

    // The median, or 0 when empty; scratch must hold GetCount() values.
    float Median(float* scratch) const;

    // The largest of the newest values that together reach at least `total` (or all of them).
    float MaxOfLatest(float total) const;

   private:
    float* values = nullptr;
    uint32_t capacity = 0;
    uint32_t count = 0;
    uint32_t next = 0;  // where the next value goes
};

class ColonyTimingSummary {
   public:
    // Per ColonyTimer: ms per step for the step timers, ms per frame for the rest.
    float median[COLONY_TIMERS] = {};
    float max[COLONY_TIMERS] = {};
    // Per ColonyTimer: % of the median frame (step timers multiplied by steps per frame).
    float share[COLONY_TIMERS] = {};
    // Timers by share, largest first.
    ColonyTimer order[COLONY_TIMERS] = {};

    float stepMedian = 0.0f;  // all step systems, ms per step
    float stepMax = 0.0f;
    float serialMedian = 0.0f;  // AntGrid + Colony + ApplySerial, ms per step
    float gameMedian = 0.0f;    // all of the game's own work, ms per frame
    float frameMedian = 0.0f;   // ms
    float frameWorst = 0.0f;    // ms, over the last WORST_WINDOW_MS
    uint32_t stepSamples = 0;
    uint32_t frameSamples = 0;
};

class ColonyTimings {
   public:
    static constexpr float WARM_UP_MS = 2'000.0f;
    static constexpr float WORST_WINDOW_MS = 1'000.0f;

    ColonyTimings() = default;
    ~ColonyTimings();

    ColonyTimings(const ColonyTimings&) = delete;
    ColonyTimings& operator=(const ColonyTimings&) = delete;

    // Allocates the windows: stepCapacity steps and frameCapacity frames. False if called twice or a capacity is 0.
    bool Init(uint32_t stepCapacity, uint32_t frameCapacity);

    // Drops every sample and starts the warm-up again: a new run (scenario or setting).
    void Restart();

    bool IsWarmingUp() const {
        return warmUpLeftMs > 0.0f;
    }

    // Steps measured since the warm-up ended (up to the window's capacity).
    uint32_t GetStepCount() const {
        return stepTotal.GetCount();
    }

    // One step's times, COLONY_STEP_TIMERS values in ColonyTimer order. Dropped while warming up.
    void AddStep(const float* stepMs);

    // One frame: its length, the game's own work in it (steps, sprite copies and anything else the game did), and the
    // sprite copies' share of that. Counts down the warm-up, during which it is dropped.
    void AddFrame(float frameMs, float gameMs, float antSpritesMs, float cellSpritesMs);

    // Medians, maxima, shares and the order, with step timers counted stepsPerFrame times per frame.
    void Summarize(uint32_t stepsPerFrame, ColonyTimingSummary& out);

   private:
    ColonySeries steps[COLONY_STEP_TIMERS];
    ColonySeries stepTotal;
    ColonySeries stepSerial;
    ColonySeries frames;
    ColonySeries games;
    ColonySeries engines;
    ColonySeries antSprites;
    ColonySeries cellSprites;
    float* scratch = nullptr;  // for medians, as large as the largest window
    float warmUpLeftMs = WARM_UP_MS;
};

}  // namespace Samples
