"""The FX lab's painted marks, as the ground indicator builder draws them (localTools/groundIndicators, shapes.json
`picture`: "localTools/fxLab/labArt.py:<function>").

- rings: the range rings round the lab's middle (FxLab.cpp lays them there on .fxlab), every 5 yards out to 40, the
  tens stronger - drawn smooth here (the terrain's blend maps drew them half a yard a pixel: jagged).
"""
import math

from PIL import Image, ImageDraw

# The picture spans RingsReach yards from its middle (shapes.json FxLabRings is laid at that radius)
RINGS_REACH = 41.0
RINGS_SIZE = 2048
SUPERSAMPLE = 2


def rings():
    """Rings every 5 yards out to 40 (the tens wider and brighter) and a small cross at the middle, white on clear,
    drawn at twice the size and scaled down (smooth edges). RGBA, square."""
    size = RINGS_SIZE * SUPERSAMPLE
    image = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    centre = size / 2.0
    per_yard = centre / RINGS_REACH
    for yards in range(5, 45, 5):
        tens = yards % 10 == 0
        radius = yards * per_yard
        width = max(2, int(round((0.28 if tens else 0.14) * per_yard)))
        alpha = 230 if tens else 150
        draw.ellipse((centre - radius, centre - radius, centre + radius, centre + radius),
                     outline=(255, 255, 255, alpha), width=width)
    arm = 1.0 * per_yard
    width = max(2, int(round(0.14 * per_yard)))
    draw.line((centre - arm, centre, centre + arm, centre), fill=(255, 255, 255, 230), width=width)
    draw.line((centre, centre - arm, centre, centre + arm), fill=(255, 255, 255, 230), width=width)
    return image.resize((RINGS_SIZE, RINGS_SIZE), Image.LANCZOS)
