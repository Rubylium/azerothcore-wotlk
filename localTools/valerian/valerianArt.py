"""Valérian's painted looks, as the ground indicator builder draws them (localTools/groundIndicators, shapes.json
`picture`: "localTools/valerian/valerianArt.py:<function>"; ZealotBoss.cpp shows them).

The paintings are art/*.png (the brief: .agents/plans/gaol-third/gaol-third.ASSETS.md). As Vrogar's
(localTools/escapeHunter/vrogarArt.py, whose shape grids this reuses): the floor's fills (radiant flagstones, the
stone about to burst) are only a material, each shape cut out of one here with its edge glowing gold, the layer that
moves (their glow alone) a second look over the same shape; the other pieces are painted as their shape and keyed.

Each function returns one picture.
"""
import importlib.util
import math
import os

import numpy
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.path.join(HERE, 'art')

spec = importlib.util.spec_from_file_location('vrogarArt', os.path.join(HERE, '..', 'escapeHunter', 'vrogarArt.py'))
vrogar = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vrogar)
smooth, image, keyed, tiled, grid_of, rect_grid = (vrogar.smooth, vrogar.image, vrogar.keyed, vrogar.tiled,
                                                  vrogar.grid_of, vrogar.rect_grid)

# A fill's alpha inside its shape, its rim: a white-gold line along the edge and a softer glow inward (yards of a
# one-yard model)
FILL_ALPHA = 0.9
RIM_COLOUR = numpy.array([1.0, 0.86, 0.5])
RIM_LINE = 0.010
RIM_GLOW = 0.045
# A painted fill's own edges are smeared (the generator's seam): this much is cut off each side before it is made to
# tile again
SEAM_MARGIN = 40


def load(name):
    return numpy.asarray(Image.open(os.path.join(ART, name)).convert('RGB'), dtype=numpy.float64) / 255.0


_seamless = {}


def seamless(name):
    """A fill made to tile: its smeared edges cut off, then cross-faded with itself shifted half a tile, so that its
    edges are the middle's (no seam where tiles meet)"""
    if name in _seamless:
        return _seamless[name]
    texture = load(name)[SEAM_MARGIN:-SEAM_MARGIN, SEAM_MARGIN:-SEAM_MARGIN]
    size = texture.shape[0]
    shifted = numpy.roll(numpy.roll(texture, size // 2, axis=0), size // 2, axis=1)
    ticks = (numpy.arange(size) + 0.5) / size
    edge = numpy.minimum(ticks, 1.0 - ticks) * 2.0          # 0 at an edge, 1 in the middle
    weight = numpy.minimum.outer(edge, edge)
    weight = smooth(0.0, 0.35, weight)[..., None]
    _seamless[name] = texture * weight + shifted * (1.0 - weight)
    return _seamless[name]


def cut(colour, inside, pixel):
    """A fill's colours cut to a shape, its rim glowing white-gold along the edge"""
    shown = smooth(0.0, 1.5 * pixel, inside)
    rim = numpy.clip(numpy.exp(-inside / RIM_LINE) + 0.35 * numpy.exp(-inside / RIM_GLOW), 0.0, 1.0) * shown
    colour = colour * (1.0 - rim[..., None]) + RIM_COLOUR * rim[..., None]
    alpha = numpy.maximum(shown * FILL_ALPHA, rim)
    return numpy.concatenate([numpy.clip(colour, 0.0, 1.0), alpha[..., None]], axis=2)


# --- Edges of light: never a ruled cut -----------------------------------------------------------------------------------
# A cone or a lance is light thrown out: its sides ragged and bleeding (flames of light licking past the edge, the fill
# eaten into unevenly), not cut along a straight line. BLEED: how far, in yards of a one-yard model, the edge wanders
# and the light licks past it.
BLEED = 0.035
_noise_cache = {}


def value_noise(u, v, cells, seed):
    """Smooth noise in [0, 1] at (u, v), cells across a unit, periodic on every unit (seamless when tiled)"""
    key = (cells, seed)
    if key not in _noise_cache:
        _noise_cache[key] = numpy.random.default_rng(seed).random((cells, cells))
    grid = _noise_cache[key]
    fu = numpy.mod(u, 1.0) * cells
    fv = numpy.mod(v, 1.0) * cells
    i0 = numpy.floor(fu).astype(int) % cells
    j0 = numpy.floor(fv).astype(int) % cells
    i1 = (i0 + 1) % cells
    j1 = (j0 + 1) % cells
    tu = smooth(0.0, 1.0, fu - numpy.floor(fu))
    tv = smooth(0.0, 1.0, fv - numpy.floor(fv))
    top = grid[j0, i0] * (1 - tu) + grid[j0, i1] * tu
    bottom = grid[j1, i0] * (1 - tu) + grid[j1, i1] * tu
    return top * (1 - tv) + bottom * tv


def fractal(u, v, seed=7):
    return (value_noise(u, v, 8, seed) * 0.55 + value_noise(u, v, 23, seed + 1) * 0.3 +
            value_noise(u, v, 61, seed + 2) * 0.15)


def bled(colour, inside, pixel, x, y, flame=RIM_COLOUR):
    """A fill cut to a shape whose edge bleeds: the edge wandering by noise, the fill fading in over it, and past it
    flames of light (streaks of flame colour) licking out where the picture has room"""
    edge = inside + BLEED * (fractal(x * 0.5 + 0.5, y * 0.5 + 0.5) - 0.5)
    shown = smooth(0.0, BLEED * 0.8, edge)
    # Inside, along the edge: a glow, uneven
    rim = numpy.exp(-numpy.maximum(edge, 0.0) / RIM_GLOW) * 0.55 * shown
    # Past it: licks of light, streaked (finer noise), fading out within BLEED * 1.6
    streaks = fractal(x * 1.7 + 0.13, y * 1.7 + 0.41, 11)
    lick = numpy.where(edge < 0.0, numpy.exp(edge / (BLEED * 0.6)) * smooth(0.35, 0.8, streaks), 0.0)
    colour = colour * (1.0 - rim[..., None]) + flame * rim[..., None]
    colour = numpy.where((edge < 0.0)[..., None], flame, colour)
    alpha = numpy.maximum(shown * FILL_ALPHA, numpy.maximum(rim, lick * 0.85))
    return numpy.concatenate([numpy.clip(colour, 0.0, 1.0), alpha[..., None]], axis=2)


def edge_cut(shape, colour, inside, pixel, x, y):
    """A cone's or a line's edges bleed (bled); a circle's or a ring's keep their clean glowing rim (cut)"""
    if shape in ('cone', 'rect'):
        return bled(colour, inside, pixel, x, y)
    return cut(colour, inside, pixel)


def layer(glow, inside, pixel):
    shown = smooth(0.0, 1.5 * pixel, inside)
    return numpy.concatenate([numpy.clip(glow, 0.0, 1.0) * shown[..., None], shown[..., None]], axis=2)


# --- The floor's fills -----------------------------------------------------------------------------------------------

def radiance(shape, value, tiles):
    """Colère du juste's halves, Chaîne brisée's sweep: dark flagstones, gold light in their joints (dim: its glow is
    radiance_glow). tiles: the stones' tiles across the shape's radius"""
    x, y, inside, pixel = grid_of(shape, value)
    stone = tiled(seamless('fill_radiance.png'), x * tiles / 2.0, y * tiles / 2.0)
    return image(edge_cut(shape, stone * 1.05, inside, pixel, x, y))


def radiance_glow(shape, value, tiles):
    x, y, inside, pixel = grid_of(shape, value)
    glow = tiled(seamless('fill_radiance_glow.png'), x * tiles / 2.0, y * tiles / 2.0)
    if shape == 'cone':
        # Over the same ragged edge as the fill (its shown part)
        inside = inside + BLEED * (fractal(x * 0.5 + 0.5, y * 0.5 + 0.5) - 0.5)
    return image(layer(glow * 1.3, inside, pixel))


def rage(shape, value, tiles):
    """Rage contenue's inside and outside: cracked stone about to burst (its fractures' glow: rage_cracks)"""
    x, y, inside, pixel = grid_of(shape, value)
    stone = tiled(seamless('fill_rage.png'), x * tiles / 2.0, y * tiles / 2.0)
    return image(cut(stone * 0.8, inside, pixel))


def rage_cracks(shape, value, tiles):
    x, y, inside, pixel = grid_of(shape, value)
    cracks = tiled(seamless('fill_rage_cracks.png'), x * tiles / 2.0, y * tiles / 2.0)
    return image(layer(cracks * 1.3, inside, pixel))


# --- Painted as their shape --------------------------------------------------------------------------------------------

# Revers sacré's wedge as painted: 90 degrees from the middle of the left edge, its flanks cut by the square's top and
# bottom; kept whole to the square's corners (REVERS_ANGLE, 53.1 degrees), its arc REVERS_ARC from the apex, widened to
# 90 by the builder (shapes.json VL_ReversCone: paintedAngle)
REVERS_APEX = (0, 512)
REVERS_ANGLE = 2.0 * math.degrees(math.atan2(512, 1024))
REVERS_ARC = 1010


def revers_footprint(height=1024, width=1024):
    """The wedge, its arc clean (the builder reads its reach there), its two sides ragged: the light eaten into
    unevenly along them, fading in over BLEED of the wedge's length"""
    rows, columns = numpy.mgrid[0:height, 0:width]
    dx = columns - REVERS_APEX[0] + 0.5
    dy = rows - REVERS_APEX[1] + 0.5
    radius = numpy.hypot(dx, dy)
    half = math.radians(REVERS_ANGLE / 2.0)
    off_axis = numpy.abs(numpy.arctan2(dy, dx))
    to_side = radius * numpy.sin(half - off_axis) / REVERS_ARC        # yards of a one-yard cone, to the nearer side
    u = dx / REVERS_ARC
    v = dy / REVERS_ARC
    to_side = to_side + BLEED * 1.4 * (fractal(u * 0.5 + 0.5, v * 0.5 + 0.5) - 0.5)
    sides = smooth(-BLEED * 0.2, BLEED * 0.9, to_side)
    arc = (radius <= REVERS_ARC).astype(numpy.float64)
    return (sides * arc)[..., None]


def revers_cone():
    """Revers sacré: the backhand's wedge (its flare at the strike: revers_flare)"""
    return image(keyed(load('revers_cone.png')) * revers_footprint())


def revers_flare():
    """The flare alone over the wedge's own footprint (black elsewhere): added, flared as it strikes"""
    footprint = revers_footprint()
    flare = load('revers_cone_flare.png')
    return image(numpy.concatenate([flare * footprint, footprint], axis=2))


# Lances de lumière: a tile of the lance, LANCE_TILE as long as it is wide (2.5 yards: 10 yards long), the painted beam
# (8 to 1) cut to that length and its glow folded in
LANCE_TILE = 4


def lance():
    """A tile of the lance: the painted beam's first LANCE_TILE widths, its ends cross-faded with the next ones so that
    tiles laid end to end join; its two sides fading out unevenly before the tile's edges (a painted beam fills its strip
    to the edge: its sides read as ruled lines)"""
    beam = numpy.maximum(load('lance_strip.png'), load('lance_strip_glow.png') * 0.8)
    height, width = beam.shape[:2]
    length = height * LANCE_TILE
    piece = beam[:, :length].copy()
    other = beam[:, length:2 * length] if 2 * length <= width else beam[:, -length:]
    blend = smooth(0.0, 0.2, numpy.minimum(numpy.arange(length), length - 1 - numpy.arange(length)) / length)
    piece = piece * blend[None, :, None] + other * (1.0 - blend[None, :, None])
    keyed_piece = keyed(piece)
    across = numpy.abs((numpy.arange(height) + 0.5) / height * 2.0 - 1.0)[:, None]     # 0 in the middle, 1 at a side
    along = (numpy.arange(length) + 0.5) / length
    wander = 0.18 * (fractal(along[None, :], across * 0.0 + 0.25, 5) - 0.5) +         0.12 * (fractal(along[None, :] * 2.0, across * 0.0 + 0.75, 9) - 0.5)
    fade = smooth(1.0, 0.62, across + wander)
    keyed_piece[..., 3] *= fade
    return image(keyed_piece)


def keyed_painting(name):
    """A ring, a sigil or a mark painted on black, as it is (keyed)"""
    return image(keyed(load(name)))


def mirrored_painting(name):
    """A painting on black keyed and mirrored left to right: the floor's projection draws a picture mirrored (La
    Litanie's clock, painted clockwise, ran anticlockwise in the world - seen in the FX lab), so a picture whose
    handedness matters is mirrored first"""
    return image(keyed(load(name)[:, ::-1]))


# --- The wall: three layers (the brief's section 5) ----------------------------------------------------------------------
# The bars in front (two of the arena's pieces a band: 2048 x 768 is 12.54 x 4.7 yards, fitted 1024 x 512 and drawn
# back at its aspect); the light behind them rising; the motes in front, faded at the top and the foot

def wall_bars():
    return Image.open(os.path.join(ART, 'wall_bars.png')).convert('RGBA').resize((1024, 512), Image.LANCZOS)


def wall_light():
    """The curtain of light, its seamless square laid across a piece (6.27 x 4.7 yards: 2 tiles wide for 1.5 high),
    faded out towards a ragged top (flames of light of uneven height: a straight top read as a ruled line) and at its
    foot. It drifts along the wall, never up it: a fade drawn in a texture moves with it"""
    tile = Image.open(os.path.join(ART, 'wall_light.png')).convert('RGB').resize((512, 512), Image.LANCZOS)
    band = Image.new('RGB', (1024, 768))
    for column in range(2):
        for row in range(2):
            band.paste(tile, (512 * column, 512 * row))
    light = numpy.asarray(band.resize((1024, 512), Image.LANCZOS), dtype=numpy.float64) / 255.0
    rows, columns = light.shape[:2]
    up = 1.0 - (numpy.arange(rows)[:, None] + 0.5) / rows        # 1 at the top, 0 at the foot
    across = (numpy.arange(columns)[None, :] + 0.5) / columns
    # Each column's own top, seamless round the band (whole sine periods): 0.45 to 0.85 of the height
    top = 0.65 + 0.11 * numpy.sin(2 * math.pi * 3 * across) + 0.07 * numpy.sin(2 * math.pi * 7 * across + 1.3) +         0.04 * numpy.sin(2 * math.pi * 13 * across + 0.4)
    fade = smooth(top, top - 0.35, up) * smooth(0.0, 0.1, up)
    faded = numpy.clip(light * fade[..., None], 0.0, 1.0)
    return Image.fromarray((faded * 255.0 + 0.5).astype(numpy.uint8))


def wall_motes():
    motes = numpy.asarray(Image.open(os.path.join(ART, 'wall_motes.png')).convert('RGB'), dtype=numpy.float64) / 255.0
    rows = motes.shape[0]
    up = 1.0 - (numpy.arange(rows) + 0.5) / rows
    fade = smooth(1.0, 0.6, up) * smooth(0.0, 0.12, up)
    faded = numpy.clip(motes * fade[:, None, None], 0.0, 1.0)
    return Image.fromarray((faded * 255.0 + 0.5).astype(numpy.uint8)).resize((1024, 512), Image.LANCZOS)
