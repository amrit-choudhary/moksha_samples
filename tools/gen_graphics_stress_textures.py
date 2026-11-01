"""
Writes the graphics stress sample's textures (RGBA PNGs) with the standard library only. White pixels, since the
sample tints each sprite. Alpha never falls below the sprite shaders' 0.1 discard level inside a shape.
Usage: python tools/gen_graphics_stress_textures.py   (from the repo root; overwrites the sample's textures/galaxy/*.png)
"""

import math
import os
import random
import struct
import zlib

OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "graphics_stress", "resources", "textures", "galaxy")
CLOUD_TILE = 256
STAR_TILE = 32
TILES = 4
DISCARD_ALPHA = 0.1


def write_png(name, width, height, alpha):
    """alpha is width * height floats in [0, 1], row by row from the top."""
    rows = bytearray()
    for y in range(height):
        rows.append(0)  # PNG filter: none
        for x in range(width):
            rows += bytes((255, 255, 255, round(alpha[y * width + x] * 255)))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, name), "wb") as out:
        out.write(b"\x89PNG\r\n\x1a\n")
        out.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)))
        out.write(chunk(b"IDAT", zlib.compress(bytes(rows), 9)))
        out.write(chunk(b"IEND", b""))


def make_noise(rng, size):
    """A smooth value noise lookup over [0, size) squared."""
    grid = [[rng.random() for _ in range(size + 1)] for _ in range(size + 1)]

    def smooth(t):
        return t * t * (3.0 - 2.0 * t)

    def noise(x, y):
        ix, iy = int(x), int(y)
        fx, fy = smooth(x - ix), smooth(y - iy)
        top = grid[iy][ix] + (grid[iy][ix + 1] - grid[iy][ix]) * fx
        bottom = grid[iy + 1][ix] + (grid[iy + 1][ix + 1] - grid[iy + 1][ix]) * fx
        return top + (bottom - top) * fy

    return noise


def cloud_tile(seed):
    """A soft puff: bright centre, billowing noise, and an edge cut at the discard level."""
    rng = random.Random(seed)
    octaves = [(make_noise(rng, cells), cells, weight) for cells, weight in ((4, 0.55), (8, 0.3), (16, 0.15))]
    alpha = [0.0] * (CLOUD_TILE * CLOUD_TILE)
    for y in range(CLOUD_TILE):
        v = (y + 0.5) / CLOUD_TILE
        for x in range(CLOUD_TILE):
            u = (x + 0.5) / CLOUD_TILE
            radius = math.hypot(u - 0.5, v - 0.5) * 2.0
            billow = sum(noise(u * cells, v * cells) * weight for noise, cells, weight in octaves)
            shape = 1.0 - radius - (1.0 - billow) * 0.45
            if shape <= 0.0:
                continue
            alpha[y * CLOUD_TILE + x] = DISCARD_ALPHA + (1.0 - DISCARD_ALPHA) * min(1.0, shape * 2.0) ** 1.5
    return alpha


def star_tile(index):
    """Four star shapes: soft dot, sharp dot, four-point flare, wide glow."""
    alpha = [0.0] * (STAR_TILE * STAR_TILE)
    for y in range(STAR_TILE):
        dy = (y + 0.5) / STAR_TILE * 2.0 - 1.0
        for x in range(STAR_TILE):
            dx = (x + 0.5) / STAR_TILE * 2.0 - 1.0
            radius = math.hypot(dx, dy)
            if index == 0:
                value = 1.0 - radius
            elif index == 1:
                value = 1.0 if radius < 0.45 else (1.0 - radius) * 1.2
            elif index == 2:
                core = max(0.0, 1.0 - radius * 2.2)
                value = core + max(0.0, 1.0 - radius) * max(0.0, 1.0 - min(abs(dx), abs(dy)) * 9.0)
            else:
                value = 0.45 * (1.0 - radius) + max(0.0, 1.0 - radius * 3.0)
            value = min(1.0, value)
            alpha[y * STAR_TILE + x] = value if value >= DISCARD_ALPHA else 0.0
    return alpha


def join_tiles(tiles, tile_size):
    joined = [0.0] * (tile_size * TILES * tile_size)
    for index, tile in enumerate(tiles):
        for y in range(tile_size):
            start = y * tile_size * TILES + index * tile_size
            joined[start:start + tile_size] = tile[y * tile_size:(y + 1) * tile_size]
    return joined


def main():
    clouds = join_tiles([cloud_tile(seed) for seed in range(TILES)], CLOUD_TILE)
    write_png("clouds.png", CLOUD_TILE * TILES, CLOUD_TILE, clouds)
    stars = join_tiles([star_tile(index) for index in range(TILES)], STAR_TILE)
    write_png("stars.png", STAR_TILE * TILES, STAR_TILE, stars)


if __name__ == "__main__":
    main()
