# Moksha samples

Small games built on the [Moksha engine](https://github.com/amrit-choudhary/moksha), each in its own
folder with its own executable. They show how a game uses the engine and serve as the engine's test
harness.

| Sample | Executable | Shows |
|---|---|---|
| `ui_demo/` | `Sample_UIDemo` | UI widgets (panels, labels, images, a button), text, input |
| `net_test/` | `Sample_NetTest` | Networking against `Samples_Server`: connect, ping, synced scores, high score, timed quiz |

Also built: `Samples_Server` (`server/`, the net test's server, with a SQLite high-score table) and
`Samples_Tests` (`tests/`). `shared/` holds the code both sides use (the net test's messages).

To try the net test, start `Samples_Server`, then one or two `Sample_NetTest` windows. Point them at another
machine with `--serverIP=<ip>` (and `--serverPort=<port>`).

### Load test
`Sample_NetTest_Headless` is the net test without a window, playing as a bot: it scores, chats, answers the quiz,
measures ping round trips, and with `--duration=<s>` logs a `BOT SUMMARY` line and quits. To run many at once:

```
bash tools/load_test.sh <path to Sample_NetTest_Headless> <server-ip> [bots=20] [seconds=60]
```

On Linux (e.g. a Raspberry Pi) the headless bot and the server build without a GPU:
`cmake --preset pinned -DCMAKE_BUILD_TYPE=Release`, then build `Samples_Server` and `Sample_NetTest_Headless`.

## Building
Requires CMake 3.31+ and a C++20 compiler. On Windows the cook also needs `dxc` and `texconv` on `PATH`.

```
cmake --preset pinned
cmake --build --preset pinned
```

`pinned` builds the engine commit set in `MOKSHA_PIN` (root `CMakeLists.txt`), fetched from GitHub.

### Working on the engine at the same time
Clone the engine next to this repo (`<Projects>/moksha/moksha` beside `<Projects>/moksha_samples/moksha_samples`)
and add a `CMakeUserPresets.json` (gitignored) with a `live` preset:

```json
{
    "version": 6,
    "configurePresets": [
        {
            "name": "live",
            "binaryDir": "${sourceDir}/build-live",
            "cacheVariables": { "FETCHCONTENT_SOURCE_DIR_MOKSHA": "${sourceDir}/../../moksha/moksha" }
        }
    ],
    "buildPresets": [ { "name": "live", "configurePreset": "live" } ]
}
```

`cmake --preset live` then builds your local engine clone, so engine edits show up on the next build.
It warns when `MOKSHA_PIN` is behind the clone. Push the engine first, then bump the pin.
Open `moksha_samples.code-workspace` to have both repos in one VS Code window.

Executables run from their build folder; their cooked `resources/` folder sits next to them.

## License
MIT, see [LICENSE](LICENSE).
