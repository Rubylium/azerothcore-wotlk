"""Builds the art of the Group Finder window (clientPatcher/interface/Interface/FrameXML/GroupFinder.lua).

The window is retail's PVEFrame (Mists of Pandaria's Group Finder, still retail's): its blue mode column and the
round mode buttons are Interface/Common/bluemenu-*, the ring bluemenuring. They are taken whole from the retail
client (TACTTool, by Marlamin, as extractRetailUi.py does) and written as TGA into
clientPatcher/interface/Interface/RetailUI. The files are cached under localTools/interface/cache/groupfinder
(gitignored), so only the first run needs retail.

Retail masks each mode's icon round (TempPortraitAlphaMask); 3.3.5 has no texture masks, so the icons are cut
round here with that same mask: the dungeon helmet, the Raid Finder portrait and the keystone hourglass.

Usage: python localTools/interface/buildGroupFinderArt.py [--tacttool <TACTTool.exe>] [--retail <retail WoW>]
"""
import argparse
import os
import subprocess
import sys

from PIL import Image, ImageChops

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from extractRetailUi import read_blp, write_tga  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CACHE = os.path.join(REPO, 'localTools', 'interface', 'cache', 'groupfinder')
OUTPUT = os.path.join(REPO, 'clientPatcher', 'interface', 'Interface', 'RetailUI')

# Whole files: FileDataID -> output name
FILES = {
    593918: 'bluemenu-main',
    593919: 'bluemenu-vert',
    593917: 'bluemenu-goldborder-horiz',
    922034: 'bluemenu-ring',
}
MASK = 130924  # interface/characterframe/tempportraitalphamask.blp
# The mode icons, cut round: FileDataID -> output name
ICONS = {
    133076: 'groupfinder-icon-dungeons',     # interface/icons/inv_helmet_08
    341547: 'groupfinder-icon-raids',        # interface/lfgframe/ui-lfr-portrait
    525134: 'groupfinder-icon-mythicplus',   # interface/icons/inv_relics_hourglass
}
ICON_SIZE = 64
# Retail insets the mask 2 px inside a 66 px icon
MASK_INSET = 2


def fetch(args, file_data_id):
    path = os.path.join(CACHE, '%d.blp' % file_data_id)
    if not os.path.exists(path):
        if not args.tacttool:
            raise SystemExit('Retail file %d is not cached: pass --tacttool' % file_data_id)
        os.makedirs(CACHE, exist_ok=True)
        subprocess.run([args.tacttool, '-r', args.region, '-m', 'fdid', '-i', str(file_data_id), '-d', args.retail,
                        '-o', path], check=True, capture_output=True)
    return read_blp(path).convert('RGBA')


def round_icon(icon, mask):
    icon = icon.resize((ICON_SIZE, ICON_SIZE), Image.LANCZOS)
    inner = ICON_SIZE - 2 * MASK_INSET
    # The mask's shape is in its alpha, or in its grey when it has none
    shape = mask.getchannel('A')
    if shape.getextrema()[0] == 255:
        shape = mask.convert('L')
    alpha = Image.new('L', (ICON_SIZE, ICON_SIZE), 0)
    alpha.paste(shape.resize((inner, inner), Image.LANCZOS), (MASK_INSET, MASK_INSET))
    red, green, blue, own = icon.split()
    return Image.merge('RGBA', (red, green, blue, ImageChops.darker(own, alpha)))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--tacttool')
    parser.add_argument('--retail', default='C:/Program Files (x86)/World of Warcraft')
    parser.add_argument('--region', default='eu')
    args = parser.parse_args()

    for file_data_id, name in FILES.items():
        write_tga(fetch(args, file_data_id), os.path.join(OUTPUT, name + '.tga'))
        print('wrote', name)

    mask = fetch(args, MASK)
    for file_data_id, name in ICONS.items():
        write_tga(round_icon(fetch(args, file_data_id), mask), os.path.join(OUTPUT, name + '.tga'))
        print('wrote', name)


if __name__ == '__main__':
    main()
