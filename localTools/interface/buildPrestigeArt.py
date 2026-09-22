"""Compiles the prestige frame art into the BLP2 files the 3.3.5 client loads.

The medallion is painted once and keyed here: black outside the ring becomes
transparent, the charcoal center stays so the prestige count can sit on it.
The glow is built in code, the same way Paragon-Node-Glow is, because a smooth
falloff is a gradient and not a painting.

Usage: python localTools/interface/buildPrestigeArt.py [medallion-source]
"""
import importlib.util
import math
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(REPO, "clientPatcher", "interface", "Interface", "Prestige")
SIZE = 256


def load_art():
    path = os.path.join(HERE, "buildParagonArt.py")
    spec = importlib.util.spec_from_file_location("buildParagonArt", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def key_background(image, threshold=40):
    """Flood-fills near-black from the edges. The ring seals the center, so the disc stays."""
    image = image.convert("RGBA")
    pixels = image.load()
    width, height = image.size
    seen = bytearray(width * height)
    stack = []

    def push(x, y):
        if x < 0 or y < 0 or x >= width or y >= height:
            return
        index = y * width + x
        if seen[index]:
            return
        seen[index] = 1
        red, green, blue, _alpha = pixels[x, y]
        if max(red, green, blue) <= threshold:
            stack.append((x, y))

    for x in range(width):
        push(x, 0)
        push(x, height - 1)
    for y in range(height):
        push(0, y)
        push(width - 1, y)

    while stack:
        x, y = stack.pop()
        red, green, blue, _alpha = pixels[x, y]
        pixels[x, y] = (red, green, blue, 0)
        push(x - 1, y)
        push(x + 1, y)
        push(x, y - 1)
        push(x, y + 1)

    return image


def fit_square(image):
    bounds = image.getchannel("A").point(lambda value: 255 if value > 8 else 0).getbbox()
    if not bounds:
        raise SystemExit("medallion keyed to nothing")
    image = image.crop(bounds)
    side = max(image.size)
    pad = side // 16
    canvas = Image.new("RGBA", (side + pad * 2, side + pad * 2), (0, 0, 0, 0))
    canvas.paste(image, ((canvas.width - image.width) // 2, (canvas.height - image.height) // 2), image)
    return canvas.resize((SIZE, SIZE), Image.Resampling.LANCZOS)


def build_glow():
    image = Image.new("RGBA", (SIZE, SIZE))
    pixels = image.load()
    center = (SIZE - 1) / 2
    radius = center
    for y in range(SIZE):
        for x in range(SIZE):
            distance = ((x - center) ** 2 + (y - center) ** 2) ** 0.5 / radius
            if distance >= 1:
                continue
            falloff = (1 - distance) ** 2
            pixels[x, y] = (255, 196, 72, int(210 * falloff))
    return image


# The halo behind the medallion. A texture cannot be rotated in 3.3.5, so the rotation is baked into a
# sequence of frames and the frame cycles them, the same trick the paragon links use. The spokes repeat every
# 360/RAY_SPOKES degrees, so the sequence only has to cover one wedge before it loops seamlessly.
RAY_SIZE = 128
RAY_SPOKES = 12
RAY_FRAMES = 24
RAY_INNER = 0.34        # where a spoke starts, as a share of the radius
RAY_OUTER = 0.98        # and where it fades out
RAY_SHARPNESS = 3.0     # how quickly a spoke narrows away from its centre line


def build_ray_frame(turn):
    """One frame of the halo, rotated `turn` degrees."""
    image = Image.new("RGBA", (RAY_SIZE, RAY_SIZE))
    pixels = image.load()
    center = (RAY_SIZE - 1) / 2
    step = 360.0 / RAY_SPOKES

    for y in range(RAY_SIZE):
        for x in range(RAY_SIZE):
            dx, dy = x - center, y - center
            distance = (dx * dx + dy * dy) ** 0.5 / center
            if distance >= RAY_OUTER or distance <= RAY_INNER:
                continue

            # how far this pixel sits from the nearest spoke's centre line, 0 at the line and 1 half way
            # to the next one
            angle = (math.degrees(math.atan2(dy, dx)) - turn) % step
            offset = abs(angle - step / 2) / (step / 2)
            across = offset ** RAY_SHARPNESS

            # fade in off the inner edge and out towards the rim, so the spokes have no hard ends
            span = RAY_OUTER - RAY_INNER
            along = (distance - RAY_INNER) / span
            length = (1.0 - along) * min(1.0, along * 6.0)

            alpha = int(150 * across * length)
            if alpha > 0:
                pixels[x, y] = (255, 214, 120, alpha)
    return image


DIVIDER_WIDTH = 256
DIVIDER_HEIGHT = 4


def build_divider():
    """A gold hairline that fades out at both ends."""
    image = Image.new("RGBA", (DIVIDER_WIDTH, DIVIDER_HEIGHT))
    pixels = image.load()
    middle = (DIVIDER_HEIGHT - 1) / 2.0

    for x in range(DIVIDER_WIDTH):
        # 1 in the middle of the run, 0 at either end, squared so the ends disappear rather than stop
        along = 1.0 - abs(x - (DIVIDER_WIDTH - 1) / 2.0) / ((DIVIDER_WIDTH - 1) / 2.0)
        fade = along ** 0.65
        for y in range(DIVIDER_HEIGHT):
            across = 1.0 - min(1.0, abs(y - middle) / (middle + 0.5))
            alpha = int(215 * fade * across)
            if alpha > 0:
                pixels[x, y] = (226, 194, 120, alpha)
    return image


def build_arrow():
    """The chevron between the current paragon cap and the next one.

    It is art rather than text because the client's fonts have no arrow: U+2192 came out as a question mark
    in game, which is what sent this frame back for another pass.
    """
    width, height = 32, 32
    image = Image.new("RGBA", (width, height))
    pixels = image.load()
    thickness = 3.2

    for y in range(height):
        for x in range(width):
            # two strokes meeting at the right, plus a shaft, measured as distance to each segment
            fx, fy = x + 0.5, y + 0.5
            tip = (width - 7.0, height / 2.0)
            best = min(
                point_to_segment(fx, fy, tip[0], tip[1], tip[0] - 9.0, tip[1] - 9.0),
                point_to_segment(fx, fy, tip[0], tip[1], tip[0] - 9.0, tip[1] + 9.0),
                point_to_segment(fx, fy, tip[0], tip[1], 6.0, tip[1]),
            )
            if best <= thickness:
                edge = min(1.0, (thickness - best) / 1.2)
                pixels[x, y] = (255, 210, 110, int(255 * edge))
    return image


def point_to_segment(px, py, ax, ay, bx, by):
    """Shortest distance from a point to a line segment, for drawing the chevron's strokes."""
    vx, vy = bx - ax, by - ay
    length = vx * vx + vy * vy
    if length <= 0:
        return ((px - ax) ** 2 + (py - ay) ** 2) ** 0.5
    t = max(0.0, min(1.0, ((px - ax) * vx + (py - ay) * vy) / length))
    return ((px - ax - t * vx) ** 2 + (py - ay - t * vy) ** 2) ** 0.5


def main():
    source = sys.argv[1] if len(sys.argv) > 1 else os.path.join(OUT, "Prestige-Emblem.png")
    if not os.path.isfile(source):
        raise SystemExit("no medallion source at %s" % source)

    os.makedirs(OUT, exist_ok=True)
    art = load_art()

    medallion = fit_square(key_background(Image.open(source)))
    medallion.save(os.path.join(OUT, "Prestige-Emblem.png"))
    art.writeRawBlp(medallion, os.path.join(OUT, "Prestige-Emblem.blp"))

    glow = build_glow()
    glow.save(os.path.join(OUT, "Prestige-Emblem-Glow.png"))
    art.writeRawBlp(glow, os.path.join(OUT, "Prestige-Emblem-Glow.blp"))
    divider = build_divider()
    divider.save(os.path.join(OUT, "Prestige-Divider.png"))
    art.writeRawBlp(divider, os.path.join(OUT, "Prestige-Divider.blp"))

    arrow = build_arrow()
    arrow.save(os.path.join(OUT, "Prestige-Arrow.png"))
    art.writeRawBlp(arrow, os.path.join(OUT, "Prestige-Arrow.blp"))

    step = (360.0 / RAY_SPOKES) / RAY_FRAMES
    for index in range(RAY_FRAMES):
        frame = build_ray_frame(index * step)
        name = "Prestige-Rays-%02d" % index
        frame.save(os.path.join(OUT, name + ".png"))
        art.writeRawBlp(frame, os.path.join(OUT, name + ".blp"))

    print("Built prestige emblem, glow, arrow and %d halo frames in %s" % (RAY_FRAMES, OUT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
