"""Vrogar's painted looks, as the ground indicator builder draws them (localTools/groundIndicators, shapes.json
`picture`: "localTools/escapeHunter/vrogarArt.py:<function>"; ForgeMaster.cpp shows them).

The paintings are art/vrogar/*.png (the brief: .agents/plans/escape-hunter/escape-hunter.ASSETS.md, section 6). The
floor's fills (furnace stone, molten iron, the laminoir's plate) are only a material: each shape is cut out of one
here, its edge glowing, and the layer painted to move with it (the furnace's cracks, the iron's streaks, the anvil's
sparks) made a second look over the same shape, breathing on its own. The other pieces are painted as their shape already: they are keyed (their black see-through) and
handed over as they are, cropped or layered where needed.

Each function returns one picture (RGBA).
"""
import math
import os

import numpy
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.path.join(HERE, 'art', 'vrogar')

# A fill's alpha inside its shape (the floor under it barely seen: the danger reads as the furnace's own floor), its
# rim: a hot line along the edge and a softer glow inward, in yards of a one-yard model
FILL_ALPHA = 0.88
RIM_COLOUR = numpy.array([1.0, 0.58, 0.16])
RIM_LINE = 0.010
RIM_GLOW = 0.045
# A painting on black keyed: alpha full from this brightness up (dark iron stays iron)
KNEE = 0.12


def load(name):
    """art/vrogar/<name> as RGB in [0, 1]"""
    return numpy.asarray(Image.open(os.path.join(ART, name)).convert('RGB'), dtype=numpy.float64) / 255.0


def keyed(rgb, knee=KNEE):
    """A painting on pure black made see-through there: alpha its brightness (full from knee), colour unpremultiplied"""
    alpha = numpy.clip(rgb.max(axis=2, keepdims=True) / knee, 0.0, 1.0)
    colour = numpy.where(alpha > 1e-4, rgb / numpy.maximum(alpha, 1e-4), 0.0)
    return numpy.concatenate([numpy.clip(colour, 0.0, 1.0), alpha], axis=2)


def image(rgba):
    return Image.fromarray(numpy.clip(rgba * 255.0 + 0.5, 0, 255).astype(numpy.uint8), 'RGBA')


def smooth(edge0, edge1, value):
    t = numpy.clip((value - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def tiled(texture, u, v):
    """texture sampled at (u, v), wrapping (one tile a unit)"""
    height, width = texture.shape[:2]
    columns = (numpy.mod(u, 1.0) * width).astype(int) % width
    rows = (numpy.mod(v, 1.0) * height).astype(int) % height
    return texture[rows, columns]


# --- The shapes a fill is cut to: their pixels in the shape's own yards, and how far each is inside -------------------

def circle_grid(size=1024):
    """A circle of radius 1 filling the square (x forward, y across)"""
    ticks = (numpy.arange(size) + 0.5) / size * 2.0 - 1.0
    x, y = numpy.meshgrid(ticks, -ticks)
    return x, y, 1.0 - numpy.hypot(x, y), 1.0 / size


def ring_grid(inner, size=1024):
    x, y, outside, pixel = circle_grid(size)
    radius = numpy.hypot(x, y)
    return x, y, numpy.minimum(1.0 - radius, radius - inner), pixel


def cone_back(angle):
    """How far behind its apex a cone reaches (a wider one than a half: its arc wraps round behind it)"""
    half = math.radians(angle) / 2.0
    return max(0.0, -math.cos(half))


def cone_grid(angle, width=1024):
    """A cone of radius 1, its apex at the middle of the picture's left edge (as the builder finds it), opening to the
    right: as tall as its arc needs. A wider one than a half reaches behind its apex: the picture starts cone_back
    behind it (shapes.json `apexX` says where the apex is then)"""
    half = math.radians(angle) / 2.0
    side = math.sin(half) if half < math.pi / 2 else 1.0
    back = cone_back(angle)
    height = int(round(width * 2.0 * side / (1.0 + back) / 2.0)) * 2
    xs = (numpy.arange(width) + 0.5) / width * (1.0 + back) - back
    ys = ((numpy.arange(height) + 0.5) / height * 2.0 - 1.0) * side
    x, y = numpy.meshgrid(xs, -ys)
    radius = numpy.hypot(x, y)
    off_axis = numpy.abs(numpy.arctan2(y, x))
    # How far inside: to the arc, and to the nearer straight side (past a quarter turn off its edge: its distance to
    # the apex)
    to_side = numpy.where(half - off_axis < math.pi / 2, radius * numpy.sin(half - off_axis), radius)
    return x, y, numpy.minimum(1.0 - radius, to_side), (1.0 + back) / width


def rect_grid(length_share, width=1024):
    """A strip's tile: x along it (its whole length the picture's width), y across (the picture's height); inside is
    measured to its two sides only (tiles laid end to end show no seam)"""
    columns = int(round(width * length_share))
    xs = (numpy.arange(columns) + 0.5) / width
    ys = (numpy.arange(width) + 0.5) / width * 2.0 - 1.0
    x, y = numpy.meshgrid(xs, -ys)
    return x, y, (1.0 - numpy.abs(y)) / 2.0, 1.0 / width


def cut(colour, inside, pixel):
    """A fill's colours cut to a shape (inside: how far each pixel is in it, in yards of a one-yard model), its rim
    glowing along the edge"""
    shown = smooth(0.0, 1.5 * pixel, inside)
    rim = numpy.clip(numpy.exp(-inside / RIM_LINE) + 0.35 * numpy.exp(-inside / RIM_GLOW), 0.0, 1.0) * shown
    colour = colour * (1.0 - rim[..., None]) + RIM_COLOUR * rim[..., None]
    alpha = numpy.maximum(shown * FILL_ALPHA, rim)
    return numpy.concatenate([numpy.clip(colour, 0.0, 1.0), alpha[..., None]], axis=2)


def grid_of(shape, value):
    if shape == 'circle':
        return circle_grid()
    if shape == 'ring':
        return ring_grid(value)
    if shape == 'cone':
        return cone_grid(value)
    if shape == 'rect':
        return rect_grid(value)
    raise ValueError(shape)


# --- The floor's fills -----------------------------------------------------------------------------------------------
#
# Each in two looks shown together: the material cut to the shape, and the layer that moves (its glow alone, added to
# what is under it, breathing on the model: shapes.json pulse / pulseRamp, `blend` add). A layer covers the whole shape
# at full alpha, black but for its glow (black adds nothing): the builder finds its apex, its reach and its band as the
# material's, so the two lie exactly over each other. (Frames of one texture, stepped through, cannot be: a texture
# projected on the floor paints on past its quad, and showed the other frames all round the arena.)

def layer(glow, inside, pixel):
    """A moving layer: glow (RGB) over the shape at full alpha"""
    shown = smooth(0.0, 1.5 * pixel, inside)
    return numpy.concatenate([numpy.clip(glow, 0.0, 1.0) * shown[..., None], shown[..., None]], axis=2)


def furnace(shape, value, tiles):
    """La Fonte's slices, the shockwave, the check missed: cracked furnace stone, the lava in its cracks dim (their
    glow is furnace_cracks). tiles: the stone's tiles across the shape's radius"""
    x, y, inside, pixel = grid_of(shape, value)
    stone = tiled(load('fill_furnace.png'), x * tiles / 2.0, y * tiles / 2.0)
    cracks = tiled(load('fill_furnace_cracks.png'), x * tiles / 2.0, y * tiles / 2.0)
    return image(cut(stone + cracks * 0.3, inside, pixel))


def furnace_cracks(shape, value, tiles):
    x, y, inside, pixel = grid_of(shape, value)
    cracks = tiled(load('fill_furnace_cracks.png'), x * tiles / 2.0, y * tiles / 2.0)
    return image(layer(cracks * 1.2, inside, pixel))


def molten(shape, value, tiles):
    """The pour's halves, the flood: molten iron (its bright streaks: molten_flow)"""
    x, y, inside, pixel = grid_of(shape, value)
    return image(cut(tiled(load('fill_molten.png'), x * tiles / 2.0, y * tiles / 2.0) * 0.85, inside, pixel))


def molten_flow(shape, value, tiles):
    x, y, inside, pixel = grid_of(shape, value)
    return image(layer(tiled(load('fill_molten_flow.png'), x * tiles / 2.0, y * tiles / 2.0) * 0.8, inside, pixel))


def laminoir(length_share):
    """A tile of the laminoir's strip (8 yards across, length_share of that long): the red-hot plate, its grooves
    across it; its two sides glowing"""
    x, y, inside, pixel = rect_grid(length_share)
    return image(cut(tiled(load('fill_laminoir.png'), x, (y + 1.0) / 2.0), inside, pixel))


# --- Painted as their shape --------------------------------------------------------------------------------------------

# The anvil's wedge as painted: about 62 degrees (a square holds no wider one from the middle of its left edge), its
# arc ANVIL_ARC of the width from its apex; its corners past the arc trimmed (the builder takes the farthest texel for
# the cone's radius). shapes.json VR_AnvilCone: paintedAngle
ANVIL_APEX = (10, 512)
ANVIL_ARC = 1010


def anvil_wedge():
    """The painted wedge keyed, its corners past the arc trimmed"""
    base = keyed(load('anvil_cone.png'))
    height, width = base.shape[:2]
    rows, columns = numpy.mgrid[0:height, 0:width]
    base[numpy.hypot(columns - ANVIL_APEX[0], rows - ANVIL_APEX[1]) > ANVIL_ARC] = 0.0
    return base


def anvil_cone():
    """Coup d'enclume: the wedge (its sparks: anvil_sparks)"""
    return image(anvil_wedge())


def anvil_sparks():
    """The sparks fanning out over the wedge, alone (black elsewhere, over the wedge's own footprint)"""
    footprint = anvil_wedge()[..., 3:4]
    sparks = load('anvil_cone_sparks.png')
    return image(numpy.concatenate([sparks * (footprint > 0.15), footprint], axis=2))


def bellows_blast():
    return image(keyed(load('bellows_blast.png')))


def chain_strip():
    return Image.open(os.path.join(ART, 'chain_strip.png')).convert('RGB')


def steam():
    """La Trempe's steam: the burst and its wisps as one (the model turns them slowly)"""
    burst = load('steam_burst.png')
    wisps = load('steam_wisps.png')
    return image(keyed(numpy.maximum(burst, wisps * 0.9)))


def sparks():
    """Gerbe d'étincelles: the scorched circle and its embers"""
    return image(keyed(numpy.maximum(load('sparks_circle.png'), load('sparks_embers.png'))))


def keyed_painting(name):
    """A ring or a sigil painted to its circle on black, as it is (keyed)"""
    return image(keyed(load(name)))


# The ingots' floor sigils: the tool's disc painted in the middle of its canvas (its circle 195 to 828): cropped to it,
# so it fills the circle it is drawn on (ForgeMaster: inside the spinning ring, IngotSigilShare of it)
SIGIL_CROP = (190, 834)


def sigil(tool):
    first, last = SIGIL_CROP
    return image(keyed(load(f'sigil_{tool}.png')[first:last, first:last]))


def soak_atlas():
    """La Fonte's soak over the healers: its four frames side by side (a billboard flipbook, grid 4 x 1)"""
    atlas = Image.new('RGBA', (1024, 256))
    for index in range(4):
        atlas.alpha_composite(Image.open(os.path.join(ART, f'soak_mark_{index + 1}.png')).convert('RGBA'),
                              (256 * index, 0))
    return atlas


def soak_timeline():
    """The drop swelling and back, looped (150 ms a frame)"""
    return [(time * 150, frame) for time, frame in enumerate([0, 1, 2, 3, 2, 1])]


# The wall's band, from one bar's middle to another's three bars on (the paintings' edges are smeared: the band ran
# on round the arena with a smear at each join)
WALL_CROP = (104, 919)


def wall_forge(frame):
    """The arena's wall: molten metal between black iron bars, four frames it flickers through, cut bar to bar"""
    painting = Image.open(os.path.join(ART, f'wall_forge_{frame}.png')).convert('RGB')
    first, last = WALL_CROP
    return painting.crop((first, 0, last, painting.height)).resize(painting.size, Image.LANCZOS)
