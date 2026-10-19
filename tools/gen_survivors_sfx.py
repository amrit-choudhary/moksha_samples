"""
Writes the survivors sample's sound effects (short 16-bit mono WAVs) with the standard library only.
Usage: python tools/gen_survivors_sfx.py   (from the repo root; overwrites survivors/resources/audio/*.wav)
"""

import math
import os
import random
import struct
import wave

RATE = 22050
OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "survivors", "resources", "audio")


def write_wav(name, samples):
    os.makedirs(OUT_DIR, exist_ok=True)
    with wave.open(os.path.join(OUT_DIR, name), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s)) * 32767)) for s in samples))


def shoot():
    # Square wave sweeping down from 1200 Hz to 300 Hz, fading out.
    length = int(RATE * 0.08)
    phase = 0.0
    samples = []
    for i in range(length):
        t = i / length
        phase += (1200.0 - 900.0 * t) / RATE
        samples.append((0.25 if (phase % 1.0) < 0.5 else -0.25) * (1.0 - t))
    return samples


def hit():
    # Noise burst with a fast decay.
    rng = random.Random(7)
    length = int(RATE * 0.06)
    return [rng.uniform(-0.3, 0.3) * math.exp(-6.0 * i / length) for i in range(length)]


if __name__ == "__main__":
    write_wav("shoot.wav", shoot())
    write_wav("hit.wav", hit())
    print("Wrote shoot.wav and hit.wav to", os.path.normpath(OUT_DIR))
