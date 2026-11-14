/**
 * Tests for Colony's timings (ColonySeries, ColonyTimings): rolling windows, median and max, the warm-up, engine time
 * as frame minus game, shares of the frame and their order.
 */

#include "colony/game/colony_timing.h"
#include "moksha/test_framework/test_framework.h"

using Samples::COLONY_STEP_TIMERS;
using Samples::ColonySeries;
using Samples::ColonyTimer;
using Samples::ColonyTimings;
using Samples::ColonyTimingSummary;

TEST(ColonySeries, MedianAndMaxOfTheWindow) {
    ColonySeries series;
    ASSERT(series.Init(4));
    float scratch[4];
    EXPECT(series.Median(scratch) == 0.0f && series.Max() == 0.0f);

    series.Add(5.0f);
    series.Add(1.0f);
    series.Add(3.0f);
    EXPECT(series.Median(scratch) == 3.0f);
    EXPECT(series.Max() == 5.0f);
    series.Add(7.0f);
    EXPECT(series.Median(scratch) == 4.0f);  // even count: (3 + 5) / 2

    // A fifth value pushes out the oldest (5).
    series.Add(2.0f);
    EXPECT(series.GetCount() == 4);
    EXPECT(series.Max() == 7.0f);
    EXPECT(series.Median(scratch) == 2.5f);  // 1, 2, 3, 7
}

TEST(ColonySeries, WorstOfTheLatestSecond) {
    ColonySeries series;
    ASSERT(series.Init(8));
    series.Add(900.0f);  // old and long: outside the last second
    for (int i = 0; i < 4; ++i) {
        series.Add(250.0f);
    }
    series.Add(300.0f);
    EXPECT(series.MaxOfLatest(1'000.0f) == 300.0f);
    EXPECT(series.MaxOfLatest(10'000.0f) == 900.0f);
}

TEST(ColonyTimings, WarmUpDropsSamples) {
    ColonyTimings timings;
    ASSERT(timings.Init(64, 64));
    float step[COLONY_STEP_TIMERS] = {};
    step[0] = 1.0f;
    EXPECT(timings.IsWarmingUp());
    timings.AddStep(step);
    timings.AddFrame(1'500.0f, 1.0f, 0.0f, 0.0f);
    EXPECT(timings.IsWarmingUp());
    timings.AddFrame(600.0f, 1.0f, 0.0f, 0.0f);  // 2.1 s in: warm-up over, this frame still dropped
    EXPECT(!timings.IsWarmingUp());

    ColonyTimingSummary summary;
    timings.Summarize(1, summary);
    EXPECT(summary.stepSamples == 0 && summary.frameSamples == 0);

    timings.AddStep(step);
    timings.AddFrame(16.0f, 4.0f, 1.0f, 2.0f);
    timings.Summarize(1, summary);
    EXPECT(summary.stepSamples == 1 && summary.frameSamples == 1);

    timings.Restart();
    EXPECT(timings.IsWarmingUp());
    timings.Summarize(1, summary);
    EXPECT(summary.stepSamples == 0 && summary.frameSamples == 0);
}

TEST(ColonyTimings, SharesAndOrder) {
    ColonyTimings timings;
    ASSERT(timings.Init(64, 64));
    timings.AddFrame(ColonyTimings::WARM_UP_MS, 0.0f, 0.0f, 0.0f);  // ends the warm-up

    // Per step: sense 2 ms, move 1 ms, grid 0.25 ms, colony 0.05 ms, apply's serial part 0.1 ms.
    float step[COLONY_STEP_TIMERS] = {};
    step[static_cast<uint32_t>(ColonyTimer::Sense)] = 2.0f;
    step[static_cast<uint32_t>(ColonyTimer::Move)] = 1.0f;
    step[static_cast<uint32_t>(ColonyTimer::AntGrid)] = 0.25f;
    step[static_cast<uint32_t>(ColonyTimer::Colony)] = 0.05f;
    step[static_cast<uint32_t>(ColonyTimer::ApplySerial)] = 0.1f;
    // Two steps a frame; a 20 ms frame of which the game took 8 ms (sprites 1 + 0.6), so the engine took 12.
    for (int frame = 0; frame < 10; ++frame) {
        timings.AddStep(step);
        timings.AddStep(step);
        timings.AddFrame(20.0f, 8.0f, 1.0f, 0.6f);
    }

    ColonyTimingSummary summary;
    timings.Summarize(2, summary);
    const auto share = [&](ColonyTimer timer) { return summary.share[static_cast<uint32_t>(timer)]; };
    EXPECT_NEAR(summary.frameMedian, 20.0f, 1e-4f);
    EXPECT_NEAR(summary.gameMedian, 8.0f, 1e-4f);
    EXPECT_NEAR(summary.median[static_cast<uint32_t>(ColonyTimer::Engine)], 12.0f, 1e-4f);
    EXPECT_NEAR(summary.stepMedian, 3.4f, 1e-4f);
    EXPECT_NEAR(summary.serialMedian, 0.4f, 1e-4f);
    EXPECT_NEAR(share(ColonyTimer::Engine), 60.0f, 1e-3f);  // 12 of 20
    EXPECT_NEAR(share(ColonyTimer::Sense), 20.0f, 1e-3f);   // 2 ms twice, of 20
    EXPECT_NEAR(share(ColonyTimer::Move), 10.0f, 1e-3f);
    EXPECT_NEAR(share(ColonyTimer::AntSprites), 5.0f, 1e-3f);  // per frame: not multiplied
    EXPECT(summary.order[0] == ColonyTimer::Engine);
    EXPECT(summary.order[1] == ColonyTimer::Sense);
    EXPECT(summary.order[2] == ColonyTimer::Move);
    EXPECT(summary.order[3] == ColonyTimer::AntSprites);
    bool bSorted = true;
    for (uint32_t i = 1; i < Samples::COLONY_TIMERS; ++i) {
        bSorted = bSorted && share(summary.order[i - 1]) >= share(summary.order[i]);
    }
    EXPECT(bSorted);
}
