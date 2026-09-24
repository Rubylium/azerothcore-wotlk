"""Builds the artwork of the Evolutions glue screens from the game's own Wrath of the Lich King loading screens.

    python buildGlueArt.py

Writes clientPatcher/interface/Interface/Glues/Evolutions/:
- RealmBackdrop.blp: behind the realm selection. Icecrown Citadel's gate (LoadScreenIcecrownCitadel), cut below the
  game logo and the parchment border, 1024x512 for a picture about 2.44 times as wide as it is tall (the loading
  screens are drawn 4:3 from a square texture)
- RealmCard.blp: a realm's card. Arthas and Frostmourne (LoadScreenNorthrendWide, a 16:9 picture in a square
  texture), the part left of the logo, 256x512 for a picture 0.64 times as wide as it is tall
- RealmCardGlow.blp: the light around the selected card (card_glow below)

GlueXML/RealmList.lua knows those proportions and sizes (BACKDROP_ASPECT, CARD_ART_ASPECT, CARD_WIDTH/HEIGHT,
CARD_GLOW_MARGIN); change them together.
"""
import importlib.util
import json
import os
import subprocess
import tempfile

from PIL import Image, ImageDraw, ImageFilter

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
OUTPUT = os.path.join(REPO_ROOT, 'clientPatcher', 'interface', 'Interface', 'Glues', 'Evolutions')
EXTRACTOR = os.path.join(REPO_ROOT, 'localTools', 'mpq-builder', 'extractClientFiles.js')

ICECROWN = 'Interface\\Glues\\LoadingScreens\\LoadScreenIcecrownCitadel.blp'
NORTHREND = 'Interface\\Glues\\LoadingScreens\\LoadScreenNorthrendWide.blp'

# In the square source textures (pixels): what each picture keeps
BACKDROP_CROP = (0, 322, 1024, 878)         # below the logo, above the parchment
CARD_CROP = (0, 0, 364, 1024)               # left of the logo: Arthas, head to knee


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


blp = load_module('blp', os.path.join(REPO_ROOT, 'localTools', 'interface', 'spike', 'blp.py'))
writer = load_module('buildParagonArt', os.path.join(REPO_ROOT, 'localTools', 'interface', 'buildParagonArt.py'))


def extract(names, directory):
    list_path = os.path.join(directory, 'list.json')
    with open(list_path, 'w', encoding='utf-8') as output:
        json.dump(names, output)
    subprocess.run(['node', EXTRACTOR, list_path, directory], check=True, stdout=subprocess.DEVNULL)
    return {name: os.path.join(directory, *name.split('\\')) for name in names}


def main():
    os.makedirs(OUTPUT, exist_ok=True)
    with tempfile.TemporaryDirectory() as work:
        files = extract([ICECROWN, NORTHREND], work)
        backdrop = blp.read_blp(files[ICECROWN]).convert('RGBA').crop(BACKDROP_CROP)
        writer.writeDxt3Blp(backdrop.resize((1024, 512), Image.Resampling.LANCZOS),
                            os.path.join(OUTPUT, 'RealmBackdrop.blp'))
        card = blp.read_blp(files[NORTHREND]).convert('RGBA').crop(CARD_CROP)
        writer.writeDxt3Blp(card.resize((256, 512), Image.Resampling.LANCZOS), os.path.join(OUTPUT, 'RealmCard.blp'))
    writer.writeRawBlp(card_glow(), os.path.join(OUTPUT, 'RealmCardGlow.blp'))
    print(f'Built the realm selection art in {OUTPUT}')


# The light around a selected realm card: white, its strength in alpha, tinted and faded from RealmList.lua. Drawn
# over the card and CARD_GLOW_MARGIN beyond it on every side, half strength at the card's edge, fading to nothing
# at the margin's outer edge, with rounded corners.
CARD_SIZE = (290, 430)            # RealmList.lua CARD_WIDTH, CARD_HEIGHT
CARD_GLOW_MARGIN = 28             # RealmList.lua CARD_GLOW_MARGIN


def card_glow():
    scale = 2
    width = (CARD_SIZE[0] + 2 * CARD_GLOW_MARGIN) * scale
    height = (CARD_SIZE[1] + 2 * CARD_GLOW_MARGIN) * scale
    mask = Image.new('L', (width, height), 0)
    ImageDraw.Draw(mask).rectangle((CARD_GLOW_MARGIN * scale, CARD_GLOW_MARGIN * scale,
                                    width - CARD_GLOW_MARGIN * scale - 1, height - CARD_GLOW_MARGIN * scale - 1),
                                   fill=255)
    # A blur of a third of the margin reaches nearly zero at its outer edge
    mask = mask.filter(ImageFilter.GaussianBlur(CARD_GLOW_MARGIN * scale / 3))
    glow = Image.new('RGBA', (width, height), (255, 255, 255, 0))
    glow.putalpha(mask)
    # A soft light has no detail to keep: small is enough, and uncompressed keeps its alpha free of banding
    return glow.resize((128, 256), Image.Resampling.LANCZOS)


if __name__ == '__main__':
    main()
