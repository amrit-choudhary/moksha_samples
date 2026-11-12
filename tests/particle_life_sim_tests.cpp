/**
 * Tests for Particle Life's simulation (ParticleLifeSim): setup, bounds, the radius rule, repeatability and forces.
 */

#include "moksha/test_framework/test_framework.h"
#include "particle_life/game/particle_life_sim.h"

using Samples::ParticleLifeSim;

static constexpr float WIDTH = 112.5f;
static constexpr float HEIGHT = 200.0f;

TEST(ParticleLifeSim, InitRejectsAnEmptyWorld) {
    ParticleLifeSim sim;
    EXPECT(!sim.Init(0.0f, HEIGHT));
    EXPECT(!sim.Init(WIDTH, -1.0f));
    EXPECT(sim.Init(WIDTH, HEIGHT));
    EXPECT(!sim.Init(WIDTH, HEIGHT));  // only once
}

TEST(ParticleLifeSim, CountIsClamped) {
    ParticleLifeSim sim;
    ASSERT(sim.Init(WIDTH, HEIGHT));
    sim.Reset(ParticleLifeSim::MAX_PARTICLES + 5, 1);
    EXPECT(sim.GetCount() == ParticleLifeSim::MAX_PARTICLES);
    sim.Reset(10, 1);
    EXPECT(sim.GetCount() == 10);
}

TEST(ParticleLifeSim, RadiusShrinksWithCount) {
    ParticleLifeSim sim;
    ASSERT(sim.Init(WIDTH, HEIGHT));
    sim.Reset(1'000, 1);
    const float few = sim.GetRadius();
    sim.Reset(100'000, 1);
    const float many = sim.GetRadius();
    // A hundred times the particles: a tenth of the radius, the same neighbours each.
    EXPECT_NEAR(few / many, 10.0f, 0.01f);
    // Tiny counts stop at a third of the world, so the cell grid stays 3 × 3 or more.
    sim.Reset(1, 1);
    EXPECT(sim.GetRadius() <= WIDTH / 3.0f + 0.001f);
}

TEST(ParticleLifeSim, RulesStayInRange) {
    ParticleLifeSim sim;
    ASSERT(sim.Init(WIDTH, HEIGHT));
    sim.RandomizeRules(99);
    for (uint32_t i = 0; i < ParticleLifeSim::TYPE_COUNT; ++i) {
        for (uint32_t j = 0; j < ParticleLifeSim::TYPE_COUNT; ++j) {
            EXPECT(sim.GetRule(i, j) >= -1.0f && sim.GetRule(i, j) <= 1.0f);
        }
    }
}

TEST(ParticleLifeSim, ParticlesStayInsideTheWorld) {
    ParticleLifeSim sim;
    ASSERT(sim.Init(WIDTH, HEIGHT));
    sim.Reset(5'000, 3);
    for (int step = 0; step < 120; ++step) {
        sim.Step(1.0f / 60.0f);
    }
    const float* x = sim.GetX();
    const float* y = sim.GetY();
    bool inside = true;
    for (uint32_t i = 0; i < sim.GetCount(); ++i) {
        inside = inside && x[i] >= 0.0f && x[i] < WIDTH && y[i] >= 0.0f && y[i] < HEIGHT;
    }
    EXPECT(inside);
    EXPECT(sim.GetCount() == 5'000);
}

TEST(ParticleLifeSim, KeepsEveryTypeThroughSorting) {
    ParticleLifeSim sim;
    ASSERT(sim.Init(WIDTH, HEIGHT));
    sim.Reset(3'000, 5);
    uint32_t before[ParticleLifeSim::TYPE_COUNT] = {};
    for (uint32_t i = 0; i < sim.GetCount(); ++i) {
        ++before[sim.GetTypes()[i]];
    }
    for (int step = 0; step < 30; ++step) {
        sim.Step(1.0f / 60.0f);
    }
    uint32_t after[ParticleLifeSim::TYPE_COUNT] = {};
    for (uint32_t i = 0; i < sim.GetCount(); ++i) {
        ++after[sim.GetTypes()[i]];
    }
    for (uint32_t t = 0; t < ParticleLifeSim::TYPE_COUNT; ++t) {
        EXPECT(before[t] == after[t]);
    }
}

TEST(ParticleLifeSim, SameSeedsRepeat) {
    ParticleLifeSim a;
    ParticleLifeSim b;
    ASSERT(a.Init(WIDTH, HEIGHT));
    ASSERT(b.Init(WIDTH, HEIGHT));
    a.RandomizeRules(4);
    b.RandomizeRules(4);
    a.Reset(2'000, 8);
    b.Reset(2'000, 8);
    for (int step = 0; step < 60; ++step) {
        a.Step(1.0f / 60.0f);
        b.Step(1.0f / 60.0f);
    }
    EXPECT(a.Checksum() == b.Checksum());

    b.Reset(2'000, 9);
    EXPECT(a.Checksum() != b.Checksum());
}

TEST(ParticleLifeSim, ParticlesMoveAndSettle) {
    ParticleLifeSim sim;
    ASSERT(sim.Init(WIDTH, HEIGHT));
    sim.Reset(2'000, 2);
    const uint32_t start = sim.Checksum();
    float fastest = 0.0f;
    for (int step = 0; step < 300; ++step) {
        sim.Step(1.0f / 60.0f);
    }
    for (uint32_t i = 0; i < sim.GetCount(); ++i) {
        const float vx = sim.GetVelocityX()[i];
        const float vy = sim.GetVelocityY()[i];
        const float speed = vx * vx + vy * vy;
        fastest = speed > fastest ? speed : fastest;
    }
    EXPECT(sim.Checksum() != start);
    // Friction bounds the speed: well under the world's height per second.
    EXPECT(fastest < HEIGHT * HEIGHT);
}
