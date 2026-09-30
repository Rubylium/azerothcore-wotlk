"""Draws the gems round a Hollow Voice tower's rim (HollowVoice.cpp Towers): one a soaker it asks for, an empty socket
until someone stands in the tower, lit for each who does. Gold for the Archbishop's towers, violet for Vel'thazar's.

    python localTools/hollowVoice/buildPips.py

Writes localTools/hollowVoice/textures/pip{Holy,Void}{Empty,Lit}.png (256 x 256, transparent), which
localTools/groundIndicators/shapes.json paints on the ground (kind texture, circle).
"""
import math
import os

import numpy
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SIZE = 256
COLOURS = {
    'Holy': ((255, 214, 120), (255, 248, 225)),     # the gem's colour, its white-hot core
    'Void': ((150, 90, 255), (235, 215, 255)),
}


def gem(colour, core, lit):
    y, x = numpy.mgrid[0:SIZE, 0:SIZE].astype(float)
    u = (x - SIZE / 2 + 0.5) / (SIZE / 2)
    v = (y - SIZE / 2 + 0.5) / (SIZE / 2)
    radius = numpy.hypot(u, v)
    # A diamond (a square turned 45 degrees): |u| + |v| inside 0.55
    diamond = numpy.abs(u) + numpy.abs(v)
    edge = 0.55
    inside = numpy.clip((edge - diamond) / 0.03, 0.0, 1.0)
    rim = numpy.clip(1.0 - numpy.abs(diamond - edge) / 0.045, 0.0, 1.0)
    # Facets: the four triangles shaded apart
    facet = 0.75 + 0.25 * numpy.sign(u) * numpy.sign(v) * 0.5 + 0.12 * numpy.sign(v)
    rgb = numpy.zeros((SIZE, SIZE, 3))
    alpha = numpy.zeros((SIZE, SIZE))
    colour = numpy.array(colour, float) / 255.0
    core = numpy.array(core, float) / 255.0
    if lit:
        # A bright gem, hot at its heart, and a soft glow round it
        heat = numpy.clip(1.0 - diamond / edge, 0.0, 1.0) ** 1.5
        body = colour[None, None, :] * facet[..., None] * (1.0 - heat[..., None]) + core[None, None, :] * heat[..., None]
        glow = numpy.clip(1.0 - radius / 0.95, 0.0, 1.0) ** 2.2 * 0.8
        rgb = body * inside[..., None] + colour[None, None, :] * (1.0 - inside[..., None])
        alpha = numpy.maximum(inside, glow)
        alpha = numpy.maximum(alpha, rim)
        rgb = rgb * (1.0 - rim[..., None]) + core[None, None, :] * rim[..., None]
    else:
        # An empty socket: a dark stone, its rim faintly of the gem's colour
        stone = numpy.array((0.10, 0.09, 0.11)) * facet[..., None]
        rgb = stone * (1.0 - rim[..., None]) + colour[None, None, :] * 0.8 * rim[..., None]
        alpha = numpy.maximum(inside * 0.75, rim * 0.95)
    # Clear border
    alpha[:3, :] = alpha[-3:, :] = 0.0
    alpha[:, :3] = alpha[:, -3:] = 0.0
    out = numpy.dstack([numpy.clip(rgb, 0, 1) * 255, numpy.clip(alpha, 0, 1) * 255]).astype(numpy.uint8)
    return Image.fromarray(out, 'RGBA')


def main():
    for name, (colour, core) in COLOURS.items():
        for lit in (False, True):
            path = os.path.join(HERE, 'textures', f"pip{name}{'Lit' if lit else 'Empty'}.png")
            gem(colour, core, lit).save(path)
            print(path)


if __name__ == '__main__':
    main()
