/**
 * Colony requests: work that a step asks for and whose result is applied later (colony_design.md, section 6). Each
 * request copies its inputs when it is made (the walls, or a snapshot of the colony), so the work reads nothing that
 * changes while it runs, and writes only its own result slot. Today RunQueued does the work inline at the end of the
 * step and ApplyReady applies it at the start of the next, so a request shows as a hitch in the requests timer. With a
 * job system, RunQueued hands the jobs to workers and ApplyReady applies only the finished ones; nothing else changes.
 *
 * Requests:
 * - Wall distance field: asked for whenever walls change. A breadth-first search from every wall cell.
 * - Path field, per colony: asked for when its rally point moves (or walls change). A breadth-first search from the
 *   rally cell; soldiers walk down it. A new request for the same colony supersedes one not yet applied.
 * - AI think, per colony: asked for every THINK_PERIOD steps with a snapshot (stock, counts, nests, enemies near the
 *   nest, and a copy of the colony's toEnemy grid). Picks the soldier share and the rally point.
 * Pure code, no engine systems.
 */

#pragma once

#include <cstdint>

#include "colony/game/colony_world.h"

namespace Samples {

class ColonyAnts;
class ColonyAntGrid;

enum class ColonyRequestKind : uint8_t {
    WallDistance,
    Path,
    Think,
    Count
};

constexpr uint32_t COLONY_REQUEST_KINDS = static_cast<uint32_t>(ColonyRequestKind::Count);

// A short lowercase name for logs: "wall_distance", "path", "think".
const char* GetColonyRequestName(ColonyRequestKind kind);

enum class ColonyJobState : uint8_t {
    Idle,    // nothing asked for, or the result was applied
    Queued,  // inputs copied, work not done
    Done,    // result ready, not yet applied
};

// Counts per kind of request since the last Clear.
class ColonyRequestStats {
   public:
    uint32_t requested = 0;
    uint32_t superseded = 0;  // replaced by a newer request before being applied
    uint32_t applied = 0;
    float lastMs = 0.0f;  // the work of the last one run
    float maxMs = 0.0f;
};

// What AI think reads: copied when the request is made.
class ColonyThinkSnapshot {
   public:
    uint32_t colony = 0;
    uint32_t colonyCount = 0;
    float nestX[ColonyWorld::MAX_COLONIES] = {};
    float nestY[ColonyWorld::MAX_COLONIES] = {};
    uint32_t ants[ColonyWorld::MAX_COLONIES] = {};  // live ants per colony
    uint32_t soldiers = 0;                          // the colony's own
    uint64_t stock = 0;                             // the colony's own
    uint32_t threatCount = 0;                       // enemy ants within THREAT_RADIUS of the nest
    float threatX = 0.0f;                           // their centre, when there are any
    float threatY = 0.0f;
    bool bRally = false;  // the current rally point
    float rallyX = 0.0f;
    float rallyY = 0.0f;
    float* toEnemy = nullptr;  // CELL_COUNT values, the colony's toEnemy grid; owned by the job
};

// What AI think decides.
class ColonyThinkResult {
   public:
    float soldierShare = 0.0f;
    bool bRally = false;
    float rallyX = 0.0f;
    float rallyY = 0.0f;
};

namespace ColonyAI {
constexpr uint32_t THINK_PERIOD = 30;      // steps between one colony's thinks
constexpr float THREAT_RADIUS = 160.0f;    // units around the nest
constexpr uint32_t THREAT_MIN = 5;         // enemies near the nest that make the colony defend
constexpr float BASE_SOLDIERS = 0.1f;      // soldier share with no threat
constexpr float MAX_SOLDIERS = 0.5f;       //
constexpr float THREAT_SOLDIERS = 2.0f;    // soldier share added per enemy per own ant near the nest
constexpr uint32_t HOT_BLOCK_CELLS = 8;    // toEnemy is summed over blocks of 8 × 8 cells
constexpr float HOT_MIN = 4.0f;            // a block's toEnemy sum that marks a fight worth joining
constexpr uint64_t ATTACK_STOCK = 200;     // stock a colony wants before it attacks
constexpr float ATTACK_ADVANTAGE = 1.25f;  // own ants over the weakest enemy's, to attack it
constexpr float RALLY_MOVE = 32.0f;        // units; a smaller move keeps the old rally point (and path field)
}  // namespace ColonyAI

// The work, as plain functions (each job calls one), so they can be tested alone.
namespace ColonyJobs {

// Breadth-first search from every wall cell over 4 neighbours: distance in cells, 0 on walls, capped at 255. queue
// holds CELL_COUNT entries.
void ComputeWallDistance(const uint8_t* walls, uint8_t* distance, uint32_t* queue);

// Breadth-first search from targetCell over open cells, 4 neighbours: distance in cells, NO_PATH on walls and cells it
// can't reach (everywhere, if targetCell is a wall). queue holds CELL_COUNT entries.
void ComputePathField(const uint8_t* walls, uint32_t targetCell, uint16_t* field, uint32_t* queue);

// AI think: defend against enemies near the nest; else join the hottest fight (the strongest toEnemy block); else
// attack the weakest enemy nest when well ahead and stocked; else guard the own nest. Soldier share grows with the
// threat.
void Think(const ColonyThinkSnapshot& snapshot, ColonyThinkResult& out);

}  // namespace ColonyJobs

class ColonyRequests {
   public:
    ColonyRequests() = default;
    ~ColonyRequests();

    ColonyRequests(const ColonyRequests&) = delete;
    ColonyRequests& operator=(const ColonyRequests&) = delete;

    // Allocates every job's buffers. False if called twice.
    bool Init();

    // Drops every request and result, and zeroes the stats: a new run.
    void Clear();

    // Copies the walls and queues the wall distance field.
    void RequestWallDistance(const ColonyWorld& world);

    // Copies the walls and queues colony's path field to (x, y); a queued or unapplied one is superseded.
    void RequestPath(const ColonyWorld& world, uint32_t colony, float x, float y);

    // Takes colony's snapshot (the ant grid finds enemies near its nest) and queues its think. Skipped while the last
    // one is still queued or unapplied.
    void RequestThink(const ColonyWorld& world, const ColonyAnts& ants, const ColonyAntGrid& grid, uint32_t colony);

    // Does every queued job's work. Inline today; a job system would start them here instead.
    void RunQueued();

    // Applies every finished result: the fields into the world, think results into the nests (a rally point that
    // moved requests that colony's path field).
    void ApplyReady(ColonyWorld& world);

    const ColonyRequestStats& GetStats(ColonyRequestKind kind) const {
        return stats[static_cast<uint32_t>(kind)];
    }

   private:
    class WallDistanceJob {
       public:
        ColonyJobState state = ColonyJobState::Idle;
        uint8_t* walls = nullptr;
        uint8_t* distance = nullptr;
        uint32_t* queue = nullptr;  // the search's own, so jobs can run side by side later
    };

    class PathJob {
       public:
        ColonyJobState state = ColonyJobState::Idle;
        uint32_t targetCell = 0;
        uint8_t* walls = nullptr;
        uint16_t* field = nullptr;
        uint32_t* queue = nullptr;
    };

    class ThinkJob {
       public:
        ColonyJobState state = ColonyJobState::Idle;
        ColonyThinkSnapshot snapshot;
        ColonyThinkResult result;
    };

    // Queues a job, counting a supersede if one was waiting.
    void Queue(ColonyJobState& state, ColonyRequestKind kind);

    // Records one run's work time.
    void RecordRun(ColonyRequestKind kind, float ms);

    WallDistanceJob wallDistance;
    PathJob paths[ColonyWorld::MAX_COLONIES];
    ThinkJob thinks[ColonyWorld::MAX_COLONIES];
    ColonyRequestStats stats[COLONY_REQUEST_KINDS];
};

}  // namespace Samples
