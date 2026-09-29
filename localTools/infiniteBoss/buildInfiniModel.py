"""Builds L'Infini's model: Algalon the Observer recoloured for the Celestial Planetarium (deep blue, starry white,
gold), for the Défi board's god fight (modules/mod-stat-growth/src/InfiniteGod.cpp).

    python buildInfiniModel.py [--preview DIR]

Algalon's model names its textures itself (no display texture to swap), so the god gets its own copy of it, at its own
path, under modules/mod-stat-growth/client-assets/compiled/infinite-boss (shipped in patch-Z at the same relative
path, localTools/mpq-builder/patchFiles.js):

- Creature\\Evolutions\\Infini\\Infini.m2, its skins and its animation files: the stock files copied as they are,
  but for the model's textures (pointed at the recoloured ones) and colours (its glow and its sparkles turned gold)
- Infini_Body.blp: Algalon's pale nebula skin (ALGALONTHEOBSERVER_01) as a deep blue night with white stars
- Infini_Glow.blp: the glows and lightning of ALGALONTHEOBSERVER_03 turned gold

The stock files are only read (the clean client, extractClientFiles.js): Algalon himself keeps his own look. The
display using the copy (CreatureModelData and CreatureDisplayInfo rows, id 60001) is added by
localTools/patchSinisterStrike.ps1. --preview renders the stock model and the copy side by side (the retail import's
software rasterizer) to check the textures by eye.
"""
import argparse
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile

import numpy
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
OUTPUT_ROOT = os.path.join(REPO_ROOT, 'modules', 'mod-stat-growth', 'client-assets', 'compiled', 'infinite-boss')
EXTRACTOR = os.path.join(REPO_ROOT, 'localTools', 'mpq-builder', 'extractClientFiles.js')

STOCK_FOLDER = 'Creature\\AlglontheObserver'
STOCK_NAME = 'AlgalontheObserver'
STOCK_BODY = 'CREATURE\\ALGLONTHEOBSERVER\\ALGALONTHEOBSERVER_01.BLP'
STOCK_GLOW = 'CREATURE\\ALGLONTHEOBSERVER\\ALGALONTHEOBSERVER_03.BLP'
FOLDER = 'Creature\\Evolutions\\Infini'
NAME = 'Infini'
BODY = FOLDER + '\\Infini_Body.blp'
GLOW = FOLDER + '\\Infini_Glow.blp'

# The model's other textures, kept stock: only read for the preview
PREVIEW_ONLY = ['WORLD\\EXPANSION02\\DOODADS\\ULDUAR\\UL_STATUEBASE_01_REF.BLP', 'CREATURE\\WISP\\STAR2_32.BLP']

# The body: the nebula's brightness, stretched over its own range, runs from a deep night blue through a royal blue
# to a starry white; its brightest specks (the stock stars) turn gold
NIGHT = numpy.array([6, 14, 52], dtype=numpy.float32)
ROYAL = numpy.array([46, 100, 222], dtype=numpy.float32)
STAR = numpy.array([236, 242, 255], dtype=numpy.float32)
GOLD = numpy.array([255, 204, 102], dtype=numpy.float32)
BODY_CURVE = 1.25           # a little more of the skin in the dark blues
STAR_SHARE = 0.965          # the brightest texels past this share of the range are stars
# The glow sprites: their brightness in gold, the hottest part white
GLOW_TINT = numpy.array([1.0, 0.8, 0.42], dtype=numpy.float32)
# The model's own colours: its glow ball's sky blue turned gold, its sparkles a warm white
GLOW_COLOUR = (1.0, 0.78, 0.35)
SPARKLE_COLOUR = (255.0, 232.0, 176.0)

sys.path.insert(0, os.path.join(REPO_ROOT, 'localTools', 'oathblade'))
from m2 import M2  # noqa: E402


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


blp_writer = load_module('buildParagonArt', os.path.join(REPO_ROOT, 'localTools', 'interface', 'buildParagonArt.py'))


def extract(files, directory):
    list_path = os.path.join(directory, 'list.json')
    with open(list_path, 'w', encoding='utf-8') as output:
        json.dump(files, output)
    subprocess.run(['node', EXTRACTOR, list_path, directory], check=True, stdout=subprocess.DEVNULL)
    with open(os.path.join(directory, 'missing.json'), encoding='utf-8') as missing:
        absent = json.load(missing)
    return {name: os.path.join(directory, *name.split('\\')) for name in files if name not in absent}


def luminance(rgb):
    return rgb[..., 0] * 0.299 + rgb[..., 1] * 0.587 + rgb[..., 2] * 0.114


def body_texture(image):
    rgba = numpy.asarray(image.convert('RGBA'), dtype=numpy.float32)
    light = luminance(rgba[..., :3])
    low, high = numpy.percentile(light, 1), numpy.percentile(light, 99.8)
    t = numpy.clip((light - low) / max(high - low, 1.0), 0.0, 1.0)
    shade = t ** BODY_CURVE
    lower = numpy.clip(shade / 0.6, 0.0, 1.0)[..., None]
    upper = numpy.clip((shade - 0.6) / 0.4, 0.0, 1.0)[..., None]
    colour = NIGHT + (ROYAL - NIGHT) * lower
    colour = colour + (STAR - colour) * upper
    stars = numpy.clip((t - STAR_SHARE) / (1.0 - STAR_SHARE), 0.0, 1.0)[..., None]
    colour = colour + (GOLD - colour) * stars
    out = numpy.concatenate([colour, rgba[..., 3:4]], axis=-1)
    return Image.fromarray(numpy.clip(out, 0, 255).astype(numpy.uint8), 'RGBA')


def glow_texture(image):
    rgba = numpy.asarray(image.convert('RGBA'), dtype=numpy.float32)
    light = luminance(rgba[..., :3]) / 255.0
    hot = (light ** 4)[..., None]
    colour = 255.0 * light[..., None] * GLOW_TINT
    colour = colour + (255.0 * light[..., None] - colour) * hot
    out = numpy.concatenate([colour, rgba[..., 3:4]], axis=-1)
    return Image.fromarray(numpy.clip(out, 0, 255).astype(numpy.uint8), 'RGBA')


def recolour_model(model):
    for record, _, name in model.textures():
        upper = name.upper()
        if upper == STOCK_BODY:
            model.rename_texture(record, BODY)
        elif upper == STOCK_GLOW:
            model.rename_texture(record, GLOW)
    for offset, scale in model.colour_slots():
        model.set_colour(offset, GLOW_COLOUR if scale == 1.0 else SPARKLE_COLOUR)


def write(relative, data):
    path = os.path.join(OUTPUT_ROOT, *relative.split('\\'))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as output:
        output.write(data)
    return path


def preview(directory, stock_m2, copy_m2, texture_root):
    viewer = load_module('preview', os.path.join(REPO_ROOT, 'localTools', 'retailImport', 'preview.py'))
    viewer.SIZE = 480
    panels = []
    for m2 in (stock_m2, copy_m2):
        positions, normals, uvs, batches = viewer.load_model(m2, m2[:-3] + '00.skin',
                                                             viewer.texture_loader(m2, None, texture_root))
        drawn = positions[numpy.unique(numpy.concatenate([batch[0].ravel() for batch in batches]))]
        center = (drawn.max(axis=0) + drawn.min(axis=0)) / 2
        scale = 0.9 * viewer.SIZE / numpy.linalg.norm(drawn.max(axis=0) - drawn.min(axis=0))
        for yaw in (0.0, 1.6):
            panels.append(viewer.render(positions, normals, uvs, batches, viewer.rotation_for(drawn, yaw, 0.2),
                                        scale, center))
    sheet = Image.new('RGB', (viewer.SIZE * 2, viewer.SIZE * 2))
    for index, panel in enumerate(panels):
        sheet.paste(panel, ((index % 2) * viewer.SIZE, (index // 2) * viewer.SIZE))
    os.makedirs(directory, exist_ok=True)
    out = os.path.join(directory, 'Infini.png')
    sheet.save(out)
    print(f'{out}: top row Algalon, bottom row L\'Infini')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--preview', help='render the stock model and the copy into this folder')
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as work:
        stock_m2 = f'{STOCK_FOLDER}\\{STOCK_NAME}.m2'
        found = extract([stock_m2, STOCK_BODY, STOCK_GLOW, *PREVIEW_ONLY], work)
        if stock_m2 not in found:
            raise SystemExit(f'{stock_m2} is not in the client')
        model = M2(open(found[stock_m2], 'rb').read())
        companions = [f'{STOCK_FOLDER}\\{STOCK_NAME}{index:02d}.skin' for index in range(model.skin_count)]
        companions += [f'{STOCK_FOLDER}\\{STOCK_NAME}{animation:04d}-{sub:02d}.anim'
                       for animation, sub in model.external_sequences()]
        extracted = extract(companions, work)
        missing = [name for name in companions if name not in extracted]
        # Only a skin is required; an animation a model lists may not exist (the client plays none for it)
        if any(name.endswith('.skin') for name in missing):
            raise SystemExit(f'missing skins: {missing}')

        shutil.rmtree(OUTPUT_ROOT, ignore_errors=True)
        recolour_model(model)
        written = [write(f'{FOLDER}\\{NAME}.m2', bytes(model.data))]
        for name, path in extracted.items():
            suffix = name[len(f'{STOCK_FOLDER}\\{STOCK_NAME}'):]
            written.append(write(f'{FOLDER}\\{NAME}{suffix}', open(path, 'rb').read()))

        body = body_texture(Image.open(found[STOCK_BODY]))
        glow = glow_texture(Image.open(found[STOCK_GLOW]))
        for relative, image in ((BODY, body), (GLOW, glow)):
            path = os.path.join(OUTPUT_ROOT, *relative.split('\\'))
            blp_writer.writeRawBlp(image, path)
            written.append(path)
        print(f'{len(written)} files, {len(missing)} animation files the model lists but the client lacks')

        if args.preview:
            # The renderer reads textures by file name with PIL, which cannot read raw BLPs: PNG copies under the
            # same names, and the stock textures beside them
            textures = os.path.join(work, 'textures')
            os.makedirs(textures, exist_ok=True)
            body.save(os.path.join(textures, 'Infini_Body.blp'), 'PNG')
            glow.save(os.path.join(textures, 'Infini_Glow.blp'), 'PNG')
            for name in (STOCK_BODY, STOCK_GLOW, *PREVIEW_ONLY):
                Image.open(found[name]).save(os.path.join(textures, os.path.basename(name.replace('\\', '/'))),
                                             'PNG')
            stock_copy = os.path.join(work, 'stock.m2')
            shutil.copy(found[stock_m2], stock_copy)
            shutil.copy(extracted[f'{STOCK_FOLDER}\\{STOCK_NAME}00.skin'], os.path.join(work, 'stock00.skin'))
            copy_m2 = os.path.join(OUTPUT_ROOT, *FOLDER.split('\\'), f'{NAME}.m2')
            preview(args.preview, stock_copy, copy_m2, textures)


if __name__ == '__main__':
    sys.exit(main())
