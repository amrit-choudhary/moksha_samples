#include "colony/game/colony_timing.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr const char* TIMER_NAMES[] = {
    "grid",   "sense",        "move",       "claims",   "fight",       "mark",         "fade",
    "colony", "apply_serial", "apply_copy", "requests", "ant_sprites", "cell_sprites", "engine",
};
static_assert(sizeof(TIMER_NAMES) / sizeof(TIMER_NAMES[0]) == Samples::COLONY_TIMERS);
static_assert(static_cast<uint32_t>(Samples::ColonyTimer::AntSprites) == Samples::COLONY_STEP_TIMERS);

}  // namespace

const char* Samples::GetColonyTimerName(ColonyTimer timer) {
    const uint32_t index = static_cast<uint32_t>(timer);
    return index < COLONY_TIMERS ? TIMER_NAMES[index] : "?";
}

// --- ColonySeries --------------------------------------------------------------------------------------------------

Samples::ColonySeries::~ColonySeries() {
    delete[] values;
}

bool Samples::ColonySeries::Init(uint32_t inCapacity) {
    if (values != nullptr || inCapacity == 0) {
        return false;
    }
    values = new float[inCapacity];
    capacity = inCapacity;
    Clear();
    return true;
}

void Samples::ColonySeries::Clear() {
    count = 0;
    next = 0;
}

void Samples::ColonySeries::Add(float value) {
    if (capacity == 0) {
        return;
    }
    values[next] = value;
    next = (next + 1) % capacity;
    count = count < capacity ? count + 1 : capacity;
}

float Samples::ColonySeries::Max() const {
    float largest = 0.0f;
    for (uint32_t i = 0; i < count; ++i) {
        largest = values[i] > largest ? values[i] : largest;
    }
    return largest;
}

float Samples::ColonySeries::Median(float* scratch) const {
    if (count == 0) {
        return 0.0f;
    }
    // The stored values are the window in some rotation; order doesn't matter for a median.
    std::memcpy(scratch, values, count * sizeof(values[0]));
    float* middle = scratch + count / 2;
    std::nth_element(scratch, middle, scratch + count);
    if (count % 2 == 1) {
        return *middle;
    }
    // Even count: the mean of the two middle values; the lower one is the largest below the middle.
    const float lower = *std::max_element(scratch, middle);
    return 0.5f * (lower + *middle);
}

float Samples::ColonySeries::MaxOfLatest(float total) const {
    float largest = 0.0f;
    float sum = 0.0f;
    for (uint32_t i = 0; i < count && sum < total; ++i) {
        const float value = values[(next + capacity - 1 - i) % capacity];
        largest = value > largest ? value : largest;
        sum += value;
    }
    return largest;
}

// --- ColonyTimings -------------------------------------------------------------------------------------------------

Samples::ColonyTimings::~ColonyTimings() {
    delete[] scratch;
}

bool Samples::ColonyTimings::Init(uint32_t stepCapacity, uint32_t frameCapacity) {
    if (scratch != nullptr || stepCapacity == 0 || frameCapacity == 0) {
        return false;
    }
    for (ColonySeries& series : steps) {
        series.Init(stepCapacity);
    }
    stepTotal.Init(stepCapacity);
    stepSerial.Init(stepCapacity);
    frames.Init(frameCapacity);
    games.Init(frameCapacity);
    engines.Init(frameCapacity);
    antSprites.Init(frameCapacity);
    cellSprites.Init(frameCapacity);
    scratch = new float[stepCapacity > frameCapacity ? stepCapacity : frameCapacity];
    Restart();
    return true;
}

void Samples::ColonyTimings::Restart() {
    for (ColonySeries& series : steps) {
        series.Clear();
    }
    ColonySeries* const others[] = {&stepTotal, &stepSerial, &frames, &games, &engines, &antSprites, &cellSprites};
    for (ColonySeries* series : others) {
        series->Clear();
    }
    warmUpLeftMs = WARM_UP_MS;
}

void Samples::ColonyTimings::AddStep(const float* stepMs) {
    if (IsWarmingUp()) {
        return;
    }
    float total = 0.0f;
    for (uint32_t i = 0; i < COLONY_STEP_TIMERS; ++i) {
        steps[i].Add(stepMs[i]);
        total += stepMs[i];
    }
    stepTotal.Add(total);
    stepSerial.Add(stepMs[static_cast<uint32_t>(ColonyTimer::AntGrid)] +
                   stepMs[static_cast<uint32_t>(ColonyTimer::Colony)] +
                   stepMs[static_cast<uint32_t>(ColonyTimer::ApplySerial)]);
}

void Samples::ColonyTimings::AddFrame(float frameMs, float gameMs, float antSpritesMs, float cellSpritesMs) {
    if (IsWarmingUp()) {
        warmUpLeftMs -= frameMs;
        return;
    }
    frames.Add(frameMs);
    games.Add(gameMs);
    const float engineMs = frameMs - gameMs;
    engines.Add(engineMs > 0.0f ? engineMs : 0.0f);
    antSprites.Add(antSpritesMs);
    cellSprites.Add(cellSpritesMs);
}

void Samples::ColonyTimings::Summarize(uint32_t stepsPerFrame, ColonyTimingSummary& out) {
    out = ColonyTimingSummary();
    for (uint32_t i = 0; i < COLONY_STEP_TIMERS; ++i) {
        out.median[i] = steps[i].Median(scratch);
        out.max[i] = steps[i].Max();
    }
    const ColonySeries* const frameTimers[] = {&antSprites, &cellSprites, &engines};
    for (uint32_t i = 0; i < 3; ++i) {
        out.median[COLONY_STEP_TIMERS + i] = frameTimers[i]->Median(scratch);
        out.max[COLONY_STEP_TIMERS + i] = frameTimers[i]->Max();
    }
    out.stepMedian = stepTotal.Median(scratch);
    out.stepMax = stepTotal.Max();
    out.serialMedian = stepSerial.Median(scratch);
    out.gameMedian = games.Median(scratch);
    out.frameMedian = frames.Median(scratch);
    out.frameWorst = frames.MaxOfLatest(WORST_WINDOW_MS);
    out.stepSamples = stepTotal.GetCount();
    out.frameSamples = frames.GetCount();

    for (uint32_t i = 0; i < COLONY_TIMERS; ++i) {
        const float perFrame =
            i < COLONY_STEP_TIMERS ? out.median[i] * static_cast<float>(stepsPerFrame) : out.median[i];
        out.share[i] = out.frameMedian > 0.0f ? 100.0f * perFrame / out.frameMedian : 0.0f;
        out.order[i] = static_cast<ColonyTimer>(i);
    }
    // Largest share first (insertion sort: twelve entries).
    for (uint32_t i = 1; i < COLONY_TIMERS; ++i) {
        const ColonyTimer timer = out.order[i];
        const float share = out.share[static_cast<uint32_t>(timer)];
        uint32_t j = i;
        while (j > 0 && out.share[static_cast<uint32_t>(out.order[j - 1])] < share) {
            out.order[j] = out.order[j - 1];
            --j;
        }
        out.order[j] = timer;
    }
}
