"""Builds the FX lab: a large, plain gray room to look at spell visuals in (.agents/docs/systems/fx-lab.md).

    python localTools/fxLab/buildFxLab.py

It takes over map 606, QA_DVD ("QA and DVD"), a test map of the stock client nothing uses, and rewrites it:

- clientPatcher/maps/World/Maps/QA_DVD/QA_DVD.wdt: one tile only, 30_26 (the stock map had a second one, 31_26)
- clientPatcher/maps/World/Maps/QA_DVD/QA_DVD.wdl: no distant terrain at all
- clientPatcher/maps/World/Maps/QA_DVD/QA_DVD_30_26.adt: the room, written from nothing (ADT v18, 3.3.5)
- clientPatcher/maps/Tileset/Evolutions/FxLab*.blp: its floor, line and wall textures
- server/Data/dbc/Light*.dbc: an even, neutral gray light for the map (no tint, no fog, the same all day). The
  server's DBC folder is not in git: the client build adds these rows again on its own (--light-only,
  clientPatcher/build/ClientGeneration.ps1) and ships the four tables in patch-Z (patchFiles.js).

The room: a flat floor at height 0, 10 x 10 map chunks (333 yards a side) in the middle of the tile, its middle at
(2933.33, 800). Every chunk round it is a plateau WallHeight high: the step between the two is one vertex (4.17 yards)
across, which reads as a wall and which nothing walks up. The floor's texture repeats once per cell (4.17 yards): a
faint line at each repeat. A second layer paints stronger lines on every chunk edge (33.3 yards), crossing at the
middle, and rings round the middle every 5 yards out to 40 (the tens stronger), for judging sizes.

Then ship it: the client step of localTools/deployWithProgress.ps1, and the server's terrain with
localTools/mapEditing/rebuildServerMaps.ps1 -maps 606 -skipVmaps (there is no object to collide with).
"""
import argparse
import math
import os
import struct

import numpy
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
MAPS_ROOT = os.path.join(REPO_ROOT, 'clientPatcher', 'maps')
DBC_ROOT = os.path.join(REPO_ROOT, 'server', 'Data', 'dbc')

MAP_ID = 606
MAP_DIRECTORY = 'QA_DVD'
TILE_X, TILE_Y = 30, 26             # the file's <x>_<y>: x runs along the world's Y axis, y along its X axis

TILE_SIZE = 1600.0 / 3.0            # 533.33 yards
CHUNK_SIZE = TILE_SIZE / 16.0       # 33.33 yards
UNIT = CHUNK_SIZE / 8.0             # 4.17 yards: one cell, one vertex step
TOP_X = (32 - TILE_Y) * TILE_SIZE   # the tile's corner the chunks count from (largest X, largest Y)
TOP_Y = (32 - TILE_X) * TILE_SIZE
CENTER_X = TOP_X - TILE_SIZE / 2.0  # 2933.33
CENTER_Y = TOP_Y - TILE_SIZE / 2.0  # 800

FLOOR_FIRST_CHUNK, FLOOR_LAST_CHUNK = 3, 12     # both ways: 10 chunks, 333 yards
WALL_HEIGHT = 40.0

TEXTURE_FOLDER = 'Tileset\\Evolutions'
TEXTURES = ['FxLabFloor.blp', 'FxLabLine.blp', 'FxLabWall.blp']
FLOOR_TEXTURE, LINE_TEXTURE, WALL_TEXTURE = 0, 1, 2

FLOOR_GRAY = 140
FLOOR_LINE_GRAY = 118
LINE_GRAY = 70
WALL_GRAY = 165

# The light: our own rows, appended to the stock tables (ids past the stock ones)
LIGHT_ID = 2600
LIGHT_PARAMS_ID = 950


# --- Textures ----------------------------------------------------------------------------------------------------

def write_blp(rgb, path):
    """Uncompressed BLP2 (BGRA, 8-bit alpha) with its mipmaps. Alpha 0: a terrain texture's alpha is its shine."""
    image = Image.fromarray(rgb.astype(numpy.uint8), 'RGB')
    mips = []
    while True:
        red, green, blue = image.split()
        alpha = Image.new('L', image.size, 0)
        mips.append(Image.merge('RGBA', (blue, green, red, alpha)).tobytes())
        if max(image.size) == 1 or len(mips) == 16:
            break
        image = image.resize((max(1, image.size[0] // 2), max(1, image.size[1] // 2)), Image.BOX)
    offsets, sizes = [0] * 16, [0] * 16
    cursor = 148
    for index, data in enumerate(mips):
        offsets[index], sizes[index] = cursor, len(data)
        cursor += len(data)
    height, width = rgb.shape[:2]
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as output:
        output.write(struct.pack('<4sIBBBBII16I16I', b'BLP2', 1, 3, 8, 8, 1, width, height, *offsets, *sizes))
        for data in mips:
            output.write(data)


def build_textures():
    folder = os.path.join(MAPS_ROOT, *TEXTURE_FOLDER.split('\\'))
    # The floor: one cell per repeat, a faint line along its edges (two texels each side: 0.07 yards)
    floor = numpy.full((256, 256, 3), FLOOR_GRAY, numpy.uint8)
    for edge in (0, 1, 254, 255):
        floor[edge, :, :] = FLOOR_LINE_GRAY
        floor[:, edge, :] = FLOOR_LINE_GRAY
    write_blp(floor, os.path.join(folder, TEXTURES[FLOOR_TEXTURE]))
    write_blp(numpy.full((64, 64, 3), LINE_GRAY, numpy.uint8), os.path.join(folder, TEXTURES[LINE_TEXTURE]))
    write_blp(numpy.full((64, 64, 3), WALL_GRAY, numpy.uint8), os.path.join(folder, TEXTURES[WALL_TEXTURE]))


# --- Terrain -----------------------------------------------------------------------------------------------------

def is_floor_chunk(index):
    return FLOOR_FIRST_CHUNK <= index <= FLOOR_LAST_CHUNK


def outer_height(row, column):
    """Height of the tile's outer vertex (row, column), 0..128 each: rows go down X, columns down Y"""
    first, last = FLOOR_FIRST_CHUNK * 8, (FLOOR_LAST_CHUNK + 1) * 8
    return 0.0 if first <= row <= last and first <= column <= last else WALL_HEIGHT


OUTER = numpy.array([[outer_height(row, column) for column in range(129)] for row in range(129)])
# A cell's middle vertex: the mean of its corners, so a cell is flat or one plane (the wall's slope)
INNER = (OUTER[:-1, :-1] + OUTER[1:, :-1] + OUTER[:-1, 1:] + OUTER[1:, 1:]) / 4.0


def chunk_vertices(chunk_row, chunk_column):
    """The chunk's 145 vertices in MCVT order (9 outer, 8 inner, ...): (row, column) in units, height"""
    vertices = []
    for row in range(17):
        if row % 2 == 0:
            for column in range(9):
                r, c = chunk_row * 8 + row // 2, chunk_column * 8 + column
                vertices.append((float(r), float(c), OUTER[r, c]))
        else:
            for column in range(8):
                r, c = chunk_row * 8 + row // 2, chunk_column * 8 + column
                vertices.append((r + 0.5, c + 0.5, INNER[r, c]))
    return vertices


def height_at(row, column):
    """The terrain's height at (row, column) in units, anywhere on the tile (clamped to it)"""
    row = min(max(row, 0.0), 128.0)
    column = min(max(column, 0.0), 128.0)
    r, c = min(int(row), 127), min(int(column), 127)
    fr, fc = row - r, column - c
    top = OUTER[r, c] * (1 - fc) + OUTER[r, c + 1] * fc
    bottom = OUTER[r + 1, c] * (1 - fc) + OUTER[r + 1, c + 1] * fc
    return top * (1 - fr) + bottom * fr


def normal_at(row, column):
    """(world X, world Y, up), as MCNR stores it. A row down is X down, a column down is Y down."""
    step = 0.5
    d_row = (height_at(row + step, column) - height_at(row - step, column)) / (2 * step * UNIT)
    d_column = (height_at(row, column + step) - height_at(row, column - step)) / (2 * step * UNIT)
    # dh/dX = -dh/drow, dh/dY = -dh/dcolumn; the normal is (-dh/dX, -dh/dY, 1)
    normal = numpy.array([d_row, d_column, 1.0])
    return normal / numpy.linalg.norm(normal)


def ring_alpha(distance):
    """The rings round the room's middle, every 5 yards out to 40, the tens stronger"""
    alpha = 0.0
    for radius in range(5, 45, 5):
        strength = 210.0 if radius % 10 == 0 else 120.0
        alpha = max(alpha, strength * max(0.0, 1.0 - abs(distance - radius) / 0.45))
    return alpha


def chunk_alpha(chunk_row, chunk_column):
    """The line layer's 64 x 64 alpha map over a floor chunk: its edges and the rings"""
    pixel = CHUNK_SIZE / 64.0
    alpha = numpy.zeros((64, 64))
    for y in range(64):
        for x in range(64):
            world_x = TOP_X - chunk_row * CHUNK_SIZE - (y + 0.5) * pixel
            world_y = TOP_Y - chunk_column * CHUNK_SIZE - (x + 0.5) * pixel
            edge = 230.0 if y in (0, 63) or x in (0, 63) else 0.0
            alpha[y, x] = max(edge, ring_alpha(math.hypot(world_x - CENTER_X, world_y - CENTER_Y)))
    return numpy.clip(alpha, 0, 255).astype(numpy.uint8).tobytes()


def sub_chunk(tag, payload, declared=None):
    return tag[::-1] + struct.pack('<I', len(payload) if declared is None else declared) + payload


def build_mcnk(chunk_row, chunk_column):
    floor = is_floor_chunk(chunk_row) and is_floor_chunk(chunk_column)
    vertices = chunk_vertices(chunk_row, chunk_column)

    mcvt = sub_chunk(b'MCVT', struct.pack('<145f', *[height for _, _, height in vertices]))
    normals = bytearray()
    for row, column, _ in vertices:
        normals += struct.pack('<3b', *[int(round(value * 127)) for value in normal_at(row, column)])
    # The size says 435, 13 more bytes follow it (as every stock file has them)
    mcnr = sub_chunk(b'MCNR', bytes(normals) + bytes(13), declared=435)
    if floor:
        layers = struct.pack('<4I', FLOOR_TEXTURE, 0, 0, 0) + struct.pack('<4I', LINE_TEXTURE, 0x100, 0, 0)
        alpha = chunk_alpha(chunk_row, chunk_column)
    else:
        layers = struct.pack('<4I', WALL_TEXTURE, 0, 0, 0)
        alpha = b''
    mcly = sub_chunk(b'MCLY', layers)
    mcrf = sub_chunk(b'MCRF', b'')
    mcal = sub_chunk(b'MCAL', alpha)
    mclq = sub_chunk(b'MCLQ', b'')
    mcse = sub_chunk(b'MCSE', b'')

    header_size = 8 + 128
    offset_mcvt = header_size
    offset_mcnr = offset_mcvt + len(mcvt)
    offset_mcly = offset_mcnr + len(mcnr)
    offset_mcrf = offset_mcly + len(mcly)
    offset_mcal = offset_mcrf + len(mcrf)
    offset_mclq = offset_mcal + len(mcal)
    offset_mcse = offset_mclq + len(mclq)

    header = bytearray(128)
    struct.pack_into('<15I', header, 0,
                     0x8000,                     # alpha maps 64 x 64 as they are (do not fix)
                     chunk_column, chunk_row,
                     2 if floor else 1,          # layers
                     0,                          # doodad references
                     offset_mcvt, offset_mcnr, offset_mcly, offset_mcrf, offset_mcal, len(alpha) + 8,
                     0, 0,                       # no shadow map
                     0,                          # area
                     0)                          # object references
    # holes 0, low quality texture map 0, no effect doodads, sound emitters at the end (none)
    struct.pack_into('<2I', header, 88, offset_mcse, 0)
    struct.pack_into('<2I', header, 96, offset_mclq, 8)
    struct.pack_into('<3f', header, 104, TOP_X - chunk_row * CHUNK_SIZE, TOP_Y - chunk_column * CHUNK_SIZE, 0.0)
    body = bytes(header) + mcvt + mcnr + mcly + mcrf + mcal + mclq + mcse
    return b'KNCM' + struct.pack('<I', len(body)) + body


def build_adt():
    textures = b''.join((TEXTURE_FOLDER + '\\' + name).encode('ascii') + b'\0' for name in TEXTURES)
    mver = sub_chunk(b'MVER', struct.pack('<I', 18))
    mhdr_size = 8 + 64
    mcin_size = 8 + 256 * 16
    mtex = sub_chunk(b'MTEX', textures)
    empties = [sub_chunk(tag, b'') for tag in (b'MMDX', b'MMID', b'MWMO', b'MWID', b'MDDF', b'MODF')]

    base = len(mver) + 8                         # MHDR offsets count from its data
    position = len(mver) + mhdr_size
    offsets = {'MCIN': position}
    position += mcin_size
    offsets['MTEX'] = position
    position += len(mtex)
    for tag, chunk in zip(('MMDX', 'MMID', 'MWMO', 'MWID', 'MDDF', 'MODF'), empties):
        offsets[tag] = position
        position += len(chunk)

    chunks = []
    entries = bytearray()
    for chunk_row in range(16):
        for chunk_column in range(16):
            chunk = build_mcnk(chunk_row, chunk_column)
            entries += struct.pack('<4I', position, len(chunk), 0, 0)
            chunks.append(chunk)
            position += len(chunk)

    mhdr = bytearray(64)
    struct.pack_into('<9I', mhdr, 0, 0, *[offsets[tag] - base for tag in
                                          ('MCIN', 'MTEX', 'MMDX', 'MMID', 'MWMO', 'MWID', 'MDDF', 'MODF')])
    data = mver + sub_chunk(b'MHDR', bytes(mhdr)) + sub_chunk(b'MCIN', bytes(entries)) + mtex + b''.join(empties)
    assert len(data) == offsets['MCIN'] + mcin_size + len(mtex) + sum(len(chunk) for chunk in empties)
    return data + b''.join(chunks)


def build_wdt():
    tiles = bytearray(64 * 64 * 8)
    struct.pack_into('<2I', tiles, (TILE_Y * 64 + TILE_X) * 8, 1, 0)
    mphd = bytearray(32)
    struct.pack_into('<I', mphd, 0, 0x4)        # 8-bit alpha maps (4096 bytes a layer)
    return (sub_chunk(b'MVER', struct.pack('<I', 18)) + sub_chunk(b'MPHD', bytes(mphd)) +
            sub_chunk(b'MAIN', bytes(tiles)) + sub_chunk(b'MWMO', b''))


def build_wdl():
    # No tile has distant terrain: past the room, nothing
    return (sub_chunk(b'MVER', struct.pack('<I', 18)) + sub_chunk(b'MWMO', b'') + sub_chunk(b'MWID', b'') +
            sub_chunk(b'MODF', b'') + sub_chunk(b'MAOF', bytes(64 * 64 * 4)))


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as output:
        output.write(data)


# --- Light -------------------------------------------------------------------------------------------------------

def read_dbc(name):
    with open(os.path.join(DBC_ROOT, name), 'rb') as source:
        data = source.read()
    magic, count, fields, size, string_size = struct.unpack_from('<4s4I', data, 0)
    assert magic == b'WDBC', name
    rows = [data[20 + index * size:20 + (index + 1) * size] for index in range(count)]
    return fields, size, rows, data[20 + count * size:20 + count * size + string_size]


def write_dbc(name, fields, size, rows, strings):
    data = struct.pack('<4s4I', b'WDBC', len(rows), fields, size, len(strings)) + b''.join(rows) + strings
    write(os.path.join(DBC_ROOT, name), data)


def replace_rows(name, new_rows):
    """Our rows in place of any earlier copy of them (the same ids), after the stock ones, ids in order"""
    fields, size, rows, strings = read_dbc(name)
    ids = {struct.unpack_from('<I', row, 0)[0] for row in new_rows}
    rows = [row for row in rows if struct.unpack_from('<I', row, 0)[0] not in ids]
    assert all(struct.unpack_from('<I', row, 0)[0] < min(ids) for row in rows), f'{name}: ids taken'
    assert all(len(row) == size for row in new_rows), name
    write_dbc(name, fields, size, rows + sorted(new_rows, key=lambda row: struct.unpack_from('<I', row, 0)[0]),
              strings)


def gray(value):
    return (value << 16) | (value << 8) | value


# One colour all day for each band of LightIntBand (diffuse, ambient, the sky from its top to the horizon, the fog,
# the shadows, the sun and its halo, the clouds, the water)
INT_BANDS = [gray(122), gray(140), gray(110), gray(125), gray(138), gray(150), gray(160), gray(154), gray(92),
             gray(192), gray(128), gray(128), 0, gray(128), gray(96), gray(64), gray(96), gray(64)]
# LightFloatBand: fog end (yards x 36: 830 yards), fog start (share of it), sun through clouds, cloud density, two
# unknown (the stock clear sky's)
FLOAT_BANDS = [30000.0, 0.6, 1.0, 0.0, 0.95, 1.0]


def band_row(band_id, value, value_format):
    return struct.pack('<2I16I', band_id, 1, *([0] * 16)) + struct.pack('<' + value_format * 16,
                                                                          *([value] + [0] * 15))


def build_light():
    replace_rows('Light.dbc', [struct.pack('<2I5f8I', LIGHT_ID, MAP_ID, 0.0, 0.0, 0.0, 0.0, 0.0,
                                           LIGHT_PARAMS_ID, LIGHT_PARAMS_ID, LIGHT_PARAMS_ID, LIGHT_PARAMS_ID,
                                           4, 0, 0, 0)])
    # No skybox, no clouds, little glow; the stock clear sky's water alphas
    replace_rows('LightParams.dbc', [struct.pack('<4I5f', LIGHT_PARAMS_ID, 0, 0, 0, 0.25, 0.5, 1.0, 0.75, 1.0)])
    replace_rows('LightIntBand.dbc', [band_row(LIGHT_PARAMS_ID * 18 - 17 + band, value, 'I')
                                      for band, value in enumerate(INT_BANDS)])
    replace_rows('LightFloatBand.dbc', [band_row(LIGHT_PARAMS_ID * 6 - 5 + band, value, 'f')
                                        for band, value in enumerate(FLOAT_BANDS)])


def main():
    parser = argparse.ArgumentParser(description='Builds the FX lab (map 606).')
    parser.add_argument('--light-only', action='store_true',
                        help='only the light rows in server/Data/dbc (the client build runs this)')
    if parser.parse_args().light_only:
        build_light()
        print(f'FX lab light: Light {LIGHT_ID}, LightParams {LIGHT_PARAMS_ID} (map {MAP_ID})')
        return
    build_textures()
    folder = os.path.join(MAPS_ROOT, 'World', 'Maps', MAP_DIRECTORY)
    write(os.path.join(folder, f'{MAP_DIRECTORY}.wdt'), build_wdt())
    write(os.path.join(folder, f'{MAP_DIRECTORY}.wdl'), build_wdl())
    write(os.path.join(folder, f'{MAP_DIRECTORY}_{TILE_X}_{TILE_Y}.adt'), build_adt())
    build_light()
    print(f'FX lab written: map {MAP_ID} ({MAP_DIRECTORY}), tile {TILE_X}_{TILE_Y}, middle '
          f'({CENTER_X:.2f}, {CENTER_Y:.2f}, 0), floor {(FLOOR_LAST_CHUNK - FLOOR_FIRST_CHUNK + 1) * CHUNK_SIZE:.0f} '
          f'yards a side, walls {WALL_HEIGHT:.0f} high')


if __name__ == '__main__':
    main()
