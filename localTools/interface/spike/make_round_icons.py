"""Retail-style round character creation icons built from the retail client's own art.

Sources (extracted read-only from the local retail install with TACTTool, 12.1.0.69814):
- FileDataID 1662186 (UiTextureAtlas 897): classicon-* and raceicon128-* (128 px square portraits)
- FileDataID 1253496 (UiTextureAtlas 708): charactercreate-ring-metallight, charactercreate-ring-select,
  charactercreate-gendericon-*
Atlas member coordinates come from wago.tools UiTextureAtlasMember.

Each portrait is cut to a circle and framed by the retail metal ring, laid out on the 3.3.5 atlas grids used by
the creation screen (same cells as RACE_ICON_TCOORDS / CLASS_ICON_TCOORDS). Output: 32-bit top-left TGA.
"""
import csv
import os
import sys

from PIL import Image, ImageChops, ImageDraw

sys.path.insert(0, os.path.join(os.environ['TEMP'], 'claude'))
from blp import read_blp  # noqa: E402

TEMP = os.environ['TEMP']
RETAIL = os.path.join(TEMP, 'claude', 'retail')
OUT = os.path.join(TEMP, 'claude', 'spike', 'payload', 'Interface', 'Glues', 'CharacterCreate')

CELL = 128
GLOW_SCALE = 1.36  # glow textures are drawn 1.36x the button size in the XML

ATLASES = {'897': read_blp(os.path.join(RETAIL, '1662186.blp')), '708': read_blp(os.path.join(RETAIL, '1253496.blp'))}
MEMBERS = {row['CommittedName']: row for row in csv.DictReader(open(os.path.join(RETAIL, 'atlasmember.csv'),
                                                                     encoding='utf8'))
           if row['UiTextureAtlasID'] in ATLASES}


def atlas(name):
    row = MEMBERS[name]
    box = tuple(int(row[key]) for key in ('CommittedLeft', 'CommittedTop', 'CommittedRight', 'CommittedBottom'))
    return ATLASES[row['UiTextureAtlasID']].crop(box)


def write_tga(image, path):
    image = image.convert('RGBA')
    width, height = image.size
    header = bytearray(18)
    header[2] = 2
    header[12:14] = width.to_bytes(2, 'little')
    header[14:16] = height.to_bytes(2, 'little')
    header[16] = 32
    header[17] = 0x28  # top-left origin, 8 alpha bits
    r, g, b, a = image.split()
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as file:
        file.write(header)
        file.write(Image.merge('RGBA', (b, g, r, a)).tobytes())


def square_canvas(image):
    """Centers an almost-square atlas member (e.g. 278x280) on a square transparent canvas."""
    size = max(image.size)
    canvas = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    canvas.alpha_composite(image, ((size - image.size[0]) // 2, (size - image.size[1]) // 2))
    return canvas


def ring_radii(ring):
    """Outer and inner radius of a ring texture, measured on its alpha along the horizontal center line."""
    alpha = ring.getchannel('A')
    size = ring.size[0]
    row = [alpha.getpixel((x, size // 2)) for x in range(size // 2)]
    solid = [x for x, value in enumerate(row) if value > 128]
    return size / 2 - solid[0], size / 2 - solid[-1]


METAL_RING = square_canvas(atlas('charactercreate-ring-metallight'))
OUTER, INNER = ring_radii(METAL_RING)
RING_SCALE = (CELL / 2 - 1) / OUTER                   # ring fills the cell
RING = METAL_RING.resize((round(METAL_RING.size[0] * RING_SCALE),) * 2, Image.LANCZOS)
ICON_RADIUS = INNER * RING_SCALE + 1.5                 # art tucks just under the ring


def circle_mask(size, radius, supersample=4):
    big = Image.new('L', (size * supersample, size * supersample), 0)
    center = size * supersample / 2
    r = radius * supersample
    ImageDraw.Draw(big).ellipse((center - r, center - r, center + r, center + r), fill=255)
    return big.resize((size, size), Image.LANCZOS)


ICON_MASK = circle_mask(CELL, ICON_RADIUS)


def centered(image, size):
    canvas = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    canvas.alpha_composite(image, ((size - image.size[0]) // 2, (size - image.size[1]) // 2))
    return canvas


def round_icon(art, background=None):
    diameter = round(ICON_RADIUS * 2) + 2
    art = art.resize((diameter, diameter), Image.LANCZOS)
    layer = centered(art, CELL)
    if background is not None:
        base = Image.new('RGBA', (CELL, CELL), background)
        base.alpha_composite(layer)
        layer = base
    layer.putalpha(ImageChops.multiply(layer.getchannel('A'), ICON_MASK))
    result = Image.new('RGBA', (CELL, CELL), (0, 0, 0, 0))
    result.alpha_composite(layer)
    result.alpha_composite(centered(RING, CELL))
    return result


def build_grid(target_name, columns, rows, cells):
    image = Image.new('RGBA', (columns * CELL, rows * CELL), (0, 0, 0, 0))
    for (column, row), icon in cells.items():
        image.alpha_composite(icon, (column * CELL, row * CELL))
    write_tga(image, os.path.join(OUT, target_name + '.tga'))
    image.save(os.path.join(TEMP, 'claude', 'spike', 'previews', target_name + '.png'))


def glow_texture(source, name, tint=None, opacity=1.0):
    """Glow ring drawn so its ring sits exactly on the metal ring when shown at GLOW_SCALE x the button size."""
    ring = square_canvas(source)
    outer, inner = ring_radii(ring)
    target_radius = (OUTER + INNER) / 2 * RING_SCALE / GLOW_SCALE
    scale = target_radius / ((outer + inner) / 2)
    ring = ring.resize((round(ring.size[0] * scale),) * 2, Image.LANCZOS)
    texture = centered(ring, CELL) if ring.size[0] <= CELL else ring.crop(
        ((ring.size[0] - CELL) // 2,) * 2 + ((ring.size[0] + CELL) // 2,) * 2)
    if tint is not None:
        gray = texture.convert('L')
        colored = Image.merge('RGBA', [gray.point(lambda v, c=c: v * c // 255) for c in tint]
                              + [texture.getchannel('A')])
        texture = colored
    if opacity < 1.0:
        texture.putalpha(texture.getchannel('A').point(lambda v: int(v * opacity)))
    write_tga(texture, os.path.join(OUT, name + '.tga'))
    texture.save(os.path.join(TEMP, 'claude', 'spike', 'previews', name + '.png'))


CLASS_CELLS = {  # CLASS_ICON_TCOORDS cells (4x4 grid)
    'warrior': (0, 0), 'mage': (1, 0), 'rogue': (2, 0), 'druid': (3, 0),
    'hunter': (0, 1), 'shaman': (1, 1), 'priest': (2, 1), 'warlock': (3, 1),
    'paladin': (0, 2), 'deathknight': (1, 2),
    'demonhunter': (2, 2),  # placeholder art for the custom class until its own icon exists
}
RACE_COLUMNS = {  # RACE_ICON_TCOORDS cells (8x4 grid): rows male alliance, male horde, female alliance, female horde
    'human': (0, 0), 'dwarf': (1, 0), 'gnome': (2, 0), 'nightelf': (3, 0), 'draenei': (4, 0),
    'tauren': (0, 1), 'undead': (1, 1), 'troll': (2, 1), 'orc': (3, 1), 'bloodelf': (4, 1),
}

if __name__ == '__main__':
    os.makedirs(os.path.join(TEMP, 'claude', 'spike', 'previews'), exist_ok=True)
    build_grid('RoundClasses', 4, 4, {cell: round_icon(atlas('classicon-' + name))
                                      for name, cell in CLASS_CELLS.items()})
    races = {}
    for name, (column, row) in RACE_COLUMNS.items():
        races[(column, row)] = round_icon(atlas('raceicon128-' + name + '-male'))
        races[(column, row + 2)] = round_icon(atlas('raceicon128-' + name + '-female'))
    build_grid('RoundRaces', 8, 4, races)
    build_grid('RoundGender', 2, 1, {
        (0, 0): round_icon(atlas('charactercreate-gendericon-male'), background=(12, 10, 8, 255)),
        (1, 0): round_icon(atlas('charactercreate-gendericon-female'), background=(12, 10, 8, 255)),
    })
    glow_texture(atlas('charactercreate-ring-select'), 'RoundSelected')
    glow_texture(atlas('charactercreate-ring-select'), 'RoundHighlight', tint=(255, 255, 255), opacity=0.55)
    shadow = Image.new('RGBA', (CELL, CELL), (0, 0, 0, 0))
    shadow.putalpha(circle_mask(CELL, CELL / 2 - 2).point(lambda v: int(v * 0.6)))
    write_tga(shadow, os.path.join(OUT, 'RoundShadow.tga'))
    print('retail round icons written to', OUT, 'ring radii', OUTER, INNER)
