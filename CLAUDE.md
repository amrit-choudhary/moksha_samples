@../../moksha/moksha/CLAUDE.md

# Moksha samples

Small games built on the Moksha engine, one folder per sample, each with its own executable. They are
the engine's test harness: an engine change is checked by running the samples against it.

## Layout
- `<sample>/`: `CMakeLists.txt` (`moksha_add_game(Sample_<Name> ...)`), `game_main.cpp` (`MOKSHA_GAME`),
  `game/`, `scene/`, `resources/` (`res_list.json`, `config/settings.ini`, the sample's own assets).
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
