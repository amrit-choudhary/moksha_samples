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

## Survivors stress test
`Sample_Survivors`: G toggles god mode, B spawns 1,000 enemies. Measure in Release (Debug was 7 fps at 30k).
Baseline (2026-10-18, Windows DX12, Release): 120 fps up to ~70k enemies, then frames drop; cap is 99k.
Each plain sprite costs a DX12 constant buffer from a pool of 256, so bulk sprites go in the instanced sets.

## Raspberry Pi
- Server-only build: `cmake --preset pinned -DMOKSHA_BUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release`, then build
  `Samples_Server`. Headless bots: configure with the client on (Linux builds only the headless client) and
  build `Sample_NetTest_Headless`; `bash tools/load_test.sh <bot exe> <server-ip> [bots] [seconds]`. Verified:
  20 bots × 60 s on a Pi 4B.
- Open: the Pi's server dashboard (me_server_manager, see the village_shop repo's CLAUDE.md) still runs the
  old server; point its `config.json` at `build/server/Samples_Server` when wanted. A small sample `dlc/`
  for the engine's content server doesn't exist yet.
