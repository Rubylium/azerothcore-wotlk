"""Compiles the Gladiateur's icons (localTools/warrior/Spells.ps1 and talentTree.json name them Gladiator_*) for the
3.3.5 client: each painted source in modules/mod-warrior/client-assets/source/icons/<name>.png, shrunk to a 64x64
32-bit TGA (top-down BGRA, as localTools/buildNecromancerClientAssets.ps1 writes them) at
modules/mod-warrior/client-assets/files/Interface/Icons/<name>.tga, which patch-Z ships at that path
(localTools/mpq-builder/patchFiles.js). patchSinisterStrike.ps1 gives a spell its icon once compiled (a stock icon
until then), and buildTalentTreeArt.py cuts the talent nodes from them.

Usage: python localTools/warrior/buildGladiatorIcons.py   (lists the sources still missing)
"""
import os
import re
import struct

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SOURCE = os.path.join(REPO, 'modules', 'mod-warrior', 'client-assets', 'source', 'icons')
OUTPUT = os.path.join(REPO, 'modules', 'mod-warrior', 'client-assets', 'files', 'Interface', 'Icons')
SIZE = 64


def wanted():
    """Every Gladiator_* icon the spell data and the talent tree name."""
    names = set()
    for file in ('Spells.ps1', 'talentTree.json'):
        text = open(os.path.join(HERE, file), encoding='utf-8').read()
        names.update(re.findall(r'\bGladiator_[A-Za-z]+\b', text))
    return sorted(names)


def write_tga(image, path):
    image = image.convert('RGBA').resize((SIZE, SIZE), Image.Resampling.LANCZOS)
    red, green, blue, alpha = image.split()
    pixels = Image.merge('RGBA', (blue, green, red, alpha)).tobytes()
    # Uncompressed true colour, 32 bits, 8 alpha bits, top-left origin
    header = struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, SIZE, SIZE, 32, 0x28)
    with open(path, 'wb') as out:
        out.write(header)
        out.write(pixels)


def main():
    os.makedirs(OUTPUT, exist_ok=True)
    built, missing = 0, []
    for name in wanted():
        source = os.path.join(SOURCE, name + '.png')
        if not os.path.exists(source):
            missing.append(name)
            continue
        write_tga(Image.open(source), os.path.join(OUTPUT, name + '.tga'))
        built += 1
    print('built %d Gladiateur icons' % built)
    if missing:
        print('missing sources (%s): %s' % (SOURCE, ', '.join(missing)))


if __name__ == '__main__':
    main()
