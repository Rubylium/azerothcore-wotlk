"""Valérian l'Ascensionné's body and wings: the Maw's Kyrian (Ascension's conversion, imported by
localTools/valerian/ascensionVisuals.json) made his own - a copy of the model at its own path with:

- the armour and body repainted to the Light: ivory plates, gold where the steel caught the light (rims, ridges,
  trims), gold cloth;
- its feathered wings turned to blades of light (Inarius, Diablo IV): their feathers painted white-gold, glowing from
  the root, drawn additive and unlit (black is nothing: each feather a blade of its own);
- the Maw's tattered shroud on its back (six strips hanging from the shoulders, its pattern scrolling down them) turned
  to tendrils of light: a thin core down each strip, white at the shoulders to gold at the tips, the scrolling pattern
  golden energy flowing along them, additive; the Maw's dark mist round him a faint golden haze.

    python localTools/valerian/buildAscendedModel.py

Reads the import (modules/mod-stat-growth/client-assets/importedValerian/files/Creature/kyrianmaw: run the import
first), writes Creature/ValerianAscended/ into the module's own client files (client-assets/files, shipped by
patchFiles.js): the model, its skins and animation files under the new name, and its textures. Creature display 60022
shows it (patchSinisterStrike.ps1). The model is edited in place, nothing moved: five materials' blend and flags, four
fixed textures renamed (their names appended at the end of the file).
"""
import importlib.util
import os
import shutil
import struct

import numpy
from PIL import Image, ImageFilter

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SOURCE = os.path.join(REPO, 'modules', 'mod-stat-growth', 'client-assets', 'importedValerian', 'files', 'Creature',
                      'kyrianmaw')
TARGET = os.path.join(REPO, 'modules', 'mod-stat-growth', 'client-assets', 'files', 'Creature', 'ValerianAscended')
NAME = 'ValerianAscended'
ARCHIVE_FOLDER = 'Creature\\ValerianAscended'


def load(module, path):
    spec = importlib.util.spec_from_file_location(module, os.path.join(REPO, path))
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


BLP = load('blp', 'localTools/interface/spike/blp.py')
BLP_WRITER = load('blp_writer', 'localTools/interface/buildParagonArt.py')
M2 = load('m2', 'localTools/oathblade/m2.py')

IVORY = numpy.array([0.95, 0.92, 0.84])     # the plates
GOLD = numpy.array([1.00, 0.80, 0.38])      # their rims, the light they catch
CLOTH = numpy.array([0.86, 0.62, 0.22])     # the cloth: deep gold
SKIN = numpy.array([0.98, 0.86, 0.72])      # what shows of him: warm, lit
LIGHT_CORE = numpy.array([1.00, 0.97, 0.88])    # the wings and tendrils: white at their heart...
LIGHT_EDGE = numpy.array([1.00, 0.74, 0.30])    # ... gold toward their ends

# The model's materials (flags, blend): 6 the feathered wings (alpha key), 7 and 8 the shroud's strips, 4 and 5 the
# Maw's mist round him (alpha blend, two-sided, no depth write) - unlit (flag 1), two-sided (4), no depth write (16);
# the shroud and the mist additive (blend 4), the feathered wings alpha blended (2): made of many feather cards one
# over the other, added they summed to a solid white
MATERIAL_WINGS = 6
MATERIALS_SHROUD = (7, 8)
MATERIALS_MIST = (4, 5)
BLEND_ADD = 4
BLEND_ALPHA = 2
FLAG_UNLIT = 0x1
FLAG_TWO_SIDED = 0x4
FLAG_NO_DEPTH_WRITE = 0x10
# The shroud's fixed textures (the model's texture records 9 and 10): its strips, its scrolling pattern; the mist's
SHROUD_STRIPS = 'kyrianmaw_3062158'
SHROUD_FLOW = 'kyrianmaw_3062160'
MISTS = ('kyrianmaw_3152595', 'kyrianmaw_3152597')


def smoothstep(low, high, x):
    t = numpy.clip((x - low) / (high - low), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def blurred(mask, radius):
    """a 0-1 mask (H x W x 1) softened, so a noisy hue in the dark makes no speckles"""
    picture = Image.fromarray(numpy.round(mask[..., 0] * 255).astype(numpy.uint8), 'L')
    return numpy.asarray(picture.filter(ImageFilter.GaussianBlur(radius))).astype(numpy.float32)[..., None] / 255.0


def channels(image):
    rgba = numpy.asarray(image.convert('RGBA')).astype(numpy.float32) / 255.0
    return rgba[..., :3], rgba[..., 3:]


def to_image(rgb, alpha):
    out = numpy.concatenate([numpy.clip(rgb, 0.0, 1.0), alpha], axis=-1)
    return Image.fromarray(numpy.round(out * 255).astype(numpy.uint8), 'RGBA')


def armour(image):
    """steel to ivory and gold, red cloth to gold, skin kept warm"""
    rgb, alpha = channels(image)
    luminance = (rgb @ numpy.array([0.299, 0.587, 0.114]))[..., None]
    hsv = numpy.asarray(image.convert('RGB').convert('HSV')).astype(numpy.float32) / 255.0
    hue, saturation = hsv[..., 0:1], hsv[..., 1:2]
    # Relative to the texture's own range: the steel is dark
    low, high = numpy.percentile(luminance, 5), numpy.percentile(luminance, 98)
    level = numpy.clip((luminance - low) / max(high - low, 1e-3), 0.0, 1.0)
    shade = 0.45 + 0.55 * level ** 0.75
    # Gold where the steel is brighter than round it: its rims, ridges and trims (not its broad lit faces)
    rims = numpy.clip((level - blurred(level, 5)) * 5.0, 0.0, 1.0)
    gold = smoothstep(0.12, 0.45, rims)
    metal = (IVORY * (1.0 - gold) + GOLD * gold) * shade
    # The cloth: a red-brown clearly saturated and not lost in the dark; what shows of skin: an orange-tan
    lit = smoothstep(0.05, 0.12, luminance)
    reddish = (numpy.minimum(hue, 1.0 - hue) < 0.06).astype(numpy.float32)
    cloth_share = blurred(reddish * smoothstep(0.30, 0.45, saturation) * lit, 2)
    tan = ((hue > 0.04) & (hue < 0.11)).astype(numpy.float32)
    skin_share = blurred(tan * smoothstep(0.35, 0.5, saturation) * lit, 2) * (1.0 - cloth_share)
    cloth = CLOTH * (0.35 + 0.75 * level)
    skin = SKIN * (0.45 + 0.6 * level)
    return to_image(metal * (1.0 - cloth_share - skin_share) + cloth * cloth_share + skin * skin_share, alpha)


def core(alpha, radius, sharpness):
    """the middle of each cut-out shape (a feather, a strip): where the shape is widest from its edges, 0 at them"""
    return blurred(alpha, radius) ** sharpness * alpha


def wing_surface(submeshes, size):
    """where each texel of a texture lies on the wings: the submeshes' triangles drawn in texture space at size
    (width, height), each texel given its rest-pose position (|y| out from the spine, z up: the wings spread in that
    plane behind him; left and right share their texels) - (out, up, covered) arrays, height x width"""
    model = open(os.path.join(SOURCE, 'kyrianmaw.m2'), 'rb').read()
    skin = open(os.path.join(SOURCE, 'kyrianmaw00.skin'), 'rb').read()
    count, offset = struct.unpack_from('<II', model, 0x3C)
    vertices = numpy.frombuffer(model, numpy.uint8, count * 48, offset).reshape(count, 48)
    positions = vertices[:, 0:12].copy().view(numpy.float32).reshape(count, 3)
    coords = vertices[:, 32:40].copy().view(numpy.float32).reshape(count, 2)
    count, offset = struct.unpack_from('<II', skin, 0x04)
    lookup = numpy.frombuffer(skin, numpy.uint16, count, offset)
    count, offset = struct.unpack_from('<II', skin, 0x0C)
    indices = numpy.frombuffer(skin, numpy.uint16, count, offset)
    _, submesh_offset = struct.unpack_from('<II', skin, 0x1C)
    width, height = size
    out = numpy.zeros((height, width), numpy.float32)
    up = numpy.zeros((height, width), numpy.float32)
    covered = numpy.zeros((height, width), bool)
    for submesh in submeshes:
        _, level, _, _, index_start, index_count = struct.unpack_from('<HHHHHH', skin, submesh_offset + 48 * submesh)
        index_start += level << 16
        for first in range(index_start, index_start + index_count, 3):
            corners = lookup[indices[first:first + 3]]
            uv = coords[corners].astype(numpy.float64)
            uv -= numpy.floor(uv.min(axis=0))
            pixel = uv * (width, height)
            where = positions[corners]
            x0, y0 = numpy.floor(pixel.min(axis=0)).astype(int)
            x1, y1 = numpy.ceil(pixel.max(axis=0)).astype(int)
            xs, ys = numpy.meshgrid(numpy.arange(x0, x1 + 1) + 0.5, numpy.arange(y0, y1 + 1) + 0.5)
            (ax, ay), (bx, by), (cx, cy) = pixel
            area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay)
            if abs(area) < 1e-9:
                continue
            w1 = ((xs - ax) * (cy - ay) - (cx - ax) * (ys - ay)) / area
            w2 = ((bx - ax) * (ys - ay) - (xs - ax) * (by - ay)) / area
            w0 = 1.0 - w1 - w2
            inside = (w0 >= -0.02) & (w1 >= -0.02) & (w2 >= -0.02)
            if not inside.any():
                continue
            px = (xs[inside] - 0.5).astype(int) % width
            py = (ys[inside] - 0.5).astype(int) % height
            weights = numpy.stack([w0[inside], w1[inside], w2[inside]], axis=1)
            point = weights @ where
            out[py, px] = numpy.abs(point[:, 1])
            up[py, px] = point[:, 2]
            covered[py, px] = True
    return out, up, covered


def blades(surface, root, count, core_width, glow_width, wave):
    """Inarius's wings: blades of light fanned from the shoulder (root: out, up) - count of them a side, each a bright
    thin core (core_width yards) in a gold glow (glow_width), waving a little along its length, fading at its tip"""
    out, up, covered = surface
    dx, dz = out - root[0], up - root[1]
    radius = numpy.hypot(dx, dz)
    angle = numpy.arctan2(dz, dx)
    spread = angle[covered]
    low, high = numpy.percentile(spread, 2), numpy.percentile(spread, 98)
    reach = numpy.percentile(radius[covered], 99)
    step = (high - low) / count
    centres = low + step * (numpy.arange(count) + 0.5)
    best = numpy.full(out.shape, numpy.inf, numpy.float32)
    for index, centre in enumerate(centres):
        waving = angle + wave * numpy.sin(2.3 * radius + 1.7 * index) / numpy.maximum(radius, 0.5)
        best = numpy.minimum(best, numpy.abs(waving - centre) * radius)
    along = radius / reach
    fade = (1.0 - smoothstep(0.72, 1.0, along)) * smoothstep(0.02, 0.15, along)
    heart = numpy.exp(-(best / core_width) ** 2)
    glow = numpy.exp(-(best / glow_width) ** 2)
    strength = numpy.clip(heart + 0.35 * glow, 0.0, 1.0) * fade * covered
    share = (heart / numpy.maximum(heart + 0.35 * glow, 1e-4))[..., None]
    colour = LIGHT_CORE[None, None, :] * share + LIGHT_EDGE[None, None, :] * (1.0 - share)
    return to_image(colour, strength[..., None])


def wings(size):
    """the feathered wings as blades of light (submeshes 2 and 3 of its skin, the feather band of the wings texture)"""
    return blades(wing_surface((2, 3), size), (0.25, 3.15), 7, 0.06, 0.32, 0.08)


def strips(image):
    """the Maw's great wings (submeshes 4 and 5, folded by its animations into the shroud on its back: six strips
    hanging from the shoulders, the texture's columns) as tendrils of light: a thin bright core down each strip, white
    at the shoulders (the texture's top) to gold at the tips, in a faint glow, the tips fading out"""
    _, alpha = channels(image)
    down = numpy.linspace(0.0, 1.0, alpha.shape[0])[:, None, None]
    colour = (LIGHT_CORE * (1.0 - down) + LIGHT_EDGE * down) * (0.5 * core(alpha, 10, 12.0) + 0.05 * alpha)
    return to_image(colour * (1.0 - 0.6 * down), alpha)


def flow(image):
    """the pattern scrolling down the strips: golden energy, faint (it adds onto the strips)"""
    rgb, alpha = channels(image)
    strength = numpy.maximum(alpha, (rgb @ numpy.array([0.299, 0.587, 0.114]))[..., None] * alpha)
    return to_image(LIGHT_EDGE * strength * 0.45, strength)


def mist(image):
    """the Maw's dark mist round him as a faint golden haze"""
    rgb, alpha = channels(image)
    luminance = (rgb @ numpy.array([0.299, 0.587, 0.114]))[..., None]
    low, high = numpy.percentile(luminance, 2), numpy.percentile(luminance, 99)
    level = numpy.clip((luminance - low) / max(high - low, 1e-3), 0.0, 1.0)
    return to_image(LIGHT_EDGE * level * 0.25, alpha)


def write_texture(image, name):
    path = os.path.join(TARGET, name + '.blp')
    if image.width * image.height <= 256 * 256:
        BLP_WRITER.writeRawBlp(image, path)
    else:
        BLP_WRITER.writeDxt3Blp(image, path)


def read_texture(name):
    return BLP.read_blp(os.path.join(SOURCE, name + '.blp'))


def build_model():
    model = M2.M2(open(os.path.join(SOURCE, 'kyrianmaw.m2'), 'rb').read())
    data = model.data
    count, offset = struct.unpack_from('<II', data, 0x70)
    for index in (MATERIAL_WINGS, *MATERIALS_SHROUD, *MATERIALS_MIST):
        flags, _ = struct.unpack_from('<HH', data, offset + 4 * index)
        flags = (flags | FLAG_UNLIT | FLAG_TWO_SIDED | FLAG_NO_DEPTH_WRITE)
        struct.pack_into('<HH', data, offset + 4 * index, flags, BLEND_ALPHA if index == MATERIAL_WINGS else BLEND_ADD)
    renamed = 0
    for record, kind, name in model.textures():
        stem = os.path.splitext(os.path.basename(name))[0].lower() if name else ''
        if kind == 0 and stem in (SHROUD_STRIPS, SHROUD_FLOW, *MISTS):
            model.rename_texture(record, f'{ARCHIVE_FOLDER}\\{NAME}_{stem.split("_")[-1]}.blp')
            renamed += 1
    if renamed != 4:
        raise SystemExit(f'expected the shroud\'s and the mist\'s four textures, renamed {renamed}')
    with open(os.path.join(TARGET, NAME + '.m2'), 'wb') as file:
        file.write(bytes(data))
    # Its skins and animation files under the new name (the client finds them by the model's)
    for file_name in os.listdir(SOURCE):
        if file_name.lower().startswith('kyrianmaw') and file_name.lower().endswith(('.skin', '.anim')):
            shutil.copyfile(os.path.join(SOURCE, file_name), os.path.join(TARGET, NAME + file_name[len('kyrianmaw'):]))


def main():
    shutil.rmtree(TARGET, ignore_errors=True)
    os.makedirs(TARGET)
    build_model()
    write_texture(armour(read_texture('kyrianmaw_body_black')), f'{NAME}_Body')
    write_texture(armour(read_texture('kyrianmaw_armor_black')), f'{NAME}_Armor')
    write_texture(wings(read_texture('kyrianmaw_wings_black').size), f'{NAME}_Wings')
    write_texture(strips(read_texture(SHROUD_STRIPS)), f'{NAME}_3062158')
    write_texture(flow(read_texture(SHROUD_FLOW)), f'{NAME}_3062160')
    for name in MISTS:
        write_texture(mist(read_texture(name)), f'{NAME}_{name.split("_")[-1]}')
    # Its other fixed textures stay the import's (Creature\kyrianmaw, where the model names them)
    print('built:', TARGET)


if __name__ == '__main__':
    main()
