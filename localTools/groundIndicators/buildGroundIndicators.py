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
faded, so the border stays clear. `size`: its texture's side (a 6-yard cell seal at 1024); `keyed`: a painting on pure
black, its black keyed to transparent (alpha full from that brightness up: dark iron stays iron); `blend` "add": added
to the floor (a glow) rather than laid over it.

A shape's picture is its `image` file, or `picture` ("path.py:function", called with `args`): a fight's own art module
(localTools/wardenVorhan/indicatorArt.py) that renders numbers, builds a flipbook from key paintings, or draws a
placeholder until the painting is there.

Any model may breathe on its own animation rather than the world's clock: `pulseRamp` ([low, high, first period, last
period, ramp ms, hold ms]: faster and faster over the ramp, then at the last period) or `alphaKeys` ([[ms, alpha], ...]:
a flare). Its animation starts when the model is put on (the fading twins rely on it), so a warning's breathing
quickens with its cast. A global sequence would start anywhere in its loop.

A shape of kind "billboard" is an upright picture turned to the camera (a boss's eye over it, a number over a player's
head): see build_billboard_model.
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

from PIL import Image, ImageFilter

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


PICTURE_MODULES = {}


def art_function(name):
    """A function of a fight's art module, named "path.py:function" (the path from the repository)"""
    path, function = name.rsplit(':', 1)
    module = PICTURE_MODULES.get(path)
    if module is None:
        module_spec = importlib.util.spec_from_file_location(
            os.path.splitext(os.path.basename(path))[0], os.path.join(REPO_ROOT, *path.split('/')))
        module = importlib.util.module_from_spec(module_spec)
        module_spec.loader.exec_module(module)
        PICTURE_MODULES[path] = module
    return getattr(module, function)


def load_picture(shape, *extra):
    """A shape's source picture: its `image` file, or what its `picture` function returns (called with `args`, then
    extra), its black keyed to transparent if it is `keyed`"""
    if 'picture' in shape:
        image = art_function(shape['picture'])(*shape.get('args', []), *extra)
    else:
        image = Image.open(os.path.join(REPO_ROOT, *shape['image'].split('/')))
    if shape.get('keyed') and image.mode != 'RGBA':
        return key_black(image, shape['keyed'])
    return image.convert('RGBA')


def key_black(image, knee):
    """A painting on pure black made transparent there: alpha from its brightness (full from knee up), its colour
    unpremultiplied, so it reads as painted laid over the scene"""
    rgb = numpy.asarray(image.convert('RGB'), dtype=numpy.float64) / 255.0
    alpha = numpy.clip(rgb.max(axis=2, keepdims=True) / knee, 0.0, 1.0)
    colour = numpy.where(alpha > 1e-4, rgb / numpy.maximum(alpha, 1e-4), 0.0)
    rgba = numpy.concatenate([numpy.clip(colour, 0.0, 1.0), alpha], axis=2)
    return Image.fromarray(numpy.clip(rgba * 255.0 + 0.5, 0, 255).astype(numpy.uint8), 'RGBA')


def build_image_texture(shape):
    """The shape's picture fitted into the padded circle, softened and cut round (see the module's comment)"""
    x0, x1, _, _ = padded_bounds(shape)
    size = shape.get('size', IMAGE_TEXTURE_SIZE)
    source = load_picture(shape)
    # (The Hollow Voice's sigils came near square and are drawn as they always were)
    if 'picture' in shape and source.width != source.height:
        raise SystemExit(f"{shape['key']}: its picture is {source.width} x {source.height}, an image is square")
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
        if shape.get('upscale'):
            # A big area from a small painting: a sharpened Lanczos upscale first, rather than the bilinear
            # sampling's blur at several texels a pixel
            factor = int(shape['upscale'])
            source_image = source_image.resize((source_image.width * factor, source_image.height * factor),
                                               Image.LANCZOS).filter(ImageFilter.UnsharpMask(radius=2, percent=80))
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
        # `size`: a big area's own texture size (the devoured edge, 100 yards across: 512 texels blurred it)
        columns = rows = shape.get('size', PAINTED_TEXTURE_SIZE)
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


def write_sequence_track(data, track_offset, timelines, value_format):
    """Points an M2Track at keys on the model's own animation: one (timestamps, values) per sequence, linear"""
    time_entries = []
    value_entries = []
    for timestamps, values in timelines:
        time_entries.append((len(timestamps), append_block(data, struct.pack(f'<{len(timestamps)}I', *timestamps))))
        value_entries.append((len(values), append_block(data, b''.join(struct.pack(value_format, *value)
                                                                        for value in values))))
    time_arrays = append_block(data, b''.join(struct.pack('<II', *entry) for entry in time_entries))
    value_arrays = append_block(data, b''.join(struct.pack('<II', *entry) for entry in value_entries))
    struct.pack_into('<Hh', data, track_offset, 1, -1)
    struct.pack_into('<IIII', data, track_offset + 4, len(timelines), time_arrays, len(timelines), value_arrays)


# The template's sequence that plays as its model goes (Decay): it holds the last key rather than starting over
DECAY_ANIMATION = 159


def sequence_timelines(data, timestamps, values, length):
    """The same keys on every sequence of the model, each lasting at least length ms (the stock ones loop after half a
    second: keys past that were never reached), the one played as it goes holding the last key"""
    sequence_count, sequences = array(data, 0x1C)
    timelines = []
    for index in range(sequence_count):
        animation = struct.unpack_from('<H', data, sequences + 64 * index)[0]
        if animation == DECAY_ANIMATION:
            timelines.append(([0], [values[-1]]))
            continue
        # (never shortened: another of its tracks may run longer)
        duration = struct.unpack_from('<I', data, sequences + 64 * index + 4)[0]
        struct.pack_into('<I', data, sequences + 64 * index + 4, max(duration, int(length)))
        timelines.append((list(timestamps), list(values)))
    return timelines


def stepped(keys, length):
    """Keys [(ms, value)] held until the next one (linear tracks: each value held to a millisecond before the next,
    so the change is a step whichever interpolation the client gives the track)"""
    timestamps, values = [], []
    for index, (time, value) in enumerate(keys):
        end = keys[index + 1][0] - 1 if index + 1 < len(keys) else length
        timestamps += [int(time), int(max(time, end))]
        values += [value, value]
    return timestamps, values


def ramp_keys(pulse_ramp):
    """A breathing that quickens: low and high in turn, its period going from first to last over ramp ms, then held
    at last until ramp + hold"""
    low, high, first, last, ramp, hold = pulse_ramp
    keys = []
    time = 0.0
    while time < ramp + hold:
        period = first + (last - first) * min(1.0, time / ramp) if ramp else last
        keys += [(int(time), low), (int(time + period / 2), high)]
        time += period
    keys.append((int(time), low))
    return keys


def own_alpha(data, shape):
    """A shape's breathing on its own animation (pulseRamp or alphaKeys, see the module's comment): its
    transparency's keys, from when it is put on"""
    if shape.get('pulseRamp'):
        keys = ramp_keys(shape['pulseRamp'])
    elif shape.get('alphaKeys'):
        keys = [tuple(key) for key in shape['alphaKeys']]
    else:
        return
    length = keys[-1][0] + 1
    _, transparency = array(data, 0x58)
    values = [(int(alpha * 0x7FFF),) for _, alpha in keys]
    write_sequence_track(data, transparency, sequence_timelines(data, [time for time, _ in keys], values, length),
                         '<h')


def hold_alpha(data, shape):
    """A fading twin carries on from where its shape's own flare ended (alphaKeys: its last alpha), not from full"""
    if not shape.get('alphaKeys'):
        return
    last = int(shape['alphaKeys'][-1][1] * 0x7FFF)
    transparency_count, transparency = array(data, 0x58)
    for item in range(transparency_count):
        for values, offset in track_values(data, transparency + 20 * item):
            for key in range(values):
                struct.pack_into('<h', data, offset + 2 * key, last)


# Whether the client scales a texture about its middle, as it turns one (GROW_CENTRED; an in-game check: a growing
# ring then grows from the middle of its area - from a corner, set it False and the scaling is moved back onto the
# middle by a translation)
GROW_CENTRED = True
# A growth's keys, eased out (fast, then settling on its full size)
GROW_STEPS = 10


def grow(data, shape, fade):
    """A painting growing from the middle of its area (`grow` [first share of its size, ms]: a shockwave), on its own
    animation from when it is put on: its texture scaled down about its middle, its outside clear (clamped to the clear
    border). Its fading twin stays at full size."""
    if not shape.get('grow'):
        return
    first, duration = shape['grow']
    sequence_count, _ = array(data, 0x1C)
    if fade:
        keys = [(0, 1.0)]
    else:
        keys = []
        for step in range(GROW_STEPS + 1):
            t = step / GROW_STEPS
            eased = 1.0 - (1.0 - t) ** 2.2
            keys.append((int(duration * t), first + (1.0 - first) * eased))
    length = keys[-1][0] + 1

    def scaled(share):
        return (1.0 / share, 1.0 / share, 1.0)

    def moved(share):
        offset = 0.0 if GROW_CENTRED else 0.5 * (1.0 - 1.0 / share)
        return (offset, offset, 0.0)
    transform = append_block(data, bytes(60))
    times = [time for time, _ in keys]
    write_sequence_track(data, transform + 40,
                         sequence_timelines(data, times, [scaled(share) for _, share in keys], length), '<3f')
    write_sequence_track(data, transform, sequence_timelines(data, times, [moved(share) for _, share in keys], length),
                         '<3f')
    struct.pack_into('<hh', data, transform + 20, 0, -1)
    struct.pack_into('<II', data, 0x60, 1, transform)
    _, uv_lookup = array(data, 0x98)
    struct.pack_into('<h', data, uv_lookup, 0)


# How a model is drawn over what is behind it (M2 blending modes): laid over it, or added to it (a glow)
BLEND_MODES = {'alpha': 2, 'add': 4}


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
    if 94200 <= spell < 94250:
        return spell + 300
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

    # Alpha-blended rather than additive: added red washes out to pink on bright ground (a glow may be added: `blend`)
    _, materials = array(data, 0x70)
    flags, _ = struct.unpack_from('<HH', data, materials)
    struct.pack_into('<HH', data, materials, flags, BLEND_MODES[shape.get('blend', 'alpha')])

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
        hold_alpha(data, shape)
    else:
        own_alpha(data, shape)
    grow(data, shape, fade)

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
#
# A wall (the Warden's electrified barrier) is a curtain of one plane (`planes` 1), its painting's whole height from
# the floor to its top (`band` "full": a band painted bottom to top, its own fade kept), its texture `textureSize`
# a frame, and `frames` paintings of it (its picture called with 1 to frames) stacked in one texture that it flickers
# through, one every `flicker` ms in a fixed shuffled order, while it flows.
CURTAIN_TEXTURE = (512, 128)


def build_curtain_frame(shape, source_image):
    rgb = numpy.clip(numpy.asarray(source_image.convert('RGB'), dtype=numpy.float64) / 255.0 * shape.get('gain', 1.0),
                     0.0, 1.0)
    source = numpy.concatenate([rgb, rgb.max(axis=2, keepdims=True)], axis=2)
    height, width = source.shape[:2]
    columns, rows = shape.get('textureSize', CURTAIN_TEXTURE)
    xs = (numpy.arange(columns) + 0.5) / columns * (width - 1.0)
    v = (numpy.arange(rows) + 0.5) / rows             # 0 at the top of the curtain, 1 at the floor
    if shape.get('band') == 'full':
        if abs(width / height - columns / rows) > 0.01:
            raise SystemExit(f"{shape['key']}: its painting is {width} x {height}, its texture {columns} x {rows} "
                             f"(never stretched: paint it at that aspect)")
        X, Y = numpy.meshgrid(xs, v * (height - 1.0))
        return sample(source, X, Y)
    rows_mean = source[..., 3].mean(axis=1)
    lit = numpy.nonzero(rows_mean > 0.1 * rows_mean.max())[0]
    top, middle = float(lit.min()), (float(lit.min()) + float(lit.max())) / 2.0
    ys = top + v * (middle - top)
    X, Y = numpy.meshgrid(xs, ys)
    result = sample(source, X, Y)
    # Fading up: full at the floor, nothing at the top
    result *= (v ** 1.5)[:, None, None]
    return result


def build_curtain_texture(shape):
    frames = int(shape.get('frames', 1))
    if frames == 1:
        source = load_picture(shape) if 'picture' in shape else \
            Image.open(os.path.join(REPO_ROOT, *shape['image'].split('/')))
        return build_curtain_frame(shape, source)
    # The frames one under the other, the first at the top
    return numpy.concatenate([build_curtain_frame(shape, load_picture(shape, frame))
                              for frame in range(1, frames + 1)], axis=0)


def flicker_order(frames, steps):
    """The frames in a fixed shuffled order, never the same twice running"""
    rng = numpy.random.default_rng(7)
    order = [0]
    while len(order) < steps:
        frame = int(rng.integers(frames))
        if frame != order[-1]:
            order.append(frame)
    return order


# A curtain's coming (`appear`: ms): its alpha ramping up from nothing and the curtain rising out of the floor, eased
# out, on its own animation from when it is put on - shown at once, a wall popped up. Its animation then lasts
# APPEAR_HOLD_MS, so it never starts over (and rises again) while it stands.
APPEAR_HOLD_MS = 600000
APPEAR_STEPS = 6


def appear(data, shape, height):
    duration = int(shape.get('appear', 0))
    if not duration:
        return
    times = [int(duration * step / APPEAR_STEPS) for step in range(APPEAR_STEPS + 1)]
    eased = [1.0 - (1.0 - step / APPEAR_STEPS) ** 2 for step in range(APPEAR_STEPS + 1)]
    colors_count, colors = array(data, 0x48)
    for color in range(colors_count):
        write_sequence_track(data, colors + 40 * color + 20,
                             sequence_timelines(data, times, [(int(0x7FFF * min(1.0, 1.4 * share)),)
                                                              for share in eased], APPEAR_HOLD_MS), '<h')
    # Rising: every vertex hangs on the first bone, moved up from a curtain's height under the floor
    _, bones = array(data, 0x2C)
    flags = struct.unpack_from('<I', data, bones + 4)[0]
    struct.pack_into('<I', data, bones + 4, flags | BONE_TRANSFORMED)
    write_sequence_track(data, bones + 16,
                         sequence_timelines(data, times, [(0.0, 0.0, -height * (1.0 - share)) for share in eased],
                                            APPEAR_HOLD_MS), '<3f')


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

    # Two planes, an X across the line: from one side at the floor to the other at the top, and back. `rise` lifts its
    # far end that many yards over its near one: a whole line from a room's middle to its rising edge (the Hollow
    # Voice's floor climbs 1.6 yards to the walls) would otherwise sink into the floor as it goes
    rise = float(shape.get('rise', 0.0))
    # A flickering curtain shows one frame of its texture at a time: the first's rows, moved to the others'
    frames = int(shape.get('frames', 1))
    floor_v = 1.0 / frames
    corners = []
    if int(shape.get('planes', 2)) == 1:
        lean = 0.0
        corners += [(x0, 0.0, 0.0, u0, floor_v), (x1, 0.0, rise, u1, floor_v),
                    (x0, 0.0, height, u0, 0.0), (x1, 0.0, height + rise, u1, 0.0)]
    else:
        for side in (-1.0, 1.0):
            corners += [(x0, side * lean, 0.0, u0, floor_v), (x1, side * lean, rise, u1, floor_v),
                        (x0, -side * lean, height, u0, 0.0), (x1, -side * lean, height + rise, u1, 0.0)]
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
        if frames > 1:
            # Flowing along u and flickering down v together: a key at every flicker, u running on evenly, v
            # stepping to the next frame within a millisecond
            flicker = int(shape['flicker'])
            if scroll % flicker:
                raise SystemExit(f"{shape['key']}: its scroll ({scroll} ms) is not a whole number of flickers")
            steps = scroll // flicker
            order = flicker_order(frames, steps)
            timestamps, values = [], []
            for step in range(steps):
                start, end = step * flicker, (step + 1) * flicker - 1
                v = FLIPBOOK_SIGN * order[step] / frames
                timestamps += [start, end]
                values += [(-start / scroll, v, 0.0), (-end / scroll, v, 0.0)]
            timestamps.append(scroll)
            values.append((-1.0, FLIPBOOK_SIGN * order[0] / frames, 0.0))
            write_global_track(data, transform, 0, sequence_count, timestamps, values, '<3f')
        else:
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
    else:
        appear(data, shape, height)

    top = height + rise
    radius = math.sqrt(max(abs(x0), abs(x1)) ** 2 + lean ** 2 + top ** 2)
    struct.pack_into('<6ff', data, 0xA0, x0, -lean, 0.0, x1, lean, top, radius)
    sequences_count, sequences = array(data, 0x1C)
    for sequence in range(sequences_count):
        struct.pack_into('<6ff', data, sequences + 64 * sequence + 32, x0, -lean, 0.0, x1, lean, top, radius)

    # Its skin: the corners, a quad for each plane; a plain batch (not projected on the floor), its texture flowing
    model_skin = bytearray(skin)
    count = len(corners)
    triangles = [0, 1, 2, 3, 2, 1, 4, 5, 6, 7, 6, 5][:count // 4 * 6]
    struct.pack_into('<II', model_skin, 0x04, count, append_block(model_skin, struct.pack(f'<{count}H', *range(count))))
    struct.pack_into('<II', model_skin, 0x0C, len(triangles),
                     append_block(model_skin, struct.pack(f'<{len(triangles)}H', *triangles)))
    struct.pack_into('<II', model_skin, 0x14, count, append_block(model_skin, bytes(4 * count)))
    _, submeshes = struct.unpack_from('<II', model_skin, 0x1C)
    struct.pack_into('<H', model_skin, submeshes + 6, count)
    struct.pack_into('<H', model_skin, submeshes + 10, len(triangles))
    centre = ((x0 + x1) / 2.0, 0.0, top / 2.0)
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


# A shape of kind "billboard" is one picture standing up, `width` x `height` yards, its middle `elevation` yards over
# its owner's feet, always turned to the camera: its bone carries the M2 billboard flag (the client turns a billboarded
# bone's local x to face the camera, as the glows and halos of the stock models do), its quad stands in the bone's
# y-z plane round the bone's pivot. Drawn unlit, both sides, laid over the scene (`blend` "add" adds it: a glow).
# carried: an aura on a player (their number over their head), at its own size.
#
# A flipbook (`grid` [columns, rows]: its picture an atlas of frames, row by row) steps through its frames on the
# model's own animation, from when it is put on: `timeline` [[ms, frame], ...], each frame held until the next key (a
# texture transform's translation, moved from one frame to the next within a millisecond), the last held to the end.
# Its fading twin holds the last frame while it fades.
#
# Two things only the game can tell, set here once checked in game:
# - which way the texture transform moves the picture: FLIPBOOK_SIGN 1 if a translation of +t shows the texels at
#   uv + t (the frames then play in order; the other way they play scrambled), -1 otherwise;
# - which way the billboard turns the quad: BILLBOARD_MIRROR False if a number reads the right way round, True if it
#   reads mirrored.
FLIPBOOK_SIGN = 1
BILLBOARD_MIRROR = False
# M2 bone flags: spherical billboard (turned to face the camera on every axis), and transformed (computed each frame)
BONE_BILLBOARD = 0x8
BONE_TRANSFORMED = 0x200
# M2 material flag: not tested against the depth buffer
MATERIAL_NO_DEPTH_TEST = 0x8


def write_flipbook(data, shape, fade):
    """A flipbook's texture transform (`grid`, `timeline`: see above), on the model's own animation; its fading twin
    holds the last frame. Nothing without a timeline; one named "path.py:function" is what that function returns
    (made with the frames it times)."""
    timeline = shape.get('timeline')
    if not timeline:
        return
    if isinstance(timeline, str):
        timeline = art_function(timeline)()
    columns, rows = shape.get('grid', [1, 1])

    def offset(frame):
        return (FLIPBOOK_SIGN * (frame % columns) / columns, FLIPBOOK_SIGN * (frame // columns) / rows, 0.0)
    keys = [(time, offset(frame)) for time, frame in timeline]
    if fade:
        keys = [(0, keys[-1][1])]
    length = int(shape.get('length', keys[-1][0] + 1000))
    timestamps, values = stepped(keys, length)
    transform = append_block(data, bytes(60))
    write_sequence_track(data, transform, sequence_timelines(data, timestamps, values, length), '<3f')
    struct.pack_into('<hh', data, transform + 20, 0, -1)
    struct.pack_into('<hh', data, transform + 40, 0, -1)
    struct.pack_into('<II', data, 0x60, 1, transform)
    _, uv_lookup = array(data, 0x98)
    struct.pack_into('<h', data, uv_lookup, 0)


def build_billboard_model(template, skin, shape, texture_path, fade=False):
    data = bytearray(template)
    width = float(shape['width'])
    height = float(shape['height'])
    elevation = float(shape.get('elevation', 0.0))
    columns, rows = shape.get('grid', [1, 1])
    du, dv = 1.0 / columns, 1.0 / rows

    # The quad in the y-z plane, facing +x: the picture's left (u 0) at -y, which the camera sees on its left once the
    # bone's x faces it; its top (v 0) up
    side = -1.0 if BILLBOARD_MIRROR else 1.0
    left, right = -side * width / 2.0, side * width / 2.0
    bottom, top = elevation - height / 2.0, elevation + height / 2.0
    count, vertices = array(data, 0x3C)
    if count != 4:
        raise SystemExit(f'{TEMPLATE}.m2 changed: {count} vertices, expected its one quad')
    corners = [(left, bottom, 0.0, dv), (right, bottom, du, dv), (left, top, 0.0, 0.0), (right, top, du, 0.0)]
    for index, (y, z, u, v) in enumerate(corners):
        struct.pack_into('<3f', data, vertices + 48 * index, 0.0, y, z)
        struct.pack_into('<3f', data, vertices + 48 * index + 20, 1.0, 0.0, 0.0)
        struct.pack_into('<2f', data, vertices + 48 * index + 32, u, v)

    # Its bone (every vertex hangs on the first) turned to the camera about the quad's middle
    _, bones = array(data, 0x2C)
    flags = struct.unpack_from('<I', data, bones + 4)[0]
    struct.pack_into('<I', data, bones + 4, flags | BONE_BILLBOARD | BONE_TRANSFORMED)
    struct.pack_into('<3f', data, bones + 76, 0.0, 0.0, elevation)

    # Its texture, clamped (a flipbook's frames stay within their cells)
    _, lookup = array(data, 0x80)
    texture_index = struct.unpack_from('<h', data, lookup)[0]
    _, textures = array(data, 0x50)
    name_offset = append_block(data, texture_path.encode('ascii') + b'\0')
    struct.pack_into('<I', data, textures + 16 * texture_index + 4, 0)
    struct.pack_into('<II', data, textures + 16 * texture_index + 8, len(texture_path) + 1, name_offset)
    _, materials = array(data, 0x70)
    material_flags, _ = struct.unpack_from('<HH', data, materials)
    # `depthTest` false: drawn over whatever stands before it (an eye on a boss, never hidden inside his body)
    if shape.get('depthTest', True) is False:
        material_flags |= MATERIAL_NO_DEPTH_TEST
    struct.pack_into('<HH', data, materials, material_flags, BLEND_MODES[shape.get('blend', 'alpha')])

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

    # A breathing glow on the world's clock (`pulse`), or on its own animation (`pulseRamp`, `alphaKeys`)
    animate(data, dict(shape, spin=0))
    if fade:
        fade_alpha(data)
        hold_alpha(data, shape)
    else:
        own_alpha(data, shape)

    # The flipbook: the first frame's cell moved onto each frame's in turn
    timeline = shape.get('timeline')
    write_flipbook(data, shape, fade)

    reach = max(width, height) / 2.0
    radius = math.sqrt(reach ** 2 + (elevation + reach) ** 2)
    box = (-reach, -reach, min(0.0, bottom - reach), reach, reach, top + reach)
    struct.pack_into('<6ff', data, 0xA0, *box, radius)
    sequences_count, sequences = array(data, 0x1C)
    for sequence in range(sequences_count):
        struct.pack_into('<6ff', data, sequences + 64 * sequence + 32, *box, radius)

    # Its skin: the quad drawn as a plain batch (not projected on the floor), its texture moving if it is a flipbook
    model_skin = bytearray(skin)
    _, submeshes = struct.unpack_from('<II', model_skin, 0x1C)
    centre = (0.0, 0.0, elevation)
    struct.pack_into('<3f', model_skin, submeshes + 20, *centre)
    struct.pack_into('<3f', model_skin, submeshes + 32, *centre)
    struct.pack_into('<f', model_skin, submeshes + 44, reach * 1.5)
    _, batches = struct.unpack_from('<II', model_skin, 0x24)
    batch_flags = model_skin[batches] & ~0x04
    if timeline:
        batch_flags &= ~0x10
        struct.pack_into('<H', model_skin, batches + 22, 0)
    model_skin[batches] = batch_flags
    return bytes(data), bytes(model_skin)


def build_billboard_texture(shape):
    """Its picture as painted (a flipbook's atlas as its function lays it out), premultiplied for its mipmaps"""
    picture = load_picture(shape)
    columns, rows = shape.get('grid', [1, 1])
    cell_aspect = (picture.width / columns) / (picture.height / rows)
    if abs(cell_aspect - float(shape['width']) / float(shape['height'])) > 0.01:
        raise SystemExit(f"{shape['key']}: its frames are {cell_aspect:.3f} wide for 1 high, the quad "
                         f"{shape['width']} x {shape['height']} (never stretched)")
    return premultiplied(picture)


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
        if shape['kind'] == 'billboard':
            if 'texture' not in shape:
                write_painted_blp(build_billboard_texture(shape), os.path.join(OUTPUT_ROOT, f'{stem}.blp'))
            # Its fading twin (a carried one goes with its aura: none)
            looks = [('', False)] if shape.get('carried') else [('', False), ('Fade', True)]
            if not shape.get('carried'):
                fade_spell(shape['spell'])
            for suffix, fade in looks:
                model, model_skin = build_billboard_model(template, skin, shape, texture_path, fade)
                with open(os.path.join(OUTPUT_ROOT, f'{stem}{suffix}.m2'), 'wb') as output:
                    output.write(model)
                with open(os.path.join(OUTPUT_ROOT, f'{stem}{suffix}00.skin'), 'wb') as output:
                    output.write(model_skin)
            print(f"{stem}: billboard, {texture_stem}")
            continue
        if shape['kind'] == 'texture':
            write_painted_blp(build_painted_texture(shape), os.path.join(OUTPUT_ROOT, f'{stem}.blp'))
        elif 'texture' not in shape:
            if shape['kind'] == 'image':
                image = build_image_texture(shape)
            else:
                image = build_texture(shape, config['color'], config['alpha'])
            blp_writer.writeRawBlp(image, os.path.join(OUTPUT_ROOT, f'{stem}.blp'))
        shape_skin = animated_skin(skin) if shape.get('spin') or shape.get('grow') else skin
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
