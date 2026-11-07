"""
Writes every asset of the Prism Drop sample with the standard library only: textures (RGBA PNGs), sound effects and the
looping music track (16-bit mono WAVs). Everything is drawn or synthesised here; the output is deterministic.
Usage: python3 tools/gen_prism_drop_assets.py   (from anywhere; overwrites prism_drop/resources/{textures,audio}/)
"""

import math
import os
import random
import struct
import wave
import zlib

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "prism_drop", "resources")
TEXTURE_DIR = os.path.join(ROOT, "textures", "prism")
AUDIO_DIR = os.path.join(ROOT, "audio")

# The sprite shaders discard texels below this alpha, so soft edges are cut there.
DISCARD_ALPHA = 0.1

# Piece colours, in the game's piece order: I, O, T, S, Z, J, L.
PIECE_COLORS = [
    (0.24, 0.90, 1.00),
    (1.00, 0.85, 0.24),
    (0.72, 0.42, 1.00),
    (0.36, 0.95, 0.48),
    (1.00, 0.31, 0.42),
    (0.31, 0.48, 1.00),
    (1.00, 0.60, 0.24),
]


# ---------------------------------------------------------------------------------------------------------------------
# Images


class Image:
    """Float RGBA pixels, row by row from the top."""

    def __init__(self, width, height):
        self.width = width
        self.height = height
        self.pixels = [(0.0, 0.0, 0.0, 0.0)] * (width * height)

    def set(self, x, y, rgba):
        self.pixels[y * self.width + x] = rgba

    def blit(self, tile, ox, oy):
        for y in range(tile.height):
            row = (oy + y) * self.width + ox
            self.pixels[row:row + tile.width] = tile.pixels[y * tile.width:(y + 1) * tile.width]

    def write_png(self, name):
        rows = bytearray()
        for y in range(self.height):
            rows.append(0)  # filter: none
            for r, g, b, a in self.pixels[y * self.width:(y + 1) * self.width]:
                rows += bytes((to_byte(r), to_byte(g), to_byte(b), to_byte(a)))

        def chunk(kind, data):
            return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

        os.makedirs(TEXTURE_DIR, exist_ok=True)
        with open(os.path.join(TEXTURE_DIR, name), "wb") as out:
            out.write(b"\x89PNG\r\n\x1a\n")
            out.write(chunk(b"IHDR", struct.pack(">IIBBBBB", self.width, self.height, 8, 6, 0, 0, 0)))
            out.write(chunk(b"IDAT", zlib.compress(bytes(rows), 9)))
            out.write(chunk(b"IEND", b""))


def to_byte(value):
    return max(0, min(255, round(value * 255.0)))


def clamp(value, low=0.0, high=1.0):
    return low if value < low else (high if value > high else value)


def mix(a, b, t):
    return tuple(x + (y - x) * t for x, y in zip(a, b))


def smoothstep(edge0, edge1, x):
    t = clamp((x - edge0) / (edge1 - edge0))
    return t * t * (3.0 - 2.0 * t)


def cut(alpha):
    """Soft alpha with the shader's discard level made explicit: faint texels vanish instead of flickering."""
    return alpha if alpha >= DISCARD_ALPHA else 0.0


def rounded_rect_distance(px, py, half_x, half_y, radius):
    """Signed distance from (px, py) to a rectangle with half extents (half_x, half_y) and rounded corners, centred
    at the origin. Negative inside."""
    qx = abs(px) - half_x + radius
    qy = abs(py) - half_y + radius
    outside = math.hypot(max(qx, 0.0), max(qy, 0.0))
    return outside + min(max(qx, qy), 0.0) - radius


def rounded_box_distance(px, py, half, radius):
    return rounded_rect_distance(px, py, half, half, radius)


def make_noise(seed, cells):
    """Smooth, tiling value noise over [0, 1) squared with `cells` lattice cells per side."""
    rng = random.Random(seed)
    grid = [[rng.random() for _ in range(cells)] for _ in range(cells)]

    def noise(u, v):
        x = (u % 1.0) * cells
        y = (v % 1.0) * cells
        ix, iy = int(x), int(y)
        fx, fy = x - ix, y - iy
        fx, fy = fx * fx * (3.0 - 2.0 * fx), fy * fy * (3.0 - 2.0 * fy)
        x1, y1 = (ix + 1) % cells, (iy + 1) % cells
        top = grid[iy][ix] + (grid[iy][x1] - grid[iy][ix]) * fx
        bottom = grid[y1][ix] + (grid[y1][x1] - grid[y1][ix]) * fx
        return top + (bottom - top) * fy

    return noise


def make_fbm(seed, base_cells, octaves):
    layers = [make_noise(seed * 31 + i, base_cells * (2 ** i)) for i in range(octaves)]
    total = sum(0.5 ** i for i in range(octaves))

    def fbm(u, v):
        return sum(layer(u, v) * (0.5 ** i) for i, layer in enumerate(layers)) / total

    return fbm


# ---------------------------------------------------------------------------------------------------------------------
# Textures

BLOCK_TILE = 64
FX_TILE = 64


def block_tile(color):
    """A cut-gem block: bevelled rim lit from the top left, a gradient face, a glossy diagonal sheen and a sparkle."""
    tile = Image(BLOCK_TILE, BLOCK_TILE)
    light = mix(color, (1.0, 1.0, 1.0), 0.55)
    dark = mix(color, (0.0, 0.0, 0.0), 0.55)
    outline = mix(color, (0.0, 0.0, 0.0), 0.75)
    rim = 7
    for y in range(BLOCK_TILE):
        for x in range(BLOCK_TILE):
            u = (x + 0.5) / BLOCK_TILE
            v = (y + 0.5) / BLOCK_TILE
            if x < 1 or y < 1 or x >= BLOCK_TILE - 1 or y >= BLOCK_TILE - 1:
                tile.set(x, y, outline + (1.0,))
                continue
            left, top, right, bottom = x, y, BLOCK_TILE - 1 - x, BLOCK_TILE - 1 - y
            nearest = min(left, top, right, bottom)
            if nearest < rim:
                # Bevel: which edge is nearest decides its light, the diagonal corners split evenly.
                if nearest == top or (nearest == left and left <= bottom):
                    shade = mix(light, (1.0, 1.0, 1.0), 0.25 * (1.0 - nearest / rim))
                    if nearest == left and nearest != top:
                        shade = mix(light, color, 0.35)
                else:
                    shade = mix(dark, color, nearest / rim * 0.4)
                    if nearest == right and nearest != bottom:
                        shade = mix(dark, color, 0.3)
                tile.set(x, y, shade + (1.0,))
                continue
            # Face: a vertical gradient with a soft centre glow.
            face = mix(mix(color, (1.0, 1.0, 1.0), 0.18), mix(color, (0.0, 0.0, 0.0), 0.28), v)
            glow = max(0.0, 1.0 - math.hypot(u - 0.42, v - 0.4) * 2.4)
            face = mix(face, mix(color, (1.0, 1.0, 1.0), 0.45), glow * 0.35)
            # Glossy sheen across the top-left half of the face.
            diagonal = u + v
            sheen = smoothstep(0.62, 0.55, diagonal) * 0.32 + smoothstep(0.86, 0.8, diagonal) * 0.1
            face = mix(face, (1.0, 1.0, 1.0), sheen)
            # Fine facet line from corner to corner, very faint.
            if abs(u - v) < 0.012:
                face = mix(face, (1.0, 1.0, 1.0), 0.08)
            tile.set(x, y, face + (1.0,))
    # Sparkle: a small four-point star near the top-left of the face.
    cx, cy = 17.5, 16.5
    for y in range(8, 26):
        for x in range(8, 28):
            dx, dy = abs(x - cx), abs(y - cy)
            star = max(0.0, 1.0 - math.hypot(dx, dy) / 2.2) + max(0.0, 1.0 - dx / 6.0) * max(0.0, 1.0 - dy * 1.2) \
                + max(0.0, 1.0 - dy / 6.0) * max(0.0, 1.0 - dx * 1.2)
            if star > 0.0:
                r, g, b, a = tile.pixels[y * BLOCK_TILE + x]
                tile.set(x, y, mix((r, g, b), (1.0, 1.0, 1.0), clamp(star)) + (a,))
    return tile


def ghost_tile():
    """White, tinted per piece: a bright rounded outline over a faint fill."""
    tile = Image(BLOCK_TILE, BLOCK_TILE)
    half = BLOCK_TILE / 2.0
    for y in range(BLOCK_TILE):
        for x in range(BLOCK_TILE):
            d = rounded_box_distance(x + 0.5 - half, y + 0.5 - half, half - 4.0, 9.0)
            if d > 0.0:
                continue
            edge = 1.0 - smoothstep(0.0, 6.0, -d)
            tile.set(x, y, (1.0, 1.0, 1.0, cut(max(0.16, edge * 0.95))))
    return tile


def glow_tile():
    """A soft rounded square glow, for flashes over cells and the lock pulse."""
    tile = Image(BLOCK_TILE, BLOCK_TILE)
    half = BLOCK_TILE / 2.0
    for y in range(BLOCK_TILE):
        for x in range(BLOCK_TILE):
            d = rounded_box_distance(x + 0.5 - half, y + 0.5 - half, half - 10.0, 8.0)
            alpha = 1.0 if d < 0.0 else max(0.0, 1.0 - d / 10.0) ** 2
            tile.set(x, y, (1.0, 1.0, 1.0, cut(alpha)))
    return tile


def solid_tile():
    tile = Image(BLOCK_TILE, BLOCK_TILE)
    tile.pixels = [(1.0, 1.0, 1.0, 1.0)] * (BLOCK_TILE * BLOCK_TILE)
    return tile


def bar_tile():
    """A horizontal light bar: bright core fading towards the top and bottom edges. Stretched across cleared rows."""
    tile = Image(BLOCK_TILE, BLOCK_TILE)
    for y in range(BLOCK_TILE):
        v = abs((y + 0.5) / BLOCK_TILE * 2.0 - 1.0)
        alpha = cut(1.0 - smoothstep(0.25, 1.0, v))
        core = 1.0 - smoothstep(0.0, 0.3, v)
        for x in range(BLOCK_TILE):
            tile.set(x, y, (1.0, 1.0, 0.9 + 0.1 * core, alpha))
    return tile


def write_blocks():
    """blocks.png: 8 x 2 tiles of 64 px. Row 0: the seven piece blocks and a white block. Row 1: ghost, glow, solid,
    light bar."""
    atlas = Image(BLOCK_TILE * 8, BLOCK_TILE * 2)
    for index, color in enumerate(PIECE_COLORS):
        atlas.blit(block_tile(color), index * BLOCK_TILE, 0)
    atlas.blit(block_tile((0.92, 0.92, 0.96)), 7 * BLOCK_TILE, 0)
    atlas.blit(ghost_tile(), 0, BLOCK_TILE)
    atlas.blit(glow_tile(), BLOCK_TILE, BLOCK_TILE)
    atlas.blit(solid_tile(), 2 * BLOCK_TILE, BLOCK_TILE)
    atlas.blit(bar_tile(), 3 * BLOCK_TILE, BLOCK_TILE)
    atlas.write_png("blocks.png")


def fx_tile(kind):
    """White particle shapes, tinted at runtime."""
    tile = Image(FX_TILE, FX_TILE)
    rng = random.Random(kind)
    for y in range(FX_TILE):
        dy = (y + 0.5) / FX_TILE * 2.0 - 1.0
        for x in range(FX_TILE):
            dx = (x + 0.5) / FX_TILE * 2.0 - 1.0
            r = math.hypot(dx, dy)
            if kind == 0:  # soft dot
                a = max(0.0, 1.0 - r) ** 1.6
            elif kind == 1:  # hard round spark with a small halo
                a = 1.0 if r < 0.38 else max(0.0, 1.0 - (r - 0.38) / 0.4) ** 2
            elif kind == 2:  # four-point twinkle
                core = max(0.0, 1.0 - r * 2.5)
                rays = max(0.0, 1.0 - r) ** 1.5 * max(0.0, 1.0 - min(abs(dx), abs(dy)) * 10.0)
                a = min(1.0, core + rays)
            elif kind == 3:  # ring
                a = max(0.0, 1.0 - abs(r - 0.82) / 0.12) ** 1.5
            elif kind == 4:  # shard: a diamond, bright at one edge
                d = abs(dx) * 1.6 + abs(dy)
                a = 1.0 if d < 0.85 else 0.0
                if a > 0.0:
                    a = 0.7 + 0.3 * clamp(1.0 - (dx + dy + 1.0) / 2.0)
            elif kind == 5:  # streak: a vertical beam, brightest at the bottom, fading up
                v = (y + 0.5) / FX_TILE
                a = max(0.0, 1.0 - abs(dx) * 2.2) ** 1.5 * (0.15 + 0.85 * v)
            elif kind == 6:  # wide glow
                a = max(0.0, 1.0 - r) ** 2.4
            else:  # smoke puff: noisy soft disc
                a = max(0.0, 1.0 - r) ** 1.2 * (0.6 + 0.4 * rng.random())
            tile.set(x, y, (1.0, 1.0, 1.0, cut(min(1.0, a))))
    return tile


def write_fx():
    """fx.png: 8 x 1 tiles of 64 px; see fx_tile for the order."""
    atlas = Image(FX_TILE * 8, FX_TILE)
    for kind in range(8):
        atlas.blit(fx_tile(kind), kind * FX_TILE, 0)
    atlas.write_png("fx.png")


def write_backdrop():
    """backdrop.png (360 x 640): deep violet space with drifting nebulae, a horizon glow and a receding floor grid."""
    width, height = 360, 640
    image = Image(width, height)
    nebula_a = make_fbm(11, 3, 5)
    nebula_b = make_fbm(23, 4, 4)
    rng = random.Random(5)
    top = (0.03, 0.02, 0.10)
    middle = (0.09, 0.03, 0.18)
    horizon_y = 0.78
    for y in range(height):
        v = (y + 0.5) / height
        base = mix(top, middle, smoothstep(0.0, horizon_y, v))
        for x in range(width):
            u = (x + 0.5) / width
            n1 = nebula_a(u * 0.9, v * 0.55)
            n2 = nebula_b(u * 0.7 + 0.3, v * 0.6)
            magenta = smoothstep(0.48, 0.78, n1) * (1.0 - smoothstep(0.55, 0.85, v)) * 0.55
            teal = smoothstep(0.52, 0.8, n2) * smoothstep(0.05, 0.4, v) * 0.45
            color = mix(base, (0.75, 0.18, 0.62), magenta * 0.6)
            color = mix(color, (0.10, 0.62, 0.80), teal * 0.55)
            # Horizon glow.
            glow = math.exp(-((v - horizon_y) / 0.035) ** 2)
            color = mix(color, (1.0, 0.36, 0.62), glow * 0.55)
            if v > horizon_y:
                # Floor: dark with a perspective grid fading into the horizon.
                depth = (v - horizon_y) / (1.0 - horizon_y)
                color = mix(color, (0.05, 0.01, 0.10), 0.55)
                z = 1.0 / max(depth, 0.02)
                line_h = abs(math.sin(z * 2.2)) < 0.12 * (1.0 + depth)
                lane = (u - 0.5) * z
                line_v = abs(lane * 4.0 - round(lane * 4.0)) < 0.035 * z
                if line_h or line_v:
                    color = mix(color, (0.95, 0.30, 0.85), 0.55 * smoothstep(0.0, 0.5, depth))
            # Dither against banding when the texture is stretched.
            jitter = (rng.random() - 0.5) / 255.0
            image.set(x, y, tuple(clamp(c + jitter) for c in color) + (1.0,))
    # Baked faint stars above the horizon.
    for _ in range(260):
        x, y = rng.randrange(width), rng.randrange(int(height * horizon_y))
        r, g, b, a = image.pixels[y * width + x]
        level = 0.25 + rng.random() * 0.5
        image.set(x, y, (max(r, level), max(g, level), max(b, level * 1.1), 1.0))
    image.write_png("backdrop.png")


def frame_image(width, height, border, fill_alpha, grid_cell):
    """A neon frame: a glowing line `border` px in from the edge, cyan at the top to magenta at the bottom, around a dark
    translucent glass fill, with an optional grid of `grid_cell` px cells inside the line."""
    image = Image(width, height)
    line = border - 3.0
    for y in range(height):
        v = (y + 0.5) / height
        neon = mix((0.30, 0.95, 1.0), (1.0, 0.30, 0.85), v)
        for x in range(width):
            px, py = x + 0.5, y + 0.5
            # Distance to the frame line, a rounded rectangle.
            d = rounded_rect_distance(px - width / 2.0, py - height / 2.0, width / 2.0 - line, height / 2.0 - line,
                                      10.0)
            inside = d < 0.0
            dist = abs(d)
            core = max(0.0, 1.0 - dist / 1.6)
            halo = max(0.0, 1.0 - dist / (border - 1.0)) ** 2
            if inside:
                fill = (0.03, 0.03, 0.08)
                if grid_cell:
                    gx = (px - border) % grid_cell
                    gy = (py - border) % grid_cell
                    if gx < 1.0 or gy < 1.0:
                        fill = (0.11, 0.10, 0.22)
                    # Inner vignette: lighter near the bottom where pieces land.
                fill = mix(fill, (0.10, 0.04, 0.16), v * 0.5)
                color = mix(fill, neon, max(core, halo * 0.6))
                alpha = max(fill_alpha, core, halo)
            else:
                color = neon
                alpha = cut(max(core, halo * 0.85))
            image.set(x, y, color + (alpha,))
    return image


def write_frames():
    # The well: 10 x 20 cells of 32 px inside a 16 px border.
    frame_image(16 + 320 + 16, 16 + 640 + 16, 16, 0.78, 32).write_png("well.png")
    # Hold and next boxes.
    frame_image(128, 128, 12, 0.70, 0).write_png("box.png")


def write_atlas_json(name, tile_x, tile_y, tiles_x, tiles_y, count):
    path = os.path.join(ROOT, "texture_data", name)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as out:
        out.write("{\n")
        out.write(f'    "tileSizeX": {tile_x},\n    "tileSizeY": {tile_y},\n    "padding": 0,\n')
        out.write(f'    "numTextures": {count},\n    "numTilesX": {tiles_x},\n    "numTilesY": {tiles_y},\n')
        out.write(f'    "width": {tile_x * tiles_x},\n    "height": {tile_y * tiles_y},\n    "paddingType": 0\n')
        out.write("}\n")


# ---------------------------------------------------------------------------------------------------------------------
# Audio

RATE = 22050


def write_wav(name, samples, gain=1.0):
    os.makedirs(AUDIO_DIR, exist_ok=True)
    data = bytearray()
    for s in samples:
        data += struct.pack("<h", int(clamp(s * gain, -1.0, 1.0) * 32767))
    with wave.open(os.path.join(AUDIO_DIR, name), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(bytes(data))


def note_hz(name):
    """'A4', 'C#5', 'Bb3' -> frequency."""
    semis = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}[name[0]]
    rest = name[1:]
    if rest.startswith("#"):
        semis += 1
        rest = rest[1:]
    elif rest.startswith("b"):
        semis -= 1
        rest = rest[1:]
    midi = (int(rest) + 1) * 12 + semis
    return 440.0 * 2.0 ** ((midi - 69) / 12.0)


def osc(kind, phase, duty=0.5):
    p = phase % 1.0
    if kind == "sine":
        return math.sin(2.0 * math.pi * p)
    if kind == "square":
        return 1.0 if p < duty else -1.0
    if kind == "triangle":
        return 4.0 * abs(p - 0.5) - 1.0
    if kind == "saw":
        return 2.0 * p - 1.0
    raise ValueError(kind)


def tone(freq, seconds, kind="sine", attack=0.005, decay=None, volume=1.0, sweep_to=None, duty=0.5, vibrato=0.0):
    """One note. decay: exponential decay rate; None holds and releases at the end."""
    count = int(seconds * RATE)
    out = [0.0] * count
    phase = 0.0
    for i in range(count):
        t = i / RATE
        f = freq if sweep_to is None else freq + (sweep_to - freq) * (i / count)
        if vibrato and t > 0.12:
            f *= 1.0 + vibrato * math.sin(2.0 * math.pi * 5.5 * t)
        phase += f / RATE
        env = min(1.0, t / attack) if attack > 0 else 1.0
        if decay is not None:
            env *= math.exp(-decay * t)
        else:
            env *= min(1.0, (seconds - t) / 0.03)
        out[i] = osc(kind, phase, duty) * env * volume
    return out


def noise(seconds, decay, volume, seed=1, lowpass=1.0):
    rng = random.Random(seed)
    count = int(seconds * RATE)
    out = [0.0] * count
    last = 0.0
    for i in range(count):
        last += (rng.uniform(-1.0, 1.0) - last) * lowpass
        out[i] = last * math.exp(-decay * i / RATE) * volume
    return out


def add(dest, src, start=0):
    end = min(len(dest), start + len(src))
    for i in range(start, end):
        dest[i] += src[i - start]
    return dest


def silence(seconds):
    return [0.0] * int(seconds * RATE)


def arpeggio(notes, step, kind, volume, decay, length):
    out = silence(length)
    for index, name in enumerate(notes):
        note = tone(note_hz(name), length - index * step, kind, decay=decay, volume=volume)
        add(out, add(note, tone(note_hz(name) * 2.0, length - index * step, "sine", decay=decay * 1.5,
                                volume=volume * 0.3)), int(index * step * RATE))
    return out


def write_sfx():
    write_wav("move.wav", tone(880.0, 0.03, "square", decay=80.0, volume=0.18, duty=0.25))
    write_wav("rotate.wav", tone(620.0, 0.06, "square", decay=40.0, volume=0.2, sweep_to=1100.0, duty=0.3))
    thump = tone(140.0, 0.22, "sine", decay=16.0, volume=0.9, sweep_to=38.0)
    write_wav("hard_drop.wav", add(thump, noise(0.12, 30.0, 0.35, seed=3, lowpass=0.3)))
    write_wav("lock.wav", add(tone(220.0, 0.08, "triangle", decay=45.0, volume=0.45),
                              noise(0.04, 80.0, 0.2, seed=4, lowpass=0.5)))
    write_wav("clear1.wav", arpeggio(["E6", "B6"], 0.06, "triangle", 0.35, 6.0, 0.45))
    write_wav("clear2.wav", arpeggio(["E6", "G#6", "B6"], 0.05, "triangle", 0.35, 6.0, 0.5))
    write_wav("clear3.wav", arpeggio(["E6", "G#6", "B6", "E7"], 0.045, "triangle", 0.35, 5.0, 0.6))
    quad = arpeggio(["A5", "C#6", "E6", "A6", "C#7", "E7"], 0.05, "square", 0.16, 3.5, 1.1)
    riser = noise(0.5, 3.0, 0.25, seed=9, lowpass=0.15)
    add(quad, [s * (i / len(riser)) for i, s in enumerate(riser)])
    add(quad, tone(110.0, 0.9, "sine", decay=4.0, volume=0.6, sweep_to=55.0))
    write_wav("quad.wav", quad)
    whoosh = noise(0.2, 6.0, 0.5, seed=12, lowpass=0.08)
    write_wav("hold.wav", [s * math.sin(math.pi * i / len(whoosh)) for i, s in enumerate(whoosh)])
    write_wav("level_up.wav", arpeggio(["C6", "E6", "G6", "C7", "E7"], 0.07, "square", 0.17, 4.0, 0.8))
    over = silence(1.6)
    for index, name in enumerate(["A4", "E4", "C4", "A3"]):
        add(over, tone(note_hz(name), 0.7, "triangle", decay=3.0, volume=0.45, vibrato=0.01), int(index * 0.22 * RATE))
    write_wav("game_over.wav", over)
    write_wav("start.wav", arpeggio(["A5", "E6", "A6"], 0.06, "triangle", 0.4, 5.0, 0.6))
    write_wav("pause.wav", add(tone(660.0, 0.08, "triangle", decay=20.0, volume=0.3),
                               tone(990.0, 0.08, "triangle", decay=20.0, volume=0.3)))


# Music: an original synthwave loop in D minor, 132 BPM, 32 bars (A B A' B'), seamless.
BPM = 132
EIGHTH = 60.0 / BPM / 2.0
BAR = EIGHTH * 8

# Melody as (note or None for a rest, eighths), one list per bar.
SECTION_A = [
    [("D5", 2), ("A4", 1), ("D5", 1), ("F5", 2), ("E5", 1), ("D5", 1)],
    [("D5", 1), ("C5", 1), ("Bb4", 2), ("F5", 2), ("D5", 2)],
    [("C5", 2), ("F5", 1), ("A5", 1), ("G5", 2), ("F5", 1), ("E5", 1)],
    [("E5", 3), ("D5", 1), ("C5", 2), ("G4", 2)],
    [("D5", 2), ("A4", 1), ("D5", 1), ("F5", 2), ("A5", 2)],
    [("Bb5", 2), ("A5", 1), ("G5", 1), ("F5", 2), ("D5", 2)],
    [("E5", 1), ("F5", 1), ("G5", 2), ("E5", 2), ("C5", 2)],
    [("C#5", 2), ("E5", 2), ("A5", 4)],
]
SECTION_B = [
    [("G5", 3), ("F5", 1), ("D5", 2), ("Bb4", 2)],
    [("A4", 2), ("D5", 2), ("F5", 3), ("E5", 1)],
    [("D5", 2), ("F5", 2), ("Bb5", 3), ("A5", 1)],
    [("A5", 2), ("G5", 1), ("F5", 1), ("C5", 4)],
    [("Bb4", 1), ("D5", 1), ("G5", 2), ("A5", 1), ("Bb5", 1), ("A5", 2)],
    [("F5", 2), ("E5", 1), ("D5", 1), ("A5", 4)],
    [("G5", 2), ("F5", 2), ("E5", 2), ("G5", 2)],
    [("A5", 2), ("G#5", 1), ("A5", 1), ("E5", 2), (None, 2)],
]
# Chords per bar (root for the bass, tones for the arpeggio and pad).
CHORDS_A = [("D", ["D", "F", "A"]), ("Bb", ["Bb", "D", "F"]), ("F", ["F", "A", "C"]), ("C", ["C", "E", "G"]),
            ("D", ["D", "F", "A"]), ("Bb", ["Bb", "D", "F"]), ("C", ["C", "E", "G"]), ("A", ["A", "C#", "E"])]
CHORDS_B = [("G", ["G", "Bb", "D"]), ("D", ["D", "F", "A"]), ("Bb", ["Bb", "D", "F"]), ("F", ["F", "A", "C"]),
            ("G", ["G", "Bb", "D"]), ("D", ["D", "F", "A"]), ("C", ["C", "E", "G"]), ("A", ["A", "C#", "E"])]


def render_music():
    sections = [(SECTION_A, CHORDS_A, False), (SECTION_B, CHORDS_B, False),
                (SECTION_A, CHORDS_A, True), (SECTION_B, CHORDS_B, True)]
    bars = sum(len(melody) for melody, _, _ in sections)
    length = int(bars * BAR * RATE)
    tail = int(2.0 * RATE)
    lead = [0.0] * (length + tail)
    backing = [0.0] * (length + tail)

    bar_index = 0
    for melody, chords, b_variation in sections:
        for bar, (root, tones) in zip(melody, chords):
            bar_start = bar_index * BAR
            # Lead: a soft pulse with vibrato; the second pass doubles it an octave up, quietly.
            at = 0
            for name, eighths in bar:
                if name is not None:
                    seconds = eighths * EIGHTH * 0.92
                    note = tone(note_hz(name), seconds, "square", attack=0.01, volume=0.11, duty=0.3, vibrato=0.006)
                    add(note, tone(note_hz(name), seconds, "triangle", attack=0.01, volume=0.1))
                    if b_variation:
                        add(note, tone(note_hz(name) * 2.0, seconds, "triangle", attack=0.01, volume=0.035))
                    add(lead, note, int((bar_start + at * EIGHTH) * RATE))
                at += eighths
            # Bass: driving eighths, root and octave.
            for step in range(8):
                octave = 2 if step % 2 == 0 else 3
                bass = tone(note_hz(f"{root}{octave}"), EIGHTH * 0.85, "triangle", attack=0.004, decay=3.0,
                            volume=0.32)
                add(backing, bass, int((bar_start + step * EIGHTH) * RATE))
            # Pad: the chord held through the bar, two detuned saws, low.
            for name in tones:
                freq = note_hz(f"{name}4")
                for detune in (0.997, 1.003):
                    pad = tone(freq * detune, BAR, "saw", attack=0.25, volume=0.018)
                    add(backing, pad, int(bar_start * RATE))
            # Arpeggio on the second pass: sixteenths over the chord tones.
            if b_variation:
                pattern = [f"{tones[0]}5", f"{tones[1]}5", f"{tones[2]}5", f"{tones[1]}5"]
                for step in range(16):
                    arp = tone(note_hz(pattern[step % 4]), EIGHTH * 0.45, "square", decay=14.0, volume=0.045,
                               duty=0.2)
                    add(backing, arp, int((bar_start + step * EIGHTH / 2.0) * RATE))
            # Drums: kick on 1 and 3, snare on 2 and 4, hats on eighths, an open hat before the next bar.
            for beat in range(4):
                beat_at = int((bar_start + beat * EIGHTH * 2) * RATE)
                if beat % 2 == 0:
                    add(backing, tone(130.0, 0.25, "sine", decay=14.0, volume=0.55, sweep_to=42.0), beat_at)
                else:
                    snare = noise(0.18, 18.0, 0.22, seed=bar_index * 4 + beat, lowpass=0.6)
                    add(snare, tone(190.0, 0.1, "triangle", decay=30.0, volume=0.18))
                    add(backing, snare, beat_at)
            for step in range(8):
                hat_len = 0.12 if step == 7 else 0.035
                hat = noise(hat_len, 60.0 if step != 7 else 14.0, 0.08, seed=1000 + bar_index * 8 + step, lowpass=1.0)
                # A crude high-pass: subtract the smoothed signal.
                smooth = 0.0
                for i in range(len(hat)):
                    smooth += (hat[i] - smooth) * 0.25
                    hat[i] -= smooth
                add(backing, hat, int((bar_start + step * EIGHTH) * RATE))
            bar_index += 1

    # Echo on the lead (a dotted-eighth delay), then mix.
    delay = int(EIGHTH * 1.5 * RATE)
    for i in range(delay, len(lead)):
        lead[i] += lead[i - delay] * 0.28
    mixed = [a + b for a, b in zip(lead, backing)]
    # Wrap the tail into the start so the loop point is seamless.
    for i in range(tail):
        mixed[i] += mixed[length + i]
    mixed = mixed[:length]
    peak = max(abs(s) for s in mixed)
    return [s / peak * 0.8 for s in mixed]


def main():
    write_blocks()
    write_fx()
    write_backdrop()
    write_frames()
    write_atlas_json("atlas_blocks.json", BLOCK_TILE, BLOCK_TILE, 8, 2, 16)
    write_atlas_json("atlas_fx.json", FX_TILE, FX_TILE, 8, 1, 8)
    write_atlas_json("atlas_backdrop.json", 360, 640, 1, 1, 1)
    write_atlas_json("atlas_well.json", 352, 672, 1, 1, 1)
    write_atlas_json("atlas_box.json", 128, 128, 1, 1, 1)
    write_sfx()
    write_wav("music.wav", render_music())
    print("Wrote Prism Drop assets to", os.path.normpath(ROOT))


if __name__ == "__main__":
    main()
