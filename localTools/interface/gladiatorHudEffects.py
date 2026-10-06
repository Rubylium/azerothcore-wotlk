"""Seamless motion assembled from the AI-painted ember sprites, with no independently generated frames."""

import math
import random

import numpy as np
from PIL import Image, ImageChops

from gladiatorHudTextures import cleanBlack


def extractSprites(source):
    source = cleanBlack(source)
    cell = source.crop((0, 0, source.width // 8, source.height // 8))
    pixels = np.asarray(cell)
    bright = pixels.max(axis=2) > 90
    seen = set()
    sprites = []
    for y, x in zip(*np.nonzero(bright)):
        if (x, y) in seen:
            continue
        pending = [(x, y)]
        points = []
        while pending:
            px, py = pending.pop()
            if (px, py) in seen or px < 0 or py < 0 or px >= cell.width or py >= cell.height:
                continue
            seen.add((px, py))
            if not bright[py, px]:
                continue
            points.append((px, py))
            pending.extend(((px - 1, py), (px + 1, py), (px, py - 1), (px, py + 1)))
        if not points:
            continue
        left, top = min(px for px, py in points), min(py for px, py in points)
        right, bottom = max(px for px, py in points) + 1, max(py for px, py in points) + 1
        if right - left > 10 or bottom - top > 10:
            continue
        sprites.append(cell.crop((max(0, left - 2), max(0, top - 2),
                                  min(cell.width, right + 2), min(cell.height, bottom + 2))))
    if not sprites:
        raise ValueError("No painted ember sprites found")
    return sprites


def emberFrames(source):
    sprites = extractSprites(source)
    rng = random.Random(335)
    particles = [(rng.randrange(len(sprites)), rng.random(), rng.uniform(0, math.tau),
                  rng.uniform(88, 100), rng.uniform(16, 30), rng.uniform(-9, 9), rng.uniform(0.3, 1.0))
                 for _ in range(90)]
    frames = []
    for index in range(64):
        frame = Image.new("RGB", (256, 256), 0)
        for spriteIndex, offset, angle, radius, rise, drift, intensity in particles:
            phase = (offset + index / 64) % 1
            fade = math.sin(math.pi * phase) ** 2 * intensity
            sprite = sprites[spriteIndex].point(lambda value: round(value * fade))
            x = round(128 + math.cos(angle) * radius + drift * math.sin(math.tau * phase))
            y = round(128 + math.sin(angle) * radius - rise * phase)
            layer = Image.new("RGB", frame.size, 0)
            layer.paste(sprite, (x - sprite.width // 2, y - sprite.height // 2))
            frame = ImageChops.add(frame, layer)
        frames.append(frame)
    return frames
