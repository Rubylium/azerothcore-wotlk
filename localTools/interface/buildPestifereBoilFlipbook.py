"""The Pestiféré HUD's Avatar de la peste flipbook: bubbles boiling up through a flask's liquid, popping at its
surface.

Usage: python localTools/interface/buildPestifereBoilFlipbook.py
Requires Pillow and NumPy. Rendered, not painted: clipped to the liquid hollow the art pipeline measured
(clientPatcher/assets/pestifereHud/flaskFillMask.png, on the flask's 132 x 120 canvas), a seamless loop (every motion
completes whole cycles in it), light only and colourless - the Lua tints it per flask (SetVertexColor) and draws it
additive over the liquid. 28 frames of 132 x 120 on a 1024 x 512 sheet, 7 columns by 4 rows, played at 14 fps.
"""

import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

from buildParagonArt import writeRawBlp


repoRoot = Path(__file__).resolve().parents[2]
assetRoot = repoRoot / "clientPatcher/assets/pestifereHud"
blpPath = repoRoot / "clientPatcher/interface/Interface/ClassHud/pestifereBoilFlipbook.blp"

CELL = (132, 120)
COLUMNS, ROWS = 7, 4
FRAMES = COLUMNS * ROWS
SUPER = 3
SEED = 9028


def bubble(canvas, x, y, radius, strength):
    """A bubble as light: a bright rim, a faint body, a highlight up and to the left (all at SUPER scale)"""
    if radius <= 0.2 or strength <= 0:
        return
    height, width = canvas.shape
    reach = int(radius + 4 * SUPER)
    left, right = max(0, int(x) - reach), min(width, int(x) + reach + 1)
    top, bottom = max(0, int(y) - reach), min(height, int(y) + reach + 1)
    if left >= right or top >= bottom:
        return
    ys, xs = np.mgrid[top:bottom, left:right].astype(float)
    distance = np.hypot(xs - x, ys - y)
    rimWidth = max(0.9 * SUPER, radius * 0.22)
    rim = np.exp(-((distance - radius) / rimWidth) ** 2)
    body = np.clip(1 - distance / radius, 0, 1) * 0.18
    glintDistance = np.hypot(xs - (x - radius * 0.38), ys - (y - radius * 0.38))
    glint = np.exp(-(glintDistance / max(0.6 * SUPER, radius * 0.22)) ** 2)
    canvas[top:bottom, left:right] += strength * (0.75 * rim + body + 0.9 * glint)


def popRing(canvas, x, y, radius, strength):
    """A bubble bursting at the surface: a thin ring spreading out"""
    if strength <= 0:
        return
    height, width = canvas.shape
    reach = int(radius + 3 * SUPER)
    left, right = max(0, int(x) - reach), min(width, int(x) + reach + 1)
    top, bottom = max(0, int(y) - reach), min(height, int(y) + reach + 1)
    ys, xs = np.mgrid[top:bottom, left:right].astype(float)
    distance = np.hypot(xs - x, (ys - y) * 1.8)
    canvas[top:bottom, left:right] += strength * np.exp(-((distance - radius) / (0.8 * SUPER)) ** 2)


def main():
    mask = np.asarray(Image.open(assetRoot / "flaskFillMask.png").convert("L")).astype(float) / 255
    rows = np.nonzero(mask.max(axis=1) > 0.5)[0]
    surface, floor = int(rows.min()), int(rows.max())
    spans = {}
    for row in range(surface, floor + 1):
        columns = np.nonzero(mask[row] > 0.5)[0]
        spans[row] = (int(columns.min()), int(columns.max())) if len(columns) else None

    random = np.random.default_rng(SEED)
    # Rising bubbles: each completes 1 or 2 rises per loop, so the loop is seamless
    risers = []
    for _ in range(30):
        risers.append({
            "lane": random.uniform(0.08, 0.92), "radius": random.uniform(1.6, 5.2),
            "cycles": int(random.choice([1, 2])), "phase": random.uniform(0, 1), "wobble": random.uniform(0.6, 2.2),
            "wobbleCycles": int(random.integers(1, 4)), "wobblePhase": random.uniform(0, 1),
        })
    # Surface froth: small bubbles swelling and bursting along the surface
    froth = []
    for _ in range(14):
        froth.append({
            "lane": random.uniform(0.1, 0.9), "radius": random.uniform(1.2, 2.8),
            "cycles": int(random.choice([2, 3, 4])), "phase": random.uniform(0, 1),
        })

    width, height = CELL[0] * SUPER, CELL[1] * SUPER
    sheet = Image.new("RGBA", (1024, 512), (0, 0, 0, 255))
    previews = []
    yy, xx = np.mgrid[0:CELL[1], 0:CELL[0]].astype(float)
    for frame in range(FRAMES):
        t = frame / FRAMES
        canvas = np.zeros((height, width))
        for bubbleInfo in risers:
            progress = (bubbleInfo["phase"] + bubbleInfo["cycles"] * t) % 1
            radius = bubbleInfo["radius"] * (0.75 + 0.45 * progress)
            y = floor + 1 - progress * (floor - surface + 1)
            span = spans.get(int(round(y)))
            if not span:
                continue
            x = span[0] + radius + (span[1] - span[0] - 2 * radius) * bubbleInfo["lane"]
            sway = bubbleInfo["wobbleCycles"] * t + bubbleInfo["wobblePhase"]
            x += bubbleInfo["wobble"] * math.sin(2 * math.pi * sway)
            fadeIn = min(1, progress / 0.12)
            if progress > 0.9:
                # The last tenth: it bursts at the surface
                popRing(canvas, x * SUPER, (surface + 1.5) * SUPER, radius * SUPER * (1 + (progress - 0.9) * 14),
                        0.9 * (1 - (progress - 0.9) / 0.1))
                continue
            bubble(canvas, x * SUPER, y * SUPER, radius * SUPER, fadeIn)
        for foam in froth:
            cycle = (foam["phase"] + foam["cycles"] * t) % 1
            span = spans[surface + 2]
            x = span[0] + (span[1] - span[0]) * foam["lane"]
            if cycle < 0.8:
                bubble(canvas, x * SUPER, (surface + 2.5) * SUPER, foam["radius"] * SUPER * (cycle / 0.8), 0.8)
            else:
                popRing(canvas, x * SUPER, (surface + 2.5) * SUPER, foam["radius"] * SUPER * (1 + (cycle - 0.8) * 8),
                        0.8 * (1 - (cycle - 0.8) / 0.2))
        frameImage = Image.fromarray(np.clip(canvas * 255, 0, 255).astype(np.uint8)).resize(CELL, Image.Resampling.BOX)
        light = np.asarray(frameImage).astype(float) / 255
        # The liquid stirred: a soft brightness rolling upwards, whole cycles in the loop
        shimmer = 0.07 * (0.5 + 0.5 * np.sin(2 * math.pi * (yy / 22 + 2 * t) + np.sin(2 * math.pi * (xx / 37 - t))))
        light = (light + shimmer) * mask
        value = np.clip(light * 255, 0, 255).astype(np.uint8)
        cell = Image.fromarray(value).filter(ImageFilter.GaussianBlur(0.35))
        rgba = Image.merge("RGBA", (cell, cell, cell, Image.new("L", CELL, 255)))
        column, row = frame % COLUMNS, frame // COLUMNS
        sheet.paste(rgba, (column * CELL[0], row * CELL[1]))
        previews.append(cell)
    sheet.save(assetRoot / "pestifereBoilFlipbook.png")
    writeRawBlp(sheet, str(blpPath))
    # A preview of the loop, tinted bile over the full bile flask, enlarged
    flask = Image.open(assetRoot / "png/flaskBile.png").convert("RGBA")
    frames = []
    for cell in previews:
        tint = Image.merge("RGB", (cell.point(lambda v: v * 0.85), cell, cell.point(lambda v: v * 0.35)))
        base = Image.new("RGB", CELL, (31, 27, 22))
        base.paste(flask, (0, 0), flask)
        added = np.asarray(base).astype(int) + np.asarray(tint).astype(int)
        lit = Image.fromarray(np.clip(added, 0, 255).astype(np.uint8))
        frames.append(lit.resize((CELL[0] * 3, CELL[1] * 3), Image.Resampling.LANCZOS))
    frames[0].save(assetRoot / "boilFlipbookPreview.gif", save_all=True, append_images=frames[1:], duration=71, loop=0)
    print("Exported", FRAMES, "frames,", COLUMNS, "x", ROWS, "of", CELL, "- 1024x512 PNG/BLP, boilFlipbookPreview.gif")


if __name__ == "__main__":
    main()
