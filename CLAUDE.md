@../../moksha/moksha/CLAUDE.md
@../../moksha/moksha/docs/game_guide.md

# Moksha samples

Small games built on the Moksha engine, one folder per sample, each with its own executable. They are
the engine's test harness: an engine change is checked by running the samples against it.

## Layout
- `<sample>/`: `CMakeLists.txt` (`moksha_add_game(Sample_<Name> ...)`), `game_main.cpp` (`MOKSHA_GAME`),
  `game/`, `scene/`, `resources/` (`res_list.json`, `config/settings.ini`, the sample's own assets).
- `shared/`: code the samples and the server both use (`Samples_Shared`), e.g. the net test's messages,
  whose verbs start at `GameProtocol::FIRST_GAME_VERB`. `server/`: `Samples_Server` (`moksha_add_server`), an
  `IGameServerListener` per sample that needs one. `tests/`: `Samples_Tests`.
- Includes are qualified from the repo root: `#include "ui_demo/scene/scene_ui_demo.h"`; engine headers
  as `"moksha/client/..."`. Samples never include each other.
- Sample code lives in `namespace Samples`, never in the engine's `ME`.
- Engine assets (shaders, the font, the UI atlas) come from the engine's resource list; a sample's
  `res_list.json` names only its own files.

## Engine link
- All samples share one engine pin, `MOKSHA_PIN` in the root `CMakeLists.txt`.
- Presets: `pinned` (`build/`, the pinned commit from GitHub) and `live` (`build-live/`, from
  `CMakeUserPresets.json`, which is per machine and gitignored), which builds the local engine clone at
  `../../moksha/moksha`. Day-to-day work uses `live`.
- An engine API change is made in the engine, and the samples are fixed in the same session. The user
  pushes the engine first, then bumps `MOKSHA_PIN` and pushes the samples.
- The engine's conventions (above) apply here too.

## Systems check (platform bring-up)
`Sample_SystemsCheck` draws nothing (empty scenes) and logs PASS/FAIL/SKIP for config, time, timers, files, random
and fixed point, physics, network, input and audio. Run it first on a platform whose renderer isn't ported yet; the
headless twin runs the automatic checks and quits. The random checksum must be the same on every platform.
Reference (2026-10-26, macOS headless, Apple clang 21): 7 pass; checksum `299C1597`, fixed-point raw `1270944`.
macOS windowed (2026-10-26, null renderer, empty window): input, audio, minimize pause and every quit path work.
macOS Metal 4 (2026-10-27, M3): `RendererMetal` clears and presents every frame; the temporary colour cycle runs smoothly.
macOS Metal 4 parity (2026-10-27, M3): every sample draws as on DX12 (sprites, instanced sets, UI, debug overlay), with
no Metal API validation errors (`MTL_DEBUG_LAYER=1`).
The ping it logs counts whole frames (33 ms on localhost at 60 Hz), so it isn't a latency measurement.

## Survivors stress test
`Sample_Survivors`: G toggles god mode, B spawns 1,000 enemies. Measure in Release (Debug was 7 fps at 30k).
Baseline (2026-10-18, Windows DX12, Release): 120 fps up to ~70k enemies, then frames drop; cap is 99k.
Baseline (2026-10-27, macOS Metal 4, M3 MacBook Air, 60 Hz display, vsync on, Release): 60 fps up to ~60k enemies,
then frames drop. Release build: `cmake --preset pinned -B build-release -DCMAKE_BUILD_TYPE=Release`.
Each plain sprite costs its own constant buffer, up to 1,000 (`MaxSpriteRendererCount`), so bulk sprites go in the instanced
sets.

## Tower Climb (mobile PoC)
`Sample_TowerClimb`: endless portrait climber (450×800), the proof of concept for the iOS port. Tap (click) the left or
right half, or Left/Right/A/D, to leap between walls. The camera never moves: shaft rows and a 16-slot hazard pool
recycle as the world scrolls down, so coordinates stay small. `ReadTap()` is the only input read; touch replaces it.
Verified on macOS Metal 4 (2026-10-30). Open: anvils can pinch an older hazard on the other wall (no fairness rule yet).
iOS (2026-10-30, iPhone 16, iOS 27, Xcode 27.1): plays and saves its best height. Build with the per-machine
`ios-device` preset (Xcode generator, `build-ios/`, team ID and `MOKSHA_BUNDLE_ID_PREFIX`; it needs `Moksha_Cook`
built in `build-live` first), then run from `build-ios/MokshaSamples.xcodeproj` in Xcode, or install and launch with
`xcrun devicectl`. Device only: the Simulator has no Metal 4. A free team's signing lasts 7 days.

## Net test on iOS
`Sample_NetTest` is landscape on iOS; its centre column fits a 393-point-high phone. It resends CONNECT every second
until the server answers. Point `[net] serverIP` / `contentServerIP` in its `settings.ini` at the server before
building (the copy in the app can't be edited); keep that edit out of commits. The first signed build of a new bundle
id runs from Xcode, which makes the provisioning profile (command-line builds can't use the Xcode account). On first
launch, allow Local Network access. Verified (2026-10-31, iPhone, Pi 4B server at 192.168.29.15): connects and plays
against the Pi.

## Rally (two-player network test)
`Sample_Rally`: portrait paddle ball for two phones (450×800). `Samples_Server` runs the match (`server/rally_server.h`,
60 Hz) and sends its state every tick; clients send their paddle x every fixed step and draw the newest state, with no
interpolation or prediction (only the own paddle is drawn where the finger is). Seat 1 sees the court turned
around. A client is connected while its input arrives: one silent for 2 s loses its seat (the engine's `GameServer`
never drops clients and gives a reconnect a new ID). Free seats go to whoever sends input; a human takes a bot's seat.
`Sample_Rally_Headless` is that bot, meant to run all the time as the partner for a single phone. `server/main.cpp`
routes verbs from `RALLY_INPUT` on to the rally, the rest to the net test. Not yet run.

## Graphics Stress (rendering stress test and demo)
`Sample_GraphicsStress`: portrait spiral galaxy that loads every render path (450×800). ^ and v select a load, - and +
change it: stars (set 1, up to 100k), moving stars (the same stars marked dirty each frame, so their matrices are rebuilt
in `Scene::Update`), clouds (set 0, up to 5k), cloud size (100-400%, overdraw grows with its square), UI panels and UI
text (240 each). 1,000 plain sprites always orbit, one draw call each. The same galaxy is built on every run. A press has a
one-frame hitch, so read the FPS after it settles.
5k clouds at 100% did not drop frames, so raise the size to reach the fill limit. Engine ceilings that keep some loads
from reaching a drop: plain sprites stop at 1,000 (`MaxSpriteRendererCount`; the draw count is also fixed at startup, so it
has no button), UI trees 512 elements, text 10k glyphs. Stars and clouds use the sets' draw counts (since 2026-11-12), so only the shown instances are
updated, copied and drawn; before that, both full buffers (105k × 96 bytes) were copied every frame and hidden instances
had zero scale, so readings at low loads aren't comparable across that change.
Instances get nearer in draw order: depth is Less with writes on, so at equal depth only the first sprite would draw.
iOS draws on a 60 Hz timer with one frame in flight, so its FPS falls in steps (60, 30, 20). Measure in Release.
Below 1 fps a frame outlasts the renderer's 1 s GPU wait, so don't extend the size ladder past that.
Works on Windows (2026-11-07, dev machine).

## Prism Drop (falling block puzzle)
`Sample_PrismDrop`: portrait (450×800) falling block puzzle for iPhone and the web. The rules are pure code in
`game/prism_board.*` (10×22 well, seven-piece bag, rotation kicks, hold, T-spin, combo, back-to-back, perfect clear),
tested in `Samples_Tests`; `game_prism_drop.cpp` does timing (gravity, 0.5 s lock delay, 15 resets, DAS/ARR), input and
effects. Touch: drag to move, drag down to soft drop, swipe down to hard drop, swipe up or tap the hold box to hold, tap
to rotate, tap above the well to pause. Keys: arrows, Space, Z/X/Up, C/Shift, P, M mute.
The title screen is attract mode: `game/prism_autoplay.*` plays a dimmed, silent demo that ends at 30 lines and
restarts, so `Sample_PrismDrop_Headless` runs locks, clears, level-ups and game overs without input.
Its art, sounds and music are generated (made outside this repo); the piece colours in the block art match
`game/prism_palette.h`. The music is a gapless 96 kbps MP3 (verified 2026-11-07). Set 0 holds every block in fixed slots, set 1 the particle pool
(`game/prism_fx.*`), each slot with its own depth (see the scene header). The engine's `StopMusic` and mute do nothing
yet, so pausing sets the master volume to 0.
Verified 2026-11-07 (macOS M3, live engine): builds for macOS, web and iOS (unsigned); 20/20 tests; 2 min headless
demo; Metal runs with no validation errors. Plays on the web (2026-11-07, mokshaengine.com) and on Windows (2026-11-07). Not yet played on an
iPhone. The first signed iOS build of
its bundle id runs from Xcode.

## Particle Life (CPU simulation load test)
`Sample_ParticleLife`: landscape (1280×720) Particle Life, the CPU counterpart of Graphics Stress. Six particle types attract or
repel each other by a random, asymmetric rule table (`game/particle_life_sim.*`, pure code, tested in `Samples_Tests`): a
uniform grid with a counting sort each step, toroidal world, one step per frame on the main thread. The interaction radius
shrinks with the count, so every particle has about 40 neighbours and the cost grows linearly. - and + (or -/= and the
arrows) step through 1,000 to 100,000 particles, respawning from a fixed seed; R or every 45 s switches the rules. The UI
shows FPS, the count and the simulation time. Particles are one additive instanced set (`SpriteBlend::Additive`, draw
count = live particles), so crowds glow toward white; the glow texture is generated (stdlib Python, not kept).
`Sample_ParticleLife_Headless` is a CPU benchmark: 120 fixed steps per count, logging ms per step and a checksum, then quits.
Baseline (2026-11-12, Windows DX12, Release, dev machine): 5k 120 fps (vsync cap), sim 2.3 ms; 20k 113 fps, sim 7.9 ms;
100k 22 fps, sim 40 ms. Headless: 0.63 ms at 1k, 8.2 ms at 20k, 38 ms at 100k. The Windows Vulkan dev build draws the same, and so does the web build (2026-11-12, Chrome on Windows, published
to mokshaengine.com). Not yet run on Metal or an Android phone.

## Web
Every offline sample plays in a browser. Verified 2026-11-04 in Safari on macOS (M3), Emscripten 6.0.13: Tower Climb, Survivors,
Evolution, UI Demo and Graphics Stress, with input, audio and saves. Rally and Net Test build, but browsers have no raw sockets,
so they can't connect. `Sample_SystemsCheck` is a good first check: its network check fails, and its checksums must match.
- Setup: emsdk, whose install needs Python 3.10+, and a per-machine `web` preset (`build-web/`, Release, Emscripten's toolchain
  file, `EMSDK_PYTHON`, `MOKSHA_COOK_EXECUTABLE` from `build-live`, tests off). Rebuild `Moksha_Cook` in `build-live` after
  changing the cook. A changed `MOKSHA_RENDERER` default needs `cmake --preset web --fresh`.
- `python3 tools/build_web.py` builds the published samples into `dist/` with an index page. Serve it with
  `python3 -m http.server -d dist 8000` and open `http://localhost:8000/`; WebGPU needs `localhost` or HTTPS.
- Windows (verified 2026-11-07, Prism Drop with audio): the `web` preset needs the Ninja generator (VS's own `ninja.exe` as
  `CMAKE_MAKE_PROGRAM`), `CMAKE_CXX_SCAN_FOR_MODULES=OFF` (the scan can't find the `emdawnwebgpu` port's headers), and
  the cook from `build-live/.../tools/cook/Debug/`. Run the scripts with `python`, not `python3` (a Store stub).
- Reload with Cmd+Option+R after a build, or Safari may run the old files. Under Node, the headless twin
  (`Sample_SystemsCheck_Headless.js`) runs from its build folder with emsdk's Node.
- The two socket errors in the console at startup are expected. Plan and research: the engine's `docs/web_port_plan.md`.

## Raspberry Pi
- The Pi is `amritpi` (192.168.29.15), Debian 13 aarch64; the clone is
  `~/gamedev/cpp_projects/moksha_samples/moksha_samples`. The user runs commands on the Pi themselves.
- Server-only build: `cmake --preset pinned -DMOKSHA_BUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release`, then build
  `Samples_Server`. Verified 2026-10-31 at pin `d3adef2`, with the Mac net test as the client. Headless bots:
  configure with the client on (Linux builds only the headless client) and build `Sample_NetTest_Headless`;
  `bash tools/load_test.sh <bot exe> <server-ip> [bots] [seconds]`. Verified: 20 bots × 60 s on a Pi 4B.
- The Pi's dashboard (`me_server_manager`, a separate repo; `http://192.168.29.15:8080`) builds and runs
  `Samples_Server` and the engine's `Moksha_ContentServer` (configure adds `-DMOKSHA_BUILD_TOOLS=ON`). The content
  server runs in place at `build/_deps/moksha-build/tools/content_server/`, so a rebuild can replace it while it
  runs; the build steps copy its `settings.ini` and the engine's `test_dlc/` (one `hello.txt`) next to it as
  `dlc/`. Open: the samples have no `dlc/` of their own yet; when they do, change that copy step.
