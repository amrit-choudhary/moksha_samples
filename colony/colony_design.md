# Colony: design

Not implemented. A landscape (1280×720) ant colony sample whose job is to be the **test bed for the engine's
multithreading** (roadmap item 16 and the job system after it). It is built and measured now, on one thread, and run
again unchanged once threads exist, so the before and after can be compared system by system.

The ant behaviour follows the ant simulator videos of the YouTube channel Pezzza's Work (trails that fade, sampled
sensing, ant collisions, explorers and followers); see [Sources](#sources). The code is our own.

## 1. Purpose and rules

**Purpose.** Measure how much faster threading makes a heavy frame, on the game side and the engine side, with numbers
taken before threading exists:
- **Game side:** the sample's own systems (section 5), threaded by changing `colony_step.*`.
- **Engine side:** the engine's per-frame work, which Colony makes large on purpose: up to about 157,000 instances
  (57,600 cells and 100,000 ants) whose data is rebuilt and copied every frame (about 15 MB at 96 bytes each) in
  `Scene::Update` and the renderer's upload. When the engine threads that work, Colony needs no change; the same runs
  measure it.

The work covers every shape a game produces:
- **Parallel-for:** many items, same function, each writes only its own output.
- **Job system:** independent systems side by side, jobs that wait on other jobs, work that spans frames, background
  requests whose results come back later.
- **The hard cases:** uneven cost per item, writes from many items into shared cells, claims on a shared resource,
  serial steps between parallel ones, snapshots for readers that run while the world changes.

**Rules.**
1. **No threading code now.** No `std::thread`, atomics, mutexes or async. Every system is written so that threading can
   be added later by changing *one call site*, not the system.
2. **Every system is a range function:** `UpdateSensing(world, ants, begin, end)` and so on. Today the step calls it
   once with the full range. Later, `ParallelFor(0, count, minChunk, ...)` calls it per chunk.
3. **Every system declares what it reads and writes** (table in section 5). That table *is* the future job graph.
4. **Shared writes are marked as such** (section 7). Today they are plain writes; threading picks a fix per case
   (atomics, per-chunk lists, or gathering).
5. **Not deterministic across thread counts, on purpose.** Results may differ between 1 and 12 threads; correctness is
   checked by invariants, gameplay results and ThreadSanitizer instead (section 8). One seeded `ME::Random` makes
   today's single-thread runs repeat.
6. **Every system times itself**, and serial time is reported on its own line (section 10).
7. **No engine changes for measuring.** Colony times only its own code. The engine's share is derived (frame time −
   game time); splitting it further waits for the engine's profiler (roadmap item 17).
8. Sample code follows the engine conventions: `class`, no `std::vector` (fixed capacity arrays allocated at
   `Init`), no exceptions, `namespace Samples`.

## 2. Modes

| Mode | Who controls the colonies | Used for |
|---|---|---|
| **Simulation** (default) | No human. Colonies run on their own rules (v1) or AI (v2+). A scenario file sets the map, the ant counts and timed events | Benchmarks, watching, attract mode on the title screen, the headless twin |
| **Play** (v3) | You control one colony; AI controls the rest | The game |

Simulation mode needs no input at all: it runs a scenario to its end (or loops it). The viewer may still change the
load settings and the speed, and may click an ant to inspect it, but nothing the viewer does is needed. Benchmark runs
use Simulation mode with input ignored and a fixed seed, so single-thread runs repeat.

`Sample_Colony_Headless` runs Simulation mode through a list of scenarios and load settings, logs, and quits.

## 3. The world

- **World:** 1280×720 world units on screen, fixed camera (no scrolling in v1). An ant is a circle of radius 1.
- **Cell grid:** 4×4 units per cell, so 320×180 = 57,600 cells. Each cell holds:
  - `wall` (bool), `food` (amount, 0 = none);
  - per colony: `toHome` and `toFood` marker strength (float), `toEnemy` from v2;
  - from v2: `wallDistance` (cells to the nearest wall), filled by the wall distance field.
- **Colonies:** 1 in v1, up to 4 later. Each has a nest (a circle), stored food, ant counts by caste, and settings
  (explorer share, spawn cost).
- **Ants:** fixed slots, `MAX_ANTS` (100,000 total), structure of arrays. A slot keeps its index for life, so ids are
  stable (for clicking an ant and following it). Dead slots are reused.

Ant fields:

| Field | Meaning |
|---|---|
| `x, y`, `prevX, prevY` | position now and at the start of the step (collisions read `prev`) |
| `heading` | direction in radians |
| `colony`, `caste` | which colony; worker or soldier (v2) |
| `role` | follower or explorer |
| `task` | to food, to home, fight (v2), flee (v2) |
| `carrying` | food carried |
| `walkTime` | time since leaving home or food; sets marker strength |
| `progressTime`, `progressX/Y` | stuck detection |
| `energy` | dies at 0 (v2: fights take it) |
| `alive` | slot in use |

## 4. Ant behaviour

Each step advances 1/60 s. Steps run in `Game::Update`, exactly **steps per frame** of them each frame (section 10),
not in `FixedUpdate`. The engine's fixed step catches up after a slow frame (up to `maxFixedStepsPerFrame`, 8), so the
step count would depend on the frame time and the load would grow as the frame slows. With a set number of steps per
frame, the load is what the settings say, and a slow frame just shows as a lower frame rate (as in Particle Life).

Each step:

1. **Sense.** Take `samples` random points in a 90° cone ahead, up to 40 units away: 32 for followers, 4 for explorers.
   - Skip a point if a wall lies between the ant and it (a ray walk over the cell grid).
   - Food (when searching) or the nest (when carrying) seen directly wins at once.
   - Otherwise keep the strongest marker of the kind the task follows (`toFood` when searching, `toHome` when carrying).
   - Steer toward it, plus a little noise. With nothing found, wander.
2. **Move and collide.** Move forward by speed × dt. Push away from every neighbour within 2 units, using the
   neighbours' **previous** positions; the ant moves only itself. Bounce off walls. (Reading `prev` keeps the
   neighbour reads free of data races once ants move on several threads, without locks.)
3. **Mark.** Every 0.25 s, drop a marker of the kind *opposite* to the one followed: searching ants drop `toHome`,
   carrying ants drop `toFood`. Strength `MARKER_MAX × exp(-k × walkTime)`, written as `max(cell, strength)`.
4. **Food.** A searching ant on a food cell claims one unit, turns around, and starts carrying. At the nest it drops
   the food (colony stock), turns around and searches again. `walkTime` resets at both.
5. **Stuck.** If an ant has moved less than a set distance in the last 4 s, it picks a random direction for a while
   and multiplies every marker it walks over by 0.9. This wipes trails blocked by a new wall.
6. **Fade.** Every marker cell: `strength *= decay` (exponential, as the videos found works with collisions).

Lanes for opposite directions, and lanes switching, should appear by themselves at around 2,000 ants. That is the
visual check that collisions work.

v2 adds fights: ants of different colonies within 2 units attack every 1.5 s; soldiers deal more damage and have more
energy. A killed ant drops its food. A colony spawns a new ant every 0.125 s while it has food (4 per ant, soldiers
12).

**v1 population** (decided 2026-11-14). The Ants setting is the colony's target population. A scenario starts with all
its ants at once, at no cost. After that, Colony spawns toward the target (up to 200 per step), each spawn costing 4
food from the stock when the stock has it and free otherwise (counted as free spawns), and removes the highest slots
when above it (their food goes back on their cell). So benchmarks get exact counts, and food conservation stays exact:
cells + carried + stock + spent on spawns = the starting total. No ant dies otherwise in v1.

## 5. Systems, data access and the step

The step runs these systems in this order. "Kind" is what threading will do with it.

| # | System | Reads | Writes | Kind |
|---|---|---|---|---|
| 1 | **AntGrid** | ant positions | ant grid (counting sort of ant ids by 8×8-unit cell) | **Serial** (prefix sum); later a parallel count plus a serial scan |
| 2 | **Sense** | ants, walls, markers, food, ant grid (v2 enemies) | ant heading, task intent | Parallel-for, **uneven cost** (sample count by role, ray length near walls) |
| 3 | **Move** | ants, `prev` positions, ant grid, walls | ant `x, y`, `progress*` | Parallel-for, neighbour reads |
| 4 | **Claims** | ants, food cells (v2: enemies) | food cells, ant `task`, `carrying`, `walkTime`, colony stock (v2: enemy energy) | **Claims on a shared resource** (section 7) |
| 5 | **Mark** | ant position, `task`, `walkTime` | **markers** (any cell) | **Scatter write** (section 7) |
| 6 | **Fade** | markers | markers | Parallel-for over cells; must follow 5 |
| 7 | **Colony** | colony stock, ant counts | colony stock, a **pending list** of spawns and deaths | Serial, small |
| 8 | **Apply** | pending list | ant slots (`alive`, new ants), `prev` = now | Serial for spawns and deaths, parallel-for for the copy |

Claims runs before Mark because Mark reads `task` and `walkTime`, which Claims changes. Colony writes spawns and
deaths to a pending list instead of the ant slots, because Mark is still reading the slots at the same time; Apply
applies the list at the end of the step.

Then, once per frame (not per step):

| # | System | Reads | Writes | Kind |
|---|---|---|---|---|
| 9 | **AntSprites** | ants | instanced set 1 (one slot per ant) | Parallel-for (game side) |
| 10 | **CellSprites** | cells | instanced set 0 colours (one slot per cell) | Parallel-for (game side) |
| 11 | **Engine `Scene::Update`** | instanced sets | GPU instance data | Engine loop, serial until the engine threads it |
| 12 | **UI** | stats | UI | Main thread only |

**The job graph this table gives** (what the job system should be able to express):

```
AntGrid → Sense → Move → Claims ─┬─ Mark → Fade ─┬─ Apply → (next step)
                                 └─ Colony ──────┘
```
The Mark/Fade branch and Colony can run at the same time; AntSprites and CellSprites can run at the same time as each
other. Today everything runs in table order.

**Main-thread-only rule.** Engine calls that touch shared state (adding or removing scene objects, UI, audio, input)
stay on the main thread. AntSprites and CellSprites call the per-slot setters (`Transform::SetPosition`,
`SpriteRenderer` colour and tile), which write only that slot and its dirty flag, so different slots may be written
from different threads. The engine's threading design has to state that as a rule, or the sprite copy stays serial.

## 6. Job-system cases: background requests (v2, v3)

Each is a **request** with a **result slot**. The game checks the slot each step and applies the result **when it is
ready**; until then it keeps using the old result (the old wall distances, the old path, the old AI plan). Today the
work runs inline when requested, so the result is ready on the next step and the request shows as a hitch. With
threads, the same code polls a job that finishes some frames later. The game logic never changes.

| Request | When | Work | Case it tests |
|---|---|---|---|
| **Wall distance field** (v2) | Walls edited (by a scenario event or the player) | BFS from every wall over 57,600 cells | Background job, result applied later |
| **Path field** (v2) | A rally flag placed | BFS from the flag over open cells; soldiers follow it | High priority; **superseded**: a new flag cancels the old request |
| **AI think** (v2) | Every 30 steps per AI colony | Reads a **snapshot** (copied at the request step: food, ant counts, seen enemies), picks spawn mix, explorer share, rally point | Long job reading a double-buffered snapshot; one job per colony |
| **Map generation** (v3) | New match | Noise walls, food clusters, nests | Low priority, spans many frames behind a loading screen while the attract simulation keeps running |

Jobs read only data that doesn't change while they run (a snapshot, or a copy of the wall grid made at request time),
and write only their own result slot. That is what makes them safe on another thread.

## 7. The hard cases, and today's code for them

**Scatter write (Mark, stuck wipes).** Many ants write the same marker cells.
- Today: plain writes, `cell = max(cell, strength)` and `cell *= 0.9`.
- Later options to compare: **atomics** (a compare-and-swap loop on the float; for `max`, an atomic integer max on the
  float's bits also works since the strengths are positive), **per-chunk lists** applied after the loop, or **each cell
  gathering** from the ants in it through the ant grid. A lost marker write only weakens a trail a little, so the
  cheapest option that is free of data races is good enough here.

**Claims (food, v2 fights).** Two ants may take the last unit of food, or hit the same enemy. Unlike markers, this is a
correctness rule: food must not be taken twice, or created.
- Today: plain read, check, decrement.
- Later options to compare: an atomic decrement that fails at 0, or per-chunk claim lists resolved after the loop.

**Serial steps (AntGrid, Colony).** Their time is shown on its own line, as it limits the gain from
threads (the videos report 2.6× faster at 12,000 ants with parallel-for alone).

**Uneven cost (Sense).** Followers take 8× the samples of explorers, and rays near walls are longer. Explorers are
spread through the slots, so chunks are mixed, but maps with walls on one side are not. This is what `minChunk`, and
splitting work dynamically, should be tried on.

**Too little work (Fade, Colony).** The marker grids are small (57,600 cells × 2 floats per colony, about 460 KB) and
fit in cache, so Fade takes a fraction of a millisecond. Threading it may cost more than it saves. This is the case
for a sensible `minChunk`, or for running it inline.

**Memory access (Move, Sense at high counts).** Ants keep their slots for life, so neighbours in the world are
scattered in memory and the ant grid's lookups jump around. At 100k ants this can make Move bound by memory, not
by the CPU, and then more threads help less. Particle Life avoids this by reordering its particles by cell each step;
Colony doesn't in v1 (slots stay stable for clicking an ant). If the numbers show it, a later step reorders the ants
by cell every few steps and keeps an id-to-slot table.

## 8. Random numbers and checking correctness

**Random numbers: the engine's `ME::Random`** (`moksha/shared/random/random_engine.h`, xoshiro128**).
- Today: one `ME::Random` seeded from the scenario (`Random(uint32_t seed)`), used by every system. Single-thread runs
  with the same seed repeat, which keeps the baseline benchmarks comparable.
- With threads: one `ME::Random` per worker or chunk, seeded from the scenario seed and the worker index. One generator
  can't be shared, since `Next()` changes its state (that would be a data race, and a hot shared cache line).
- Its state is 16 bytes and needs no allocation, so a generator per worker costs nothing.
- `Random(uint32_t)` sets its four state words to the seed XOR'd with fixed constants, then runs 10 warm-up steps.
  For seeds like `base + workerIndex`, which differ only in low bits, hashing the seed first (for example with
  `ME::Utils::HashString2uint32` on a seed string, which the string constructor already does) is cheap insurance.

**Correctness without determinism.** Thread counts may give different runs, so instead of matching checksums:
- **Invariants**, checked every step in Debug:
  - food is conserved: food in cells + carried + colony stock + spent on ants = the scenario's starting total;
  - no cell's food is below 0, no marker is negative or NaN;
  - every live ant lies inside the world and outside walls;
  - the ant grid holds every live ant exactly once.
- **Gameplay results** compared with the single-thread baseline within a tolerance: harvest time (the step when a
  scenario's food runs out), food per minute, ants alive.
- **ThreadSanitizer** on the Mac (Apple clang, `-fsanitize=thread`); MSVC has no equivalent. Run the headless list
  under it once threads land.

## 9. Drawing

- **Set 0, cells:** one quad per cell, 57,600 instances. Colour = ground mixed with each colony's marker colours, walls
  and food. Marker colours change every step, so every cell's appearance is updated each frame (appearance-only updates,
  roadmap item 15).
- **Set 1, ants:** one sprite per ant, turned to its heading, coloured by task (searching, carrying, fighting) and
  colony. Up to 100,000 ants, minus the overlay slots below.
- **Set 1 also takes the overlay sprites** (nest, rally flag), with a few slots reserved.
- **Not now:** shadows. The videos draw an offset body into a white texture and multiply it over the ground. That needs
  render targets and a multiply blend (roadmap items 13 and 2); a plain second sprite per ant would darken where
  shadows overlap and double the instances.

## 10. Measuring

**Per system timing.** Each system in section 5 is timed with `std::chrono::steady_clock` (as Particle Life and Wind
Tunnel do). This is wall-clock time, which is what threading should shrink. Reported:
- ms per system, per step: **median and maximum**, not only the average (threads add variance); the first 2 seconds
  of a run are skipped as warm-up;
- **share of the frame** per system (%), and the systems **sorted by cost**, on screen and in the log, so the one to
  thread first is the top line (per-step systems are multiplied by steps per frame for this);
- **serial ms** (AntGrid + Colony + Apply's spawns and deaths) on its own line;
- **game ms** (all systems, per frame) and **engine ms** = frame time − game ms. The engine's share can't be split
  further until profiling scopes exist (roadmap item 17); it covers `Scene::Update`, the renderer and the UI;
- frame time, median and **worst** over the last second;
- the thread count and the machine's core count, in every log line.

**Where to measure.**
- **Game-side numbers:** the headless twin. It has the null renderer, so the frame holds almost only game work.
- **Engine-side numbers:** the windowed Release build with **vsync off** (`[window] vsync=0` in the benchmark
  settings); with vsync on, the frame rate stops at the display's rate and hides any gain.
- Both in Release. Debug numbers aren't comparable (Wind Tunnel's Debug was about 10× slower).

**Load settings** (keys and buttons; scenarios set them too):

| Setting | Ladder | What it loads |
|---|---|---|
| Ants | 1k, 2k, 4k, 8k, 12k, 20k, 40k, 70k, 100k | Every per-ant system |
| Steps per frame | 1, 2, 4, 8, 16 | Simulation only, not drawing: the cleanest CPU load |
| Follower samples | 8, 16, 32, 64 | Sense, uneven |
| Explorer share | 0%, 10%, 30% | Unevenness of Sense |
| Collisions | on / off | Move's neighbour reads |
| Drawing | on / off | Engine sprite cost vs simulation cost |
| Colonies (v2) | 1–4 | Marker grids, fights, AI jobs |

12k ants is in the ladder to compare with the 2.6× the videos report (different code and hardware, so only a rough
outside point).

**Scenarios** (`resources/scenarios/*.json`): map (walls, food, nests), ants per colony, settings, a step count and
**timed events**: `{ "step": 43200, "event": "addWall", "rect": [...] }`, `removeWall`, `addFood`. v1 scenarios:
1. **Open:** one nest, one food source, nothing else. Lanes form.
2. **Four food sources.**
3. **Wall:** routes form for 12 minutes (43,200 steps), then a wall cuts them. Measures harvest step (the videos: 65
   minutes without stuck wiping, 26 with it).
4. **Shortcut:** routes form around a wall, then a gap opens. Tests explorers.
5. **Maze** with two food sources (v2, with the wall distance field).

**Headless output** (one line per scenario × setting, easy to paste into a sheet):
```
colony_bench scenario=wall ants=12000 spf=4 samples=32 explorers=10 collisions=1 steps=7200 threads=1 cores=12
  ms/step median total=6.10 grid=0.40 sense=3.20 move=1.30 claims=0.05 mark=0.25 fade=0.40 colony=0.02 apply=0.18
  ms/step max total=7.90 serial=0.47 game_ms/frame=24.4 engine_ms/frame=3.1
  top sense=52% move=21% engine=11% fade=7% grid=7% ...
  harvest_step=-1 food_per_min=812 alive=12000 invariants=ok
```
(Numbers above are placeholders for the format.)

`Sample_Colony_Headless` runs a fixed list: every v1 scenario at 2k, 12k and 40k ants, steps per frame 4, then the full
ant ladder on Open. Steps are capped so the whole run takes a few minutes in Release.

## 11. UI

- **Stats panel:** FPS, worst frame, ants, steps per frame, then one row per system sorted by cost (ms and % of the
  frame), serial ms and engine ms. Toggle with Tab.
- **Load panel:** the settings of section 10, `-`/`+` per row, like Graphics Stress.
- **Simulation controls:** pause, speed (= steps per frame), restart scenario, next scenario.
- v3 adds: a collection graph (food brought home per 10 s, as 60 thin UI bars, since there is no line batch yet,
  roadmap item 19); ant info on click (nearest ant within 4 units, through the ant grid: role, task, carrying, walk
  time); and for Play mode the tool buttons (wall, erase, food, rally flag), the worker/soldier mix and colony stock.

## 12. Code layout

Pure simulation code has no engine includes (like `particle_life_sim.*`), so it can be tested in `Samples_Tests`.

| File | Contents |
|---|---|
| `game/colony_world.*` | Cells, walls, food, marker grids, the ant grid; ray walk |
| `game/colony_ants.*` | Ant arrays (structure of arrays); spawn and free slots |
| `game/colony_systems.*` | One range function per system of section 5 |
| `game/colony_step.*` | **The schedule:** `Step()` calls the systems in order and times them. The only file threading will change |
| `game/colony_requests.*` (v2) | Request and result slots; wall distance field, path field, AI think |
| `game/colony_scenario.*` | Loads scenario JSON, fires timed events |
| `game/game_colony.*` | Modes, input, load settings, benchmark loop |
| `scene/scene_colony.*`, `scene/scene_ui_colony.*` | Sets 0 and 1, the UI |

## 13. Stages

Each stage is playable or watchable, measured, and checked before the next starts.

**v1: one colony, Simulation mode only.**
- World, ants, AntGrid, Sense, Move with collisions, Claims, Mark, Fade, Colony, Apply; stuck wiping; explorers.
- Drawing; stats and load panels; scenarios 1–4 with timed events; headless benchmark; Debug invariants.
- Tests: Sense picks the strongest marker; Fade; food is never taken below 0; food is conserved over a run.
- Done when: lanes form in Open at 2k ants; Wall finishes its harvest; invariants hold over the headless list.

**v2: rivals and background requests.**
- Up to 4 colonies, AI colonies, fights, soldiers, `toEnemy` markers.
- Wall distance field, path field (rally flags, used by AI), AI think on snapshots, each applied when ready.
- Maze scenario.

**v3: the game.**
- Play mode: tools, caste mix, win (destroy the enemy nests, or most food at the time limit) and lose.
- Map generation behind a loading screen; title screen running Simulation mode as attract mode.

**Later, when threading lands:** on the game side, change `colony_step.*` only; on the engine side, change nothing in
Colony. Run the headless list with 1, 2, 4, 8 and 12 threads;
invariants must hold, gameplay results must stay within tolerance of the single-thread baseline, ThreadSanitizer must
be clean, and the per-system table shows what scaled.

## 14. What we expect threading to show

To check later, not facts:

| System | Expectation |
|---|---|
| Sense | Scales best: most of the time, independent per ant. Uneven cost shows whether chunking balances |
| Move, Apply's copy, sprite copy | Scale until memory bandwidth limits them; Move may hit that early at 100k (section 7) |
| Fade | Too small to gain much; shows the cost of starting work on other threads |
| Mark, Claims | Limited by contention on shared cells (atomics) or by their merge step (lists) |
| AntGrid, Colony, Apply's spawns and deaths | Don't scale until rewritten (parallel count, serial scan) |
| Engine `Scene::Update` and upload | Don't scale until the engine threads them; likely the biggest serial share at high counts, which makes them the engine's first target |
| Background requests | No faster, but the hitches disappear: worst frame time drops |

## 15. Engine gaps this sample runs into

- 100k instances per set (the top of the ant ladder); two instanced sets only (roadmap item 10).
- Render targets and multiply blend for shadows (items 13, 2).
- Profiling scopes (item 17): the sample times itself until then.
- Pointer over UI (item 18): clicks on the panels must not place things in Play mode.
- Line batch for the graph (item 19).

## Sources
- Pezzza's Work (YouTube): "Ant Simulator, a fresh new version" (multithreading with parallel-for, 2.6× at 12,000 ants;
  collisions; lanes), the follow-up on exponential marker decay, and the video on
  changing environments (stuck wiping, 65 to 26 minutes; 10% explorers; shadows; collection graph).
- [johnBuffer/AntSimulator](https://github.com/johnBuffer/AntSimulator) (MIT): the earlier public version; sampled
  sensing (32 samples, 90° cone, 40 units), marker kinds, colony spawning costs.
