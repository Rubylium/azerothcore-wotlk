"""Builds the ground indicator models: the red areas drawn under enemy abilities to stay out of.

    python buildGroundIndicators.py

For every shape of shapes.json this writes, under modules/mod-stat-growth/client-assets/compiled/indicators
(shipped in patch-Z as Spells\\Evolutions\\...):

- GI_<key>.m2 and GI_<key>00.skin: one flat quad carrying the shape's texture
- GI_<key>.blp: the shape, filled with one even, low-alpha red, with a bright red outline over its edge

How the client draws them (found by testing in game, see .agents/plans/ground-indicators):
- The quad is never drawn itself. Its skin batch carries the projected-texture flag (0x04), so the client paints its
  texture on the terrain and the buildings under it, following slopes and stairs: the "Projected Textures" video
  option (CVar projectedTextures, which the interface turns on). Everything from 2 yards below the quad to 2 yards
  above it is painted, a constant of the client.
- The texture follows the quad's own texture coordinates, so a rectangle or a cone turns with its owner.
- The owner's scale scales the painted area. Every model is one yard (a circle of radius 1, a rectangle 1 long, a
  cone of radius 1) and the server gives its owner the scale of the ability's size.
- The models are the stock Spells\\HolyZone.m2 (a projected zone of the game) with its quad, texture and colours
  replaced. A model written from nothing froze the client, so the rest of the stock file is kept as it is.
- A texture's outermost texels must be fully transparent: the client clamps to them past the quad, and anything
  left there streaks outward over the whole painted area. Each texture keeps a clear border around the shape.

A shape of kind "texture" is a painted ability (The Hollow Voice's, localTools/hollowVoice/textures) drawn in place of
the red: its picture warped onto the very area the ability hits (a line stretched to its ratio, a cone turned to its
arc, a ring's band moved to its inner proportion, a circle's edge onto the area's), its colours kept as painted
(uncompressed, mipmaps resized on premultiplied alpha so no dark fringe creeps in), and optionally animated in the
model: `spin` (ms a turn, the texture turning on the ground) and `pulse` ([low, high, ms]: its alpha breathing, for a
warning). Both run on global sequences, so they go on whatever the model's own animation does.

A shape of kind "image" is a picture painted on the ground the same way (a boss's sigil: The Hollow Voice's, from
localTools/hollowVoice/sigils): a circle of radius 1 carrying the picture, its image path relative to the repository,
softened (brightness, alpha: a projected texture on bright ground reads as a flash otherwise) and cut round, its edge
faded, so the border stays clear.
"""
import importlib.util
import json
import math

import numpy
import os
import shutil
import struct
import subprocess
import sys
import tempfile

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
OUTPUT_ROOT = os.path.join(REPO_ROOT, 'modules', 'mod-stat-growth', 'client-assets', 'compiled', 'indicators')
ARCHIVE_ROOT = 'Spells\\Evolutions'
EXTRACTOR = os.path.join(REPO_ROOT, 'localTools', 'mpq-builder', 'extractClientFiles.js')
TEMPLATE = 'Spells\\HolyZone'

# The quad sits where the template's does, just above its owner's feet
QUAD_HEIGHT = 0.028
# Clear border around the shape, in yards of a one-yard model
PADDING = 0.06
# The outline drawn over the fill: brighter and nearly opaque, so the edge reads at a glance. Its thickness, in
# yards of a one-yard model: a share of the shape's narrowest side (a thin line would be all outline otherwise),
# up to a most
OUTLINE_COLOR = (255, 70, 40)
OUTLINE_ALPHA = 0.9
OUTLINE_SHARE = 0.15
OUTLINE_MAX = 0.045
# Edges are smoothed over this many pixels
ANTIALIAS_PIXELS = 1.2
# The longest side of a texture, in pixels; the other follows the shape's proportions. Big enough that a thin
# line's outline is still a few pixels wide.
TEXTURE_SIZE = 256
MIN_TEXTURE_SIDE = 64
# A picture's texture: detailed enough to read on a 10-yard circle
IMAGE_TEXTURE_SIZE = 512
# Its white core (a sigil's glow) is taken down to this share of its alpha: blown-out white over the ground is a flash
IMAGE_WHITE_ALPHA = 0.55

spec = importlib.util.spec_from_file_location(
    'buildParagonArt', os.path.join(REPO_ROOT, 'localTools', 'interface', 'buildParagonArt.py'))
blp_writer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(blp_writer)


# --- Shapes: the signed distance from a point to the outline (negative inside), in model yards ----------------

def shape_bounds(shape):
    """(x0, x1, y0, y1) of the shape: x forward from its owner, y to its left"""
    kind = shape['kind']
    if kind == 'texture':
        return shape_bounds(dict(shape, kind=shape['shape']))
    if kind in ('circle', 'ring', 'star', 'image'):
        return -1.0, 1.0, -1.0, 1.0
    if kind == 'rect':
        half = 0.5 / shape['ratio']
        if shape.get('segment'):
            return -0.5, 0.5, -half, half
        return 0.0, 1.0, -half, half
    if kind == 'cone':
        half = math.radians(shape['angle']) / 2
        side = math.sin(half) if half < math.pi / 2 else 1.0
        back = min(0.0, math.cos(half))
        return back, 1.0, -side, side
    raise ValueError(f"unknown shape kind {kind}")


def shape_distance(shape, x, y):
    kind = shape['kind']
    if kind == 'circle':
        return math.hypot(x, y) - 1.0
    if kind == 'ring':
        # The band between the hole and the outer edge: its outline runs along both
        radius = math.hypot(x, y)
        return max(radius - 1.0, shape['inner'] - radius)
    if kind == 'rect':
        half = 0.5 / shape['ratio']
        dx = max(-x, x - 1.0)
        dy = abs(y) - half
        outside = math.hypot(max(dx, 0.0), max(dy, 0.0))
        return outside + min(max(dx, dy), 0.0)
    if kind == 'star':
        # Two bars through the middle, one along x and one along y: inside either
        half = 0.5 / shape['ratio']

        def bar(along, across):
            dx = abs(along) - 1.0
            dy = abs(across) - half
            return math.hypot(max(dx, 0.0), max(dy, 0.0)) + min(max(dx, dy), 0.0)
        return min(bar(x, y), bar(y, x))
    if kind == 'cone':
        half = math.radians(shape['angle']) / 2
        radius = math.hypot(x, y)
        arc = radius - 1.0
        # Distance to the nearer straight side of the sector
        angle = abs(math.atan2(y, x))
        side = radius * math.sin(angle - half) if angle - half < math.pi / 2 else radius
        return max(arc, side)
    raise ValueError(f"unknown shape kind {kind}")


def outline_thickness(shape):
    if shape['kind'] == 'ring':
        return min(OUTLINE_MAX, OUTLINE_SHARE * (1.0 - shape['inner']))
    if shape['kind'] == 'star':
        return min(OUTLINE_MAX, OUTLINE_SHARE / shape['ratio'])
    x0, x1, y0, y1 = shape_bounds(shape)
    return min(OUTLINE_MAX, OUTLINE_SHARE * min(x1 - x0, y1 - y0))


def padded_bounds(shape):
    x0, x1, y0, y1 = shape_bounds(shape)
    return x0 - PADDING, x1 + PADDING, y0 - PADDING, y1 + PADDING


def texture_size(shape):
    x0, x1, y0, y1 = padded_bounds(shape)
    length, width = x1 - x0, y1 - y0
    scale = TEXTURE_SIZE / max(length, width)

    def side(extent):
        pixels = max(MIN_TEXTURE_SIDE, int(round(extent * scale)))
        return 1 << (pixels - 1).bit_length()
    return side(length), side(width)


def build_texture(shape, color, alpha):
    """u (the image's columns) runs forward along x, v (its rows) across along y

    The shape filled with color at alpha, and its outline over it."""
    x0, x1, y0, y1 = padded_bounds(shape)
    columns, rows = texture_size(shape)
    pixel = max((x1 - x0) / columns, (y1 - y0) / rows)
    smooth = ANTIALIAS_PIXELS * pixel
    thickness = max(outline_thickness(shape), 2.5 * pixel)
    image = Image.new('RGBA', (columns, rows))
    pixels = image.load()
    for row in range(rows):
        y = y0 + (row + 0.5) / rows * (y1 - y0)
        for column in range(columns):
            x = x0 + (column + 0.5) / columns * (x1 - x0)
            inside = -shape_distance(shape, x, y)
            fill = alpha * min(1.0, max(0.0, inside / smooth))
            # The band from the edge inward, smoothed on both sides
            band = min(1.0, max(0.0, inside / smooth)) * min(1.0, max(0.0, (thickness - inside) / smooth))
            line = OUTLINE_ALPHA * band
            # The outline laid over the fill
            total = line + fill * (1.0 - line)
            if total <= 0.0:
                pixels[column, row] = tuple(color) + (0,)
                continue
            mixed = tuple(int(round((o * line + c * fill * (1.0 - line)) / total))
                          for o, c in zip(OUTLINE_COLOR, color))
            pixels[column, row] = mixed + (int(round(255 * total)),)
    # The border is clear by construction; make sure of it, as a streak over the whole area is what it prevents
    for column in range(columns):
        for row in (0, rows - 1):
            pixels[column, row] = tuple(color) + (0,)
    for row in range(rows):
        for column in (0, columns - 1):
            pixels[column, row] = tuple(color) + (0,)
    return image


def build_image_texture(shape):
    """The shape's picture fitted into the padded circle, softened and cut round (see the module's comment)"""
    x0, x1, _, _ = padded_bounds(shape)
    size = IMAGE_TEXTURE_SIZE
    source = Image.open(os.path.join(REPO_ROOT, *shape['image'].split('/'))).convert('RGBA')
    # The picture spans the circle (radius 1) inside the padded square
    inner = int(round(size * 2.0 / (x1 - x0)))
    picture = source.resize((inner, inner), Image.LANCZOS)
    image = Image.new('RGBA', (size, size))
    offset = (size - inner) // 2
    image.alpha_composite(picture, (offset, offset))
    brightness = shape.get('brightness', 0.85)
    alpha_scale = shape.get('alpha', 0.8)
    pixels = image.load()
    center = (size - 1) / 2.0
    edge = inner / 2.0
    fade = size * 0.02
    for row in range(size):
        for column in range(size):
            r, g, b, a = pixels[column, row]
            radius = math.hypot(column - center, row - center)
            cut = min(1.0, max(0.0, (edge - radius) / fade))
            white = min(r, g, b) / 255.0
            soften = 1.0 - (1.0 - IMAGE_WHITE_ALPHA) * max(0.0, (white - 0.75) / 0.25)
            alpha = a / 255.0 * alpha_scale * cut * soften
            pixels[column, row] = (int(r * brightness), int(g * brightness), int(b * brightness),
                                   int(round(255 * alpha)))
    for column in range(size):
        for row in (0, size - 1):
            pixels[column, row] = (0, 0, 0, 0)
            pixels[row, column] = (0, 0, 0, 0)
    # The texture's u runs forward along x: the picture's top faces the owner's front
    return image.transpose(Image.ROTATE_270)


# A painted ability's texture: the longest side, and the pixels past the area's edge the painting may glow into
PAINTED_TEXTURE_SIZE = 512
PAINTED_LINE_LENGTH = 1024
# A line's piece (`segment`): its texture's columns along it, rows across
PAINTED_SEGMENT_SIZE = (512, 256)


def premultiplied(image):
    array_ = numpy.asarray(image.convert('RGBA'), dtype=numpy.float64) / 255.0
    array_[..., :3] *= array_[..., 3:4]
    return array_


def unpremultiplied(array_):
    out = array_.copy()
    alpha = out[..., 3:4]
    out[..., :3] = numpy.where(alpha > 1e-6, out[..., :3] / numpy.maximum(alpha, 1e-6), 0.0)
    return Image.fromarray(numpy.clip(out * 255.0 + 0.5, 0, 255).astype(numpy.uint8), 'RGBA')


def sample(source, xs, ys):
    """Bilinear samples of a premultiplied array at pixel coordinates (transparent outside)"""
    height, width = source.shape[:2]
    padded = numpy.zeros((height + 2, width + 2, 4))
    padded[1:-1, 1:-1] = source
    xs = numpy.clip(xs + 1.0, 0.0, width + 1.0 - 1e-6)
    ys = numpy.clip(ys + 1.0, 0.0, height + 1.0 - 1e-6)
    x0 = numpy.floor(xs).astype(int)
    y0 = numpy.floor(ys).astype(int)
    x1 = numpy.minimum(x0 + 1, width + 1)
    y1 = numpy.minimum(y0 + 1, height + 1)
    fx = (xs - x0)[..., None]
    fy = (ys - y0)[..., None]
    top = padded[y0, x0] * (1 - fx) + padded[y0, x1] * fx
    bottom = padded[y1, x0] * (1 - fx) + padded[y1, x1] * fx
    return top * (1 - fy) + bottom * fy


def measure(source):
    """The painting's reach: (centre-relative outer radius, inner radius) for a round one, in its own pixels"""
    alpha = source[..., 3]
    height, width = alpha.shape
    yy, xx = numpy.mgrid[0:height, 0:width]
    radius = numpy.hypot(xx - width / 2.0, yy - height / 2.0) / (width / 2.0)
    profile = [alpha[(radius >= k / 100) & (radius < (k + 1) / 100)].mean() for k in range(100)]
    solid = [k for k in range(100) if profile[k] > 0.3]
    return solid[-1] / 100.0, solid[0] / 100.0


def build_painted_texture(shape):
    """The shape's painting warped onto its area, in the padded quad's frame (u forward along x, v across y)"""
    source_image = Image.open(os.path.join(REPO_ROOT, *shape['image'].split('/'))).convert('RGBA')
    if shape.get('blackKey'):
        # Painted on black: its brightness is its alpha, and on black its colour is already premultiplied by it
        # (an additive glow's own colour comes back exactly); `gain` brightens a dim one
        rgb = numpy.asarray(source_image.convert('RGB'), dtype=numpy.float64) / 255.0
        rgb = numpy.clip(rgb * shape.get('gain', 1.0), 0.0, 1.0)
        source = numpy.concatenate([rgb, rgb.max(axis=2, keepdims=True)], axis=2)
    else:
        source = premultiplied(source_image)
    height, width = source.shape[:2]
    kind = shape['shape']
    x0, x1, y0, y1 = padded_bounds(shape)
    if kind == 'rect' and shape.get('segment'):
        columns, rows = PAINTED_SEGMENT_SIZE
    elif kind == 'rect':
        columns = PAINTED_LINE_LENGTH
        rows = max(MIN_TEXTURE_SIDE, 1 << (int(round(columns * (y1 - y0) / (x1 - x0))) - 1).bit_length())
    else:
        columns = rows = PAINTED_TEXTURE_SIZE
    xs = x0 + (numpy.arange(columns) + 0.5) / columns * (x1 - x0)
    ys = y0 + (numpy.arange(rows) + 0.5) / rows * (y1 - y0)
    X, Y = numpy.meshgrid(xs, ys)
    fade = None
    if kind == 'rect':
        half = 0.5 / shape['ratio']
        # `band`: the rows the line's width is (a painting glowing in the middle of its height, its glow past the area
        # then); "auto", where its rows are brighter than a tenth of the brightest
        top, bottom = 0.0, float(height)
        band = shape.get('band')
        if band == 'auto':
            rows_mean = source[..., 3].mean(axis=1)
            lit = numpy.nonzero(rows_mean > 0.1 * rows_mean.max())[0]
            top, bottom = float(lit.min()), float(lit.max() + 1)
        elif band:
            top, bottom = band[0] * height, band[1] * height
        sy = top + (Y + half) / (2.0 * half) * (bottom - top)
        if shape.get('segment'):
            # One of `count` pieces of a line drawn in a chain (GroundIndicators::ShowPaintedLine), each on its own
            # carrier at its middle: a long line on one carrier at its end vanished whenever that end was out of sight
            # (past a wall). Piece `index` paints that share of the tile, so the chain runs on seamless: each piece
            # ends exactly where the next begins, its padding along the line clear (cross-faded over it instead, two
            # pieces blended over each other left a darker band at every join).
            index, count = shape['segment']
            fraction = numpy.mod((X + 0.5 + index) / count, 1.0)
            sx = numpy.clip(fraction * width, 0.0, width - 1.0)
            fade = ((X >= -0.5) & (X < 0.5)).astype(numpy.float64)
        elif shape.get('tile'):
            # The painting repeated along the line at its own proportions (a copy as long as it is wide times its
            # aspect), every other copy mirrored so the joins match, its tapered tips cut off (`crop`, a share of its
            # width); the line's own two ends fade. Stretched to the line's ratio instead, it read drawn out.
            # A seamless painting (`mirror` false) is repeated as it is.
            first, last = shape.get('crop', [0.0, 1.0])
            tile_width = (last - first) * width
            tile_length = tile_width / (bottom - top) * (2.0 * half)
            u = X / tile_length
            index = numpy.floor(u)
            fraction = u - index
            if shape.get('mirror', True):
                fraction = numpy.where(numpy.mod(index, 2) == 1, 1.0 - fraction, fraction)
            sx = first * width + fraction * tile_width
            # Never past its last column: the sampler fades to clear there, a dark seam at each join
            sx = numpy.clip(sx, 0.0, width - 1.0)
            ends = shape.get('endFade', 0.04)
            fade = numpy.clip(X / ends, 0.0, 1.0) * numpy.clip((1.0 - X) / ends, 0.0, 1.0)
        else:
            # The painting fills the line: its width across its length
            sx = X * width
    elif kind == 'cone':
        # Apex at the middle of the painting's left edge; its arc turned onto the cone's
        alpha = source[..., 3]
        rows_, cols_ = numpy.nonzero(alpha > 0.15)
        apex_x = cols_.min()
        reach = numpy.hypot(cols_ - apex_x, rows_ - height / 2.0).max()
        radius = numpy.hypot(X, Y)
        angle = numpy.arctan2(Y, X)
        scale = shape['paintedAngle'] / shape['angle']
        sx = apex_x + radius * reach * numpy.cos(angle * scale)
        sy = height / 2.0 + radius * reach * numpy.sin(angle * scale)
    else:
        outer, inner = measure(source)
        radius = numpy.hypot(X, Y)
        angle = numpy.arctan2(Y, X)
        if kind == 'ring':
            # The painted band [inner, outer] moved onto the area's [ring inner, 1]
            # (inside: the painting's own hole, scaled; past the edge: its glow, at the painting's own scale)
            target = shape['inner']
            band = (radius - target) / (1.0 - target)
            source_radius = numpy.where(radius < target, radius / target * inner,
                                        numpy.where(radius <= 1.0, inner + band * (outer - inner),
                                                    outer + (radius - 1.0) * outer))
        else:
            source_radius = radius * outer
        sx = width / 2.0 + source_radius * (width / 2.0) * numpy.cos(angle)
        sy = height / 2.0 + source_radius * (height / 2.0) * numpy.sin(angle)
    result = sample(source, sx, sy)
    if fade is not None:
        result *= fade[..., None]
    result[..., 3] *= shape.get('alpha', 1.0)
    result[..., :3] *= shape.get('alpha', 1.0)
    # The outermost texels clear, whatever the painting does there (the client clamps to them past the quad)
    result[0, :] = result[-1, :] = 0.0
    result[:, 0] = result[:, -1] = 0.0
    return result


def premultiplied_mips(array_):
    """Mipmaps averaged on premultiplied colour: a soft edge keeps its colour instead of darkening"""
    current = array_
    while True:
        yield unpremultiplied(current)
        height, width = current.shape[:2]
        if max(height, width) == 1:
            return
        new_height, new_width = max(1, height // 2), max(1, width // 2)
        trimmed = current[:new_height * (height // new_height), :new_width * (width // new_width)]
        current = trimmed.reshape(new_height, height // new_height, new_width, width // new_width, 4).mean(axis=(1, 3))


def write_painted_blp(array_, path):
    """Uncompressed 32-bit BLP2 (the colours exactly as painted), premultiplied mipmaps"""
    mipmaps = [mip for mip in premultiplied_mips(array_)][:16]
    offsets = [0] * 16
    sizes = [0] * 16
    cursor = 148
    encoded = []
    for index, mip in enumerate(mipmaps):
        red, green, blue, alpha = mip.split()
        data = Image.merge('RGBA', (blue, green, red, alpha)).tobytes()
        offsets[index] = cursor
        sizes[index] = len(data)
        cursor += len(data)
        encoded.append(data)
    height, width = array_.shape[:2]
    header = struct.pack('<4sIBBBBII16I16I', b'BLP2', 1, 3, 8, 8, 1, width, height, *offsets, *sizes)
    with open(path, 'wb') as output:
        output.write(header)
        for data in encoded:
            output.write(data)


# --- Models ------------------------------------------------------------------------------------------------------

def array(data, offset):
    return struct.unpack_from('<II', data, offset)


def track_values(data, track):
    """offset of every value of an M2Track, across its sequences"""
    count, offset = array(data, track + 12)
    for sequence in range(count):
        values, values_offset = array(data, offset + 8 * sequence)
        yield values, values_offset


def append_block(data, payload):
    """Appends payload 16-byte aligned; returns its offset"""
    while len(data) % 16:
        data.append(0)
    offset = len(data)
    data += payload
    return offset


def write_global_track(data, track_offset, global_index, sequence_count, timestamps, values, value_format):
    """Points an M2Track at keys on a global sequence (the same keys for every animation sequence)"""
    times = append_block(data, struct.pack(f'<{len(timestamps)}I', *timestamps))
    keys = append_block(data, b''.join(struct.pack(value_format, *value) for value in values))
    time_arrays = append_block(data, struct.pack('<II', len(timestamps), times) * sequence_count)
    value_arrays = append_block(data, struct.pack('<II', len(values), keys) * sequence_count)
    struct.pack_into('<Hh', data, track_offset, 1, global_index)
    struct.pack_into('<IIII', data, track_offset + 4, sequence_count, time_arrays, sequence_count, value_arrays)


def animate(data, shape):
    """A painted shape's spin (its texture turning) and pulse (its alpha breathing), on global sequences"""
    spin = shape.get('spin', 0)
    pulse = shape.get('pulse')
    if not spin and not pulse:
        return False
    sequence_count, _ = array(data, 0x1C)
    loops = []
    if spin:
        loops.append(int(spin))
    if pulse:
        loops.append(int(pulse[2]))
    struct.pack_into('<II', data, 0x14, len(loops), append_block(data, struct.pack(f'<{len(loops)}I', *loops)))
    if spin:
        # One texture transform: no translation or scaling, a rotation about the texture's middle, a full turn a spin
        transform = append_block(data, bytes(60))
        steps = 8
        turn = [int(spin * k / steps) for k in range(steps + 1)]
        quaternions = [(0.0, 0.0, math.sin(math.pi * k / steps), math.cos(math.pi * k / steps))
                       for k in range(steps + 1)]
        write_global_track(data, transform + 20, 0, sequence_count, turn, quaternions, '<4f')
        struct.pack_into('<hh', data, transform, 0, -1)
        struct.pack_into('<hh', data, transform + 40, 0, -1)
        struct.pack_into('<II', data, 0x60, 1, transform)
        _, lookup = array(data, 0x98)
        struct.pack_into('<h', data, lookup, 0)
    if pulse:
        low, high, period = pulse
        index = 1 if spin else 0
        _, transparency = array(data, 0x58)
        write_global_track(data, transparency, index, sequence_count, [0, period // 2, period],
                           [(int(low * 0x7FFF),), (int(high * 0x7FFF),), (int(low * 0x7FFF),)], '<h')
    return bool(spin)


# A shape's fading twin (GI_<key>Fade): the same look, its alpha falling to nothing FADE_MS after it is put on.
# GroundIndicators.cpp turns a painting's carrier to it FADE_MS before the carrier goes: gone at once, a painting
# popped off the floor. Its spell: fade_spell (GroundIndicators.cpp FadeLookOf, patchSinisterStrike.ps1 keep the rule).
FADE_MS = 300


def fade_spell(spell):
    if 90600 <= spell < 90900:
        return spell + 6000
    if 94000 <= spell < 94200:
        return spell + 200
    raise SystemExit(f'spell {spell}: no fading twin range for it (fade_spell)')


def fade_alpha(data):
    """Every sequence's colour alpha opaque when the model is put on, clear from FADE_MS: the twin starts its
    animation over when its aura is applied"""
    sequence_count, _ = array(data, 0x1C)
    count, colors = array(data, 0x48)
    for index in range(count):
        track = colors + 40 * index + 20
        times = append_block(data, struct.pack('<2I', 0, FADE_MS))
        keys = append_block(data, struct.pack('<2h', 0x7FFF, 0))
        time_arrays = append_block(data, struct.pack('<II', 2, times) * sequence_count)
        value_arrays = append_block(data, struct.pack('<II', 2, keys) * sequence_count)
        struct.pack_into('<Hh', data, track, 1, -1)
        struct.pack_into('<IIII', data, track + 4, sequence_count, time_arrays, sequence_count, value_arrays)


def animated_skin(skin):
    """The skin with its batch's texture no longer static (0x10), so the texture transform plays"""
    data = bytearray(skin)
    count, batches = struct.unpack_from('<II', data, 0x24)
    for index in range(count):
        flags = data[batches + 24 * index]
        data[batches + 24 * index] = flags & ~0x10
        struct.pack_into('<H', data, batches + 24 * index + 22, 0)
    return bytes(data)


def build_model(template, shape, texture_path, fade=False):
    data = bytearray(template)
    # A carried circle is drawn at its own size: its owner (a player) cannot be scaled
    scale = shape.get('scale', 1.0)
    x0, x1, y0, y1 = (value * scale for value in padded_bounds(shape))

    # The quad: its four corners, bottom-left, bottom-right, top-left, top-right, as the template lists them
    count, vertices = array(data, 0x3C)
    if count != 4:
        raise SystemExit(f'{TEMPLATE}.m2 changed: {count} vertices, expected its one quad')
    corners = [(x0, y0, 0.0, 0.0), (x1, y0, 1.0, 0.0), (x0, y1, 0.0, 1.0), (x1, y1, 1.0, 1.0)]
    for index, (x, y, u, v) in enumerate(corners):
        struct.pack_into('<3f', data, vertices + 48 * index, x, y, QUAD_HEIGHT)
        struct.pack_into('<2f', data, vertices + 48 * index + 32, u, v)

    # The texture the batch draws (the first texture lookup) is pointed at the shape's, appended at the end
    _, lookup = array(data, 0x80)
    texture_index = struct.unpack_from('<h', data, lookup)[0]
    _, textures = array(data, 0x50)
    name = texture_path.encode('ascii') + b'\0'
    while len(data) % 16:
        data.append(0)
    name_offset = len(data)
    data += name
    struct.pack_into('<II', data, textures + 16 * texture_index + 8, len(name), name_offset)

    # Alpha-blended rather than additive: added red washes out to pink on bright ground
    _, materials = array(data, 0x70)
    flags, _ = struct.unpack_from('<HH', data, materials)
    struct.pack_into('<HH', data, materials, flags, 2)

    # The template's white-gold tint and fades: white and opaque at every key, the texture carries the colour
    colors_count, colors = array(data, 0x48)
    for index in range(colors_count):
        for values, offset in track_values(data, colors + 40 * index):
            for key in range(values):
                struct.pack_into('<3f', data, offset + 12 * key, 1.0, 1.0, 1.0)
        for values, offset in track_values(data, colors + 40 * index + 20):
            for key in range(values):
                struct.pack_into('<h', data, offset + 2 * key, 0x7FFF)
    transparency_count, transparency = array(data, 0x58)
    for index in range(transparency_count):
        for values, offset in track_values(data, transparency + 20 * index):
            for key in range(values):
                struct.pack_into('<h', data, offset + 2 * key, 0x7FFF)

    # No sparkles: the template's particles are its holy glitter
    struct.pack_into('<II', data, 0x128, 0, 0)

    animate(data, shape)
    if fade:
        fade_alpha(data)

    # Bounds of the quad, for culling
    radius = max(math.hypot(x, y) for x in (x0, x1) for y in (y0, y1))
    struct.pack_into('<6ff', data, 0xA0, x0, y0, 0.0, x1, y1, QUAD_HEIGHT, radius)
    sequences_count, sequences = array(data, 0x1C)
    for index in range(sequences_count):
        struct.pack_into('<6ff', data, sequences + 64 * index + 32, x0, y0, 0.0, x1, y1, QUAD_HEIGHT, radius)
    return bytes(data)


# A shape of kind "curtain" is a line's light standing up from the floor (3D, unlike the projected paintings): two
# planes crossed along the line (an X across it, each meeting the floor `spread` yards off the line's middle and leaning
# across it, so it covers the line's width and reads from above as from the side), `height` yards tall, `length` yards long (built to size: its carrier is never scaled; a
# piece of a line drawn in pieces is centred on its carrier, a whole one starts at it). Its texture is its painting's
# whole tile, bright at the floor and fading up (the painted band's middle at the floor, its edge at the top), added to
# what is behind it, flowing along the line every `scroll` ms. A piece (`segment` [index, count]) shows its share of
# the tile, so the pieces of a line, flowing on the same clock, run on seamless; `tiles` repeats the tile along a
# whole one.
CURTAIN_TEXTURE = (512, 128)


def build_curtain_texture(shape):
    source_image = Image.open(os.path.join(REPO_ROOT, *shape['image'].split('/'))).convert('RGB')
    rgb = numpy.clip(numpy.asarray(source_image, dtype=numpy.float64) / 255.0 * shape.get('gain', 1.0), 0.0, 1.0)
    source = numpy.concatenate([rgb, rgb.max(axis=2, keepdims=True)], axis=2)
    height, width = source.shape[:2]
    rows_mean = source[..., 3].mean(axis=1)
    lit = numpy.nonzero(rows_mean > 0.1 * rows_mean.max())[0]
    top, middle = float(lit.min()), (float(lit.min()) + float(lit.max())) / 2.0
    columns, rows = CURTAIN_TEXTURE
    xs = (numpy.arange(columns) + 0.5) / columns * (width - 1.0)
    v = (numpy.arange(rows) + 0.5) / rows             # 0 at the top of the curtain, 1 at the floor
    ys = top + v * (middle - top)
    X, Y = numpy.meshgrid(xs, ys)
    result = sample(source, X, Y)
    # Fading up: full at the floor, nothing at the top
    result *= (v ** 1.5)[:, None, None]
    return result


def build_curtain_model(template, skin, shape, texture_path, fade=False):
    data = bytearray(template)
    length = float(shape['length'])
    height = float(shape['height'])
    # How far off the line's middle each plane meets the floor: `spread` yards (most of the line's width: a narrow X
    # read as a seam down the middle), or `lean` of its height
    lean = shape.get('spread', height * shape.get('lean', 0.35))
    x0, x1 = (-length / 2.0, length / 2.0) if shape.get('segment') else (0.0, length)
    index, count = shape.get('segment', [0, 1])
    u0 = index / count
    u1 = u0 + shape.get('tiles', 1.0) / count

    # Two planes, an X across the line: from one side at the floor to the other at the top, and back
    corners = []
    for side in (-1.0, 1.0):
        corners += [(x0, side * lean, 0.0, u0, 1.0), (x1, side * lean, 0.0, u1, 1.0),
                    (x0, -side * lean, height, u0, 0.0), (x1, -side * lean, height, u1, 0.0)]
    _, template_vertices = array(data, 0x3C)
    vertex = bytes(data[template_vertices:template_vertices + 48])
    block = bytearray()
    for x, y, z, u, v in corners:
        record = bytearray(vertex)
        struct.pack_into('<3f', record, 0, x, y, z)
        struct.pack_into('<2f', record, 32, u, v)
        block += record
    struct.pack_into('<II', data, 0x3C, len(corners), append_block(data, bytes(block)))

    # The texture, wrapping along the line (it flows), and drawn as the template's: added, both sides, unlit
    _, lookup = array(data, 0x80)
    texture_index = struct.unpack_from('<h', data, lookup)[0]
    _, textures = array(data, 0x50)
    name = texture_path.encode('ascii') + b'\0'
    name_offset = append_block(data, name)
    struct.pack_into('<I', data, textures + 16 * texture_index + 4, 1)
    struct.pack_into('<II', data, textures + 16 * texture_index + 8, len(name), name_offset)

    colors_count, colors = array(data, 0x48)
    for color in range(colors_count):
        for values, offset in track_values(data, colors + 40 * color):
            for key in range(values):
                struct.pack_into('<3f', data, offset + 12 * key, 1.0, 1.0, 1.0)
        for values, offset in track_values(data, colors + 40 * color + 20):
            for key in range(values):
                struct.pack_into('<h', data, offset + 2 * key, 0x7FFF)
    transparency_count, transparency = array(data, 0x58)
    for item in range(transparency_count):
        for values, offset in track_values(data, transparency + 20 * item):
            for key in range(values):
                struct.pack_into('<h', data, offset + 2 * key, 0x7FFF)
    struct.pack_into('<II', data, 0x128, 0, 0)

    # Flowing (a texture transform's translation along u) and breathing (a warning's pulse), on global sequences
    sequence_count, _ = array(data, 0x1C)
    scroll = int(shape.get('scroll', 0))
    pulse = shape.get('pulse')
    loops = ([scroll] if scroll else []) + ([int(pulse[2])] if pulse else [])
    if loops:
        struct.pack_into('<II', data, 0x14, len(loops), append_block(data, struct.pack(f'<{len(loops)}I', *loops)))
    if scroll:
        transform = append_block(data, bytes(60))
        write_global_track(data, transform, 0, sequence_count, [0, scroll], [(0.0, 0.0, 0.0), (-1.0, 0.0, 0.0)],
                           '<3f')
        struct.pack_into('<hh', data, transform + 20, 0, -1)
        struct.pack_into('<hh', data, transform + 40, 0, -1)
        struct.pack_into('<II', data, 0x60, 1, transform)
        _, uv_lookup = array(data, 0x98)
        struct.pack_into('<h', data, uv_lookup, 0)
    if pulse:
        low, high, period = pulse
        write_global_track(data, transparency, 1 if scroll else 0, sequence_count, [0, period // 2, period],
                           [(int(low * 0x7FFF),), (int(high * 0x7FFF),), (int(low * 0x7FFF),)], '<h')
    if fade:
        fade_alpha(data)

    radius = math.sqrt(max(abs(x0), abs(x1)) ** 2 + lean ** 2 + height ** 2)
    struct.pack_into('<6ff', data, 0xA0, x0, -lean, 0.0, x1, lean, height, radius)
    sequences_count, sequences = array(data, 0x1C)
    for sequence in range(sequences_count):
        struct.pack_into('<6ff', data, sequences + 64 * sequence + 32, x0, -lean, 0.0, x1, lean, height, radius)

    # Its skin: the eight corners, two quads; a plain batch (not projected on the floor), its texture flowing
    model_skin = bytearray(skin)
    triangles = [0, 1, 2, 3, 2, 1, 4, 5, 6, 7, 6, 5]
    struct.pack_into('<II', model_skin, 0x04, 8, append_block(model_skin, struct.pack('<8H', *range(8))))
    struct.pack_into('<II', model_skin, 0x0C, len(triangles),
                     append_block(model_skin, struct.pack(f'<{len(triangles)}H', *triangles)))
    struct.pack_into('<II', model_skin, 0x14, 8, append_block(model_skin, bytes(4 * 8)))
    _, submeshes = struct.unpack_from('<II', model_skin, 0x1C)
    struct.pack_into('<H', model_skin, submeshes + 6, 8)
    struct.pack_into('<H', model_skin, submeshes + 10, len(triangles))
    centre = ((x0 + x1) / 2.0, 0.0, height / 2.0)
    struct.pack_into('<3f', model_skin, submeshes + 20, *centre)
    struct.pack_into('<3f', model_skin, submeshes + 32, *centre)
    struct.pack_into('<f', model_skin, submeshes + 44, radius)
    _, batches = struct.unpack_from('<II', model_skin, 0x24)
    flags = model_skin[batches] & ~0x04
    if scroll:
        flags &= ~0x10
        struct.pack_into('<H', model_skin, batches + 22, 0)
    model_skin[batches] = flags
    return bytes(data), bytes(model_skin)


def extract_template(directory):
    list_path = os.path.join(directory, 'list.json')
    with open(list_path, 'w', encoding='utf-8') as output:
        json.dump([f'{TEMPLATE}.m2', f'{TEMPLATE}00.skin'], output)
    subprocess.run(['node', EXTRACTOR, list_path, directory], check=True, stdout=subprocess.DEVNULL)
    with open(os.path.join(directory, 'missing.json'), encoding='utf-8') as missing:
        if json.load(missing):
            raise SystemExit(f'{TEMPLATE}.m2 is not in the client')
    local = os.path.join(directory, *TEMPLATE.split('\\'))
    with open(local + '.m2', 'rb') as model, open(local + '00.skin', 'rb') as skin:
        return model.read(), skin.read()


def main():
    with open(os.path.join(HERE, 'shapes.json'), encoding='utf-8') as source:
        config = json.load(source)
    shutil.rmtree(OUTPUT_ROOT, ignore_errors=True)
    os.makedirs(OUTPUT_ROOT)
    with tempfile.TemporaryDirectory() as work:
        template, skin = extract_template(work)

    for shape in config['shapes']:
        stem = f"GI_{shape['key']}"
        # A shape drawn with another's texture (a carried circle: the circle at its own size) ships none of its own
        texture_stem = f"GI_{shape.get('texture', shape['key'])}"
        texture_path = f'{ARCHIVE_ROOT}\\{texture_stem}.blp'
        if shape['kind'] == 'curtain':
            # The pieces of a line share their tile's texture (`texture`: the shape that ships it)
            if 'texture' not in shape:
                write_painted_blp(build_curtain_texture(shape), os.path.join(OUTPUT_ROOT, f'{stem}.blp'))
            fade_spell(shape['spell'])
            for suffix, fade in (('', False), ('Fade', True)):
                model, model_skin = build_curtain_model(template, skin, shape, texture_path, fade)
                with open(os.path.join(OUTPUT_ROOT, f'{stem}{suffix}.m2'), 'wb') as output:
                    output.write(model)
                with open(os.path.join(OUTPUT_ROOT, f'{stem}{suffix}00.skin'), 'wb') as output:
                    output.write(model_skin)
            print(f"{stem}: curtain, {texture_stem}")
            continue
        if shape['kind'] == 'texture':
            write_painted_blp(build_painted_texture(shape), os.path.join(OUTPUT_ROOT, f'{stem}.blp'))
        elif 'texture' not in shape:
            if shape['kind'] == 'image':
                image = build_image_texture(shape)
            else:
                image = build_texture(shape, config['color'], config['alpha'])
            blp_writer.writeRawBlp(image, os.path.join(OUTPUT_ROOT, f'{stem}.blp'))
        shape_skin = animated_skin(skin) if shape.get('spin') else skin
        with open(os.path.join(OUTPUT_ROOT, f'{stem}.m2'), 'wb') as output:
            output.write(build_model(template, shape, texture_path))
        with open(os.path.join(OUTPUT_ROOT, f'{stem}00.skin'), 'wb') as output:
            output.write(shape_skin)
        # Its fading twin, on the same texture (a carried circle goes with its aura: none)
        if not shape.get('carried'):
            fade_spell(shape['spell'])
            with open(os.path.join(OUTPUT_ROOT, f'{stem}Fade.m2'), 'wb') as output:
                output.write(build_model(template, shape, texture_path, fade=True))
            with open(os.path.join(OUTPUT_ROOT, f'{stem}Fade00.skin'), 'wb') as output:
                output.write(shape_skin)
        print(f"{stem}: {texture_stem}")


if __name__ == '__main__':
    sys.exit(main())
