/**
 * Colony systems: one range function per system of the step (colony_design.md, section 5). Each handles ant slots (or
 * cells) [begin, end) and writes only what its comment says, so threading can later call it per chunk without
 * changing it. Writes that many items share are marked "Shared write". colony_step.* calls them in order.
 * Pure code: the only engine type is ME::Random (shared module, no engine systems).
 */

#pragma once

#include <cstdint>

#include "colony/game/colony_ants.h"
#include "moksha/shared/random/random_engine.h"

namespace Samples {

class ColonyAntGrid;
class ColonyWorld;

// Load settings the systems read.
class ColonySettings {
   public:
    uint32_t followerSamples = 32;  // sensing samples per step
    uint32_t explorerSamples = 4;
    bool bCollisions = true;  // ants push each other apart
    uint32_t antTarget = 0;   // the population the Colony system keeps, per colony
};

class ColonySpawn {
   public:
    float x = 0.0f;
    float y = 0.0f;
    float heading = 0.0f;
    uint8_t colony = 0;
    AntRole role = AntRole::Follower;
};

// Spawns and deaths the Colony system asks for, applied by Apply at the end of the step (while Mark still reads the
// ant slots, Colony mustn't change them). Deaths are ant slots.
class ColonyPending {
   public:
    static constexpr uint32_t MAX_SPAWNS = 256;
    static constexpr uint32_t MAX_DEATHS = 2'048;

    void Clear() {
        spawnCount = 0;
        deathCount = 0;
    }

    ColonySpawn spawns[MAX_SPAWNS];
    uint32_t spawnCount = 0;
    uint32_t deaths[MAX_DEATHS];
    uint32_t deathCount = 0;
};

namespace ColonyTuning {
constexpr float ANT_SPEED = 50.0f;          // units per second
constexpr float SENSE_RANGE = 40.0f;        // units
constexpr float SENSE_MIN = 4.0f;           // samples start a cell away, off the ant's own cell
constexpr float SENSE_CONE = 1.5707963f;    // radians, centred on the heading
constexpr float TURN_RATE = 9.0f;           // radians per second toward the sensed target
constexpr float STEER_NOISE = 0.08f;        // radians per step, either way
constexpr uint32_t MARK_PERIOD_STEPS = 15;  // one marker every 0.25 s at 60 steps per second
constexpr float MARKER_FALLOFF = 0.05f;     // strength = exp(-MARKER_FALLOFF × walkTime)
constexpr float MARKER_MIN = 0.02f;         // weaker markers aren't dropped
constexpr float MARKER_LIFETIME = 30.0f;    // seconds for an untouched marker to fade to 1/e
constexpr float COLLISION_DISTANCE = 2.0f;  // two ants of radius 1
constexpr float MAX_PUSH = 1.0f;            // units per step, all neighbours together
constexpr float MARKER_FLOOR = 0.001f;
constexpr uint32_t SPAWN_COST = 4;             // food per ant, from the nest's stock
constexpr uint32_t MAX_SPAWNS_PER_STEP = 200;  // per colony; at most ColonyPending::MAX_SPAWNS in all
constexpr uint32_t MAX_DEATHS_PER_STEP =
    2'000;  // per colony; at most ColonyPending::MAX_DEATHS in all      // fading markers below this become 0
}  // namespace ColonyTuning

namespace ColonySystems {

// AntGrid: buckets live ants of slots [begin, end) by cell. Serial (count, prefix sum, fill), so it is called with the
// full range; threading later splits it into a parallel count and a serial scan. Writes the ant grid.
void UpdateAntGrid(const ColonyAnts& ants, ColonyAntGrid& grid, uint32_t begin, uint32_t end);

// Sense: samples points in a cone ahead and turns each ant toward food or its nest in sight, else the strongest marker
// it follows, else wanders. Writes each ant's own heading. Uneven cost: followers sample more, rays vary in length.
void UpdateSensing(const ColonyWorld& world, ColonyAnts& ants, const ColonySettings& settings, ME::Random& random,
                   float deltaTime, uint32_t begin, uint32_t end);

// Move: steps each ant forward, bouncing off walls, then (with collisions on) pushes it away from neighbours within
// COLLISION_DISTANCE, found through the ant grid at their previous positions; adds deltaTime to its walk time.
// Writes each ant's own x, y, heading and walkTime. Neighbour reads: memory access jumps around at high counts.
void UpdateMovement(const ColonyWorld& world, ColonyAnts& ants, const ColonyAntGrid& grid,
                    const ColonySettings& settings, float deltaTime, uint32_t begin, uint32_t end);

// Claims: a searching ant on food takes one unit and turns for home; a carrying ant in its nest drops its food there.
// Both reset the walk time, as does a searching ant passing its nest. Shared write: food cells and nest stock.
void UpdateClaims(ColonyWorld& world, ColonyAnts& ants, uint32_t begin, uint32_t end);

// Mark: every MARK_PERIOD_STEPS (staggered by slot), each ant raises its cell's marker to its strength: toHome while
// searching, toFood while carrying. Shared write: marker cells (max).
void UpdateMarkers(ColonyWorld& world, const ColonyAnts& ants, uint32_t stepIndex, uint32_t begin, uint32_t end);

// Fade: multiplies the markers of cells [begin, end) by decay, every colony's; below MARKER_FLOOR they become 0.
// Writes those cells only.
void UpdateFade(ColonyWorld& world, float decay, uint32_t begin, uint32_t end);

// Colony: for colonies [begin, end), asks for spawns in the nest while the colony is below antTarget (each costs
// SPAWN_COST from the stock if the stock has it, else it is free and counted), or for deaths of its highest slots while
// above. Writes nest stock and spend counts, and the pending list. Serial and small.
void UpdateColonies(ColonyWorld& world, const ColonyAnts& ants, const ColonySettings& settings, ME::Random& random,
                    ColonyPending& pending, uint32_t begin, uint32_t end);

// Apply, serial part: entries [begin, end) of the pending list, deaths first (indices below deathCount), then spawns.
// A removed ant's food is put back on its cell, or in its nest if the cell is full. Writes ant slots, food cells and
// nest stock. Called with the full range: slot allocation is shared.
void ApplyPending(ColonyWorld& world, ColonyAnts& ants, const ColonyPending& pending, uint32_t begin, uint32_t end);

// Apply, parallel part: copies each ant's position into prev for the next step.
void UpdatePrevPositions(ColonyAnts& ants, uint32_t begin, uint32_t end);

}  // namespace ColonySystems

}  // namespace Samples
