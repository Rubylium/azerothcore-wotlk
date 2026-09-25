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
"""
import importlib.util
import json
import math
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

spec = importlib.util.spec_from_file_location(
    'buildParagonArt', os.path.join(REPO_ROOT, 'localTools', 'interface', 'buildParagonArt.py'))
blp_writer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(blp_writer)


# --- Shapes: the signed distance from a point to the outline (negative inside), in model yards ----------------

def shape_bounds(shape):
    """(x0, x1, y0, y1) of the shape: x forward from its owner, y to its left"""
    kind = shape['kind']
    if kind == 'circle':
        return -1.0, 1.0, -1.0, 1.0
    if kind == 'rect':
        half = 0.5 / shape['ratio']
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
    if kind == 'rect':
        half = 0.5 / shape['ratio']
        dx = max(-x, x - 1.0)
        dy = abs(y) - half
        outside = math.hypot(max(dx, 0.0), max(dy, 0.0))
        return outside + min(max(dx, dy), 0.0)
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


# --- Models ------------------------------------------------------------------------------------------------------

def array(data, offset):
    return struct.unpack_from('<II', data, offset)


def track_values(data, track):
    """offset of every value of an M2Track, across its sequences"""
    count, offset = array(data, track + 12)
    for sequence in range(count):
        values, values_offset = array(data, offset + 8 * sequence)
        yield values, values_offset


def build_model(template, shape, texture_path):
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

    # Bounds of the quad, for culling
    radius = max(math.hypot(x, y) for x in (x0, x1) for y in (y0, y1))
    struct.pack_into('<6ff', data, 0xA0, x0, y0, 0.0, x1, y1, QUAD_HEIGHT, radius)
    sequences_count, sequences = array(data, 0x1C)
    for index in range(sequences_count):
        struct.pack_into('<6ff', data, sequences + 64 * index + 32, x0, y0, 0.0, x1, y1, QUAD_HEIGHT, radius)
    return bytes(data)


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
        if 'texture' not in shape:
            image = build_texture(shape, config['color'], config['alpha'])
            blp_writer.writeRawBlp(image, os.path.join(OUTPUT_ROOT, f'{stem}.blp'))
        with open(os.path.join(OUTPUT_ROOT, f'{stem}.m2'), 'wb') as output:
            output.write(build_model(template, shape, texture_path))
        with open(os.path.join(OUTPUT_ROOT, f'{stem}00.skin'), 'wb') as output:
            output.write(skin)
        print(f"{stem}: {texture_stem}")


if __name__ == '__main__':
    sys.exit(main())
