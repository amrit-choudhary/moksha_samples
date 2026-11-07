#!/usr/bin/env python3
"""Builds the samples that work offline for the web and collects them in dist/, with an index page linking them.

Needs the per-machine `web` preset (see CLAUDE.md). Serve the result with: python3 -m http.server -d dist 8000
"""

import html
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build-web"
DIST = ROOT / "dist"

# Folder, target, title and how to play. Rally and Net Test need a server, which a browser can't reach over raw sockets.
SAMPLES = [
    ("prism_drop", "Sample_PrismDrop", "Prism Drop", "Falling block puzzle. Drag to move, tap to rotate, swipe down to drop; or arrows, Z/X, Space and C."),
    ("tower_climb", "Sample_TowerClimb", "Tower Climb", "Endless climber. Click the left or right half, or use Left/Right or A/D."),
    ("survivors", "Sample_Survivors", "Survivors", "Sprite stress test. G toggles god mode, B spawns 1,000 enemies."),
    ("evolution", "Sample_Evolution", "Evolution", "Environment simulation with thousands of creatures. WASD moves the camera."),
    ("ui_demo", "Sample_UIDemo", "UI Demo", "The engine's UI: panels, labels and buttons."),
    ("graphics_stress", "Sample_GraphicsStress", "Graphics Stress", "Spiral galaxy. ^ and v select a load, - and + change it."),
]

INDEX = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Moksha Samples</title>
<style>
  body {{ margin: 0 auto; max-width: 640px; padding: 32px 16px; background: #050505; color: #ccc; font: 16px system-ui, sans-serif; }}
  h1 {{ color: #fff; }}
  a {{ color: #fff; font-size: 18px; }}
  li {{ margin-bottom: 20px; }}
  ul {{ list-style: none; padding: 0; }}
  p {{ margin: 4px 0 0; }}
</style>
</head>
<body>
<h1>Moksha Samples</h1>
<p>Small games on the Moksha engine, compiled to WebAssembly and drawn with WebGPU.</p>
<ul>
{items}
</ul>
</body>
</html>
"""


def main():
    targets = [sample[1] for sample in SAMPLES]
    subprocess.run(["cmake", "--preset", "web"], cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    subprocess.run(["cmake", "--build", "--preset", "web", "--target", *targets, "-j8"], cwd=ROOT, check=True)

    shutil.rmtree(DIST, ignore_errors=True)
    items = []
    for folder, target, title, about in SAMPLES:
        out = DIST / folder
        out.mkdir(parents=True)
        for extension in (".js", ".wasm", ".data"):
            shutil.copy2(BUILD / folder / (target + extension), out)
        # The page loads its .js by name, so only the page itself can be renamed.
        shutil.copy2(BUILD / folder / (target + ".html"), out / "index.html")
        items.append(f'<li><a href="{folder}/">{html.escape(title)}</a><p>{html.escape(about)}</p></li>')

    (DIST / "index.html").write_text(INDEX.format(items="\n".join(items)), encoding="utf-8")
    print(f"Done: {DIST}")


if __name__ == "__main__":
    main()
