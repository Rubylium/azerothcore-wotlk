"""Builds the Oathblade's own blue copy of every spell effect model its visuals play.

    python buildBlueEffects.py <effects.json> <mapping.json>

effects.json is [{"id": <SpellVisualEffectName id>, "path": "Spells\\X.mdx"}, ...], written by
localTools/patchSinisterStrike.ps1 from every effect model the Oathblade kits (OB_*) use. For each one this writes,
under modules/mod-oathblade/client-assets/compiled/spells (shipped in patch-Z as Spells\\Oathblade\\...):

- OB_<name>.m2 and its OB_<name>NN.skin files: the stock model with every colour it animates (colours, lights,
  ribbons, particles) turned blue
- Textures\\OB_<texture>.blp: each texture the model names, its coloured pixels turned blue, the model pointed at it

and mapping.json, {"<id>": "Spells\\Oathblade\\OB_<name>.mdx"}, from which the patcher adds new effect records.
The stock files are only read: another class playing the same effect still sees it in its own colours.

Blue: every coloured value keeps its saturation, brightness and alpha, and takes a hue between ice blue (the
brightest, 195 degrees) and deep blue (the darkest, 220). Greys and whites are left alone; spell textures are mostly
white and take their colour from the model, so the two together read as one blue.
"""
import colorsys
import importlib.util
import json
import ntpath
import os
import shutil
import subprocess
import sys
import tempfile

import numpy
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from m2 import M2  # noqa: E402

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
OUTPUT_ROOT = os.path.join(REPO_ROOT, 'modules', 'mod-oathblade', 'client-assets', 'compiled', 'spells')
ARCHIVE_ROOT = 'Spells\\Oathblade'
EXTRACTOR = os.path.join(REPO_ROOT, 'localTools', 'mpq-builder', 'extractClientFiles.js')

# Below this saturation a value is a grey or a white, and stays one
MIN_SATURATION = 0.12
BRIGHT_HUE = 195 / 360
DARK_HUE = 220 / 360
# Textures up to this size ship uncompressed (spell textures are small and their soft alpha bands in DXT3)
RAW_MAX_PIXELS = 256 * 256


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


blp = load_module('blp', os.path.join(REPO_ROOT, 'localTools', 'interface', 'spike', 'blp.py'))
blp_writer = load_module('buildParagonArt', os.path.join(REPO_ROOT, 'localTools', 'interface', 'buildParagonArt.py'))


def blue_hue(value):
    return DARK_HUE + (BRIGHT_HUE - DARK_HUE) * value


def blue_colour(rgb):
    """an M2 colour (0-1, may exceed 1 to over-brighten) turned blue"""
    peak = max(rgb)
    if peak <= 0:
        return rgb
    scale = max(1.0, peak)
    hue, saturation, value = colorsys.rgb_to_hsv(*(max(0.0, c) / scale for c in rgb))
    if saturation < MIN_SATURATION:
        return rgb
    return tuple(c * scale for c in colorsys.hsv_to_rgb(blue_hue(value), saturation, value))


def blue_image(image):
    rgba = image.convert('RGBA')
    alpha = rgba.getchannel('A')
    hsv = numpy.asarray(rgba.convert('RGB').convert('HSV')).astype(numpy.float32) / 255
    coloured = hsv[..., 1] >= MIN_SATURATION
    hues = blue_hue(hsv[..., 2])
    hsv[..., 0] = numpy.where(coloured, hues, hsv[..., 0])
    result = Image.fromarray(numpy.round(hsv * 255).astype(numpy.uint8), 'HSV').convert('RGB')
    # Pixels left grey keep their exact stock values (HSV there is lossy)
    mask = Image.fromarray((coloured * 255).astype(numpy.uint8), 'L')
    result = Image.composite(result, rgba.convert('RGB'), mask)
    result.putalpha(alpha)
    return result


def extract(paths, directory):
    list_path = os.path.join(directory, 'list.json')
    with open(list_path, 'w', encoding='utf-8') as output:
        json.dump(sorted(set(paths)), output)
    subprocess.run(['node', EXTRACTOR, list_path, directory], check=True, stdout=subprocess.DEVNULL)
    with open(os.path.join(directory, 'missing.json'), encoding='utf-8') as missing:
        return set(json.load(missing))


def archive_path(name):
    """a path as the archives spell it: models name some of their textures with forward slashes"""
    return name.replace('/', '\\')


def local(directory, name):
    return os.path.join(directory, *name.split('\\'))


def write_output(name, data):
    target = os.path.join(OUTPUT_ROOT, *name[len(ARCHIVE_ROOT) + 1:].split('\\'))
    os.makedirs(os.path.dirname(target), exist_ok=True)
    with open(target, 'wb') as output:
        output.write(data)
    return target


def main(effects_path, mapping_path):
    with open(effects_path, encoding='utf-8-sig') as source:
        effects = json.load(source)
    # Rebuilt whole every time, so an effect no kit uses any more is not shipped
    shutil.rmtree(OUTPUT_ROOT, ignore_errors=True)
    stems = {effect['id']: ntpath.splitext(archive_path(effect['path']))[0] for effect in effects}

    with tempfile.TemporaryDirectory() as work:
        missing = extract([f'{stem}.m2' for stem in stems.values()], work)
        models = {}
        texture_paths = set()
        for effect_id, stem in stems.items():
            if f'{stem}.m2' in missing:
                raise SystemExit(f'effect {effect_id}: {stem}.m2 is not in the client')
            model = M2(open(local(work, f'{stem}.m2'), 'rb').read())
            if model.external_sequences():
                raise SystemExit(f'{stem}.m2 keeps animations in .anim files, which are not copied')
            for name in model.particle_models():
                print(f'  warning: {stem}.m2 particles draw {name}, which keeps its own colours')
            models[effect_id] = model
            texture_paths.update(archive_path(name) for _, kind, name in model.textures() if kind == 0 and name)

        # Its skins (one per level of detail, 00 the closest) and textures
        skins = {effect_id: [f'{stems[effect_id]}{index:02}.skin' for index in range(model.skin_count)]
                 for effect_id, model in models.items()}
        missing = extract(list(texture_paths) + [skin for names in skins.values() for skin in names], work)
        if missing:
            raise SystemExit(f'skins or textures not in the client: {sorted(missing)}')

        # One blue copy of each texture, however many models share it
        texture_names = {}
        for texture_path in sorted(texture_paths, key=str.lower):
            key = texture_path.lower()
            if key in texture_names:
                continue
            name = f'{ARCHIVE_ROOT}\\Textures\\OB_{ntpath.basename(texture_path)}'
            if name.lower() in (taken.lower() for taken in texture_names.values()):
                raise SystemExit(f'two textures are named {ntpath.basename(texture_path)}; give the copies folders')
            image = blue_image(blp.read_blp(local(work, texture_path)))
            target = write_output(name, b'')
            if image.width * image.height <= RAW_MAX_PIXELS:
                blp_writer.writeRawBlp(image, target)
            else:
                blp_writer.writeDxt3Blp(image, target)
            texture_names[key] = name

        mapping = {}
        for effect_id, model in models.items():
            stem = stems[effect_id]
            for offset, scale in model.colour_slots():
                rgb = tuple(c / scale for c in model.colour(offset))
                model.set_colour(offset, tuple(c * scale for c in blue_colour(rgb)))
            for record, kind, name in model.textures():
                if kind == 0 and name:
                    model.rename_texture(record, texture_names[archive_path(name).lower()])
            new_stem = f'{ARCHIVE_ROOT}\\OB_{ntpath.basename(stem)}'
            write_output(f'{new_stem}.m2', bytes(model.data))
            for skin_name in skins[effect_id]:
                with open(local(work, skin_name), 'rb') as skin:
                    write_output(new_stem + skin_name[len(stem):], skin.read())
            mapping[str(effect_id)] = f'{new_stem}.mdx'

    with open(mapping_path, 'w', encoding='utf-8') as output:
        json.dump(mapping, output, indent=2)
    print(f'Oathblade blue effects: {len(mapping)} models, {len(texture_names)} textures')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('usage: python buildBlueEffects.py <effects.json> <mapping.json>')
    main(sys.argv[1], sys.argv[2])
