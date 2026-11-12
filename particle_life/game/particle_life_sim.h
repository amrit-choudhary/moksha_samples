/**
 * Particle Life: particles of a few types attract or repel each other by a random, asymmetric rule table, and cells,
 * chains and swarms emerge. Pure code on one thread, no engine systems, so it can be tested.
 *
 * The world is a torus of width × height. The interaction radius shrinks as the count grows, so each particle has about
 * the same number of neighbours at every count: the picture keeps its look and the cost grows linearly. Neighbours
 * come from a uniform grid of cells at least one radius wide; each step counting-sorts the particles by cell, so the
 * arrays are reordered (and neighbours stay close in memory). Indices don't identify a particle across steps.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace Samples {

class ParticleLifeSim {
   public:
    static constexpr uint32_t MAX_PARTICLES = 100'000;
    static constexpr uint32_t TYPE_COUNT = 6;

    ParticleLifeSim() = default;
    ~ParticleLifeSim();

    ParticleLifeSim(const ParticleLifeSim&) = delete;
    ParticleLifeSim& operator=(const ParticleLifeSim&) = delete;

    // Allocates room for MAX_PARTICLES in a width × height world (both > 0). False if the size is invalid.
    bool Init(float width, float height);

    // Respawns count particles (clamped to MAX_PARTICLES) at rest, at random places and types from seed.
    void Reset(uint32_t count, uint32_t seed);

    // A new rule table from seed: every entry in [-1, 1], how strongly type i is drawn to type j.
    void RandomizeRules(uint32_t seed);

    // Advances the world by deltaTime seconds (clamped to MAX_STEP_TIME).
    void Step(float deltaTime);

    uint32_t GetCount() const {
        return count;
    }

    float GetWidth() const {
        return width;
    }

    float GetHeight() const {
        return height;
    }

    // The distance within which particles interact, in world units.
    float GetRadius() const {
        return radius;
    }

    float GetRule(uint32_t from, uint32_t to) const {
        return rules[from * TYPE_COUNT + to];
    }

    // Particle i's state, for i < GetCount(). Positions lie in [0, width) × [0, height).
    const float* GetX() const {
        return x;
    }
    const float* GetY() const {
        return y;
    }
    const float* GetVelocityX() const {
        return vx;
    }
    const float* GetVelocityY() const {
        return vy;
    }
    const uint8_t* GetTypes() const {
        return types;
    }

    // A hash of every particle's quantized position and type, to check that runs repeat.
    uint32_t Checksum() const;

    static constexpr float MAX_STEP_TIME = 1.0f / 30.0f;
    // Average neighbours within the radius if particles were spread evenly.
    static constexpr float NEIGHBOURS = 40.0f;
    // Below this fraction of the radius, every pair repels, whatever the rules say.
    static constexpr float REPEL_DISTANCE = 0.3f;
    static constexpr float FORCE_SCALE = 10.0f;
    static constexpr float FRICTION_HALF_LIFE = 0.04f;  // seconds for a particle's speed to halve without forces

   private:
    // Sets the radius and the cell grid for the current count.
    void FitGrid();

    // Counting-sorts the particles by cell into the spare arrays, then swaps them in.
    void SortByCell();

    // Each particle's summed force from its neighbours, into forceX/forceY.
    void AccumulateForces();

    float width = 0.0f;
    float height = 0.0f;
    uint32_t count = 0;
    float radius = 1.0f;
    float rules[TYPE_COUNT * TYPE_COUNT] = {};

    int32_t columns = 0;
    int32_t rows = 0;
    float cellWidth = 1.0f;
    float cellHeight = 1.0f;
    size_t maxCells = 0;

    float* x = nullptr;
    float* y = nullptr;
    float* vx = nullptr;
    float* vy = nullptr;
    uint8_t* types = nullptr;

    // The sort's destination, swapped with the arrays above every step.
    float* spareX = nullptr;
    float* spareY = nullptr;
    float* spareVx = nullptr;
    float* spareVy = nullptr;
    uint8_t* spareTypes = nullptr;

    float* forceX = nullptr;
    float* forceY = nullptr;
    uint32_t* cellOf = nullptr;     // per particle
    uint32_t* cellStart = nullptr;  // per cell, plus one past the end: cell c holds [cellStart[c], cellStart[c + 1])
    uint32_t* cellCursor = nullptr;
};

}  // namespace Samples
