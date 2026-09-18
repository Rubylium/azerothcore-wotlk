"""Paints the custom classes' icons into Details' class icon sheets.

Details draws every class icon from one 128x128 sheet (a 4x4 grid of 32 px cells) and a class's cell from
its own class_coords table. It has no cell for a custom class, so its bars fall back to the "unknown" icon.
Two cells of row 3 are empty in every sheet: each custom class claims one through `detailsCell` in
localTools/customClasses/classes.json, and DetailsCustomClasses points class_coords at it.

Details reads four variants of the sheet: plain, alpha (soft cut-out corners), and a grey version of each.
Only the class's own cell is painted, so running this again over an already painted sheet is harmless.

Writes the sheets to clientPatcher/addons/Details/images (shipped by the patcher, which copies files one by
one and never replaces the Details folder) and installs them into the local client.

Usage: python buildDetailsClassIcons.py [--client <WotLK client path>]
"""
import argparse
import json
import os
import shutil

from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFINITIONS = os.path.join(REPO, 'localTools', 'customClasses', 'classes.json')
OUTPUT = os.path.join(REPO, 'clientPatcher', 'addons', 'Details', 'images')
CELL = 32
# The cells a custom class may take: the two empty ones of row 3. Every other cell is a stock class icon.
FREE_CELLS = {(2, 2), (3, 2)}
# The alpha sheets cut every icon to the same soft-cornered shape: borrow it from the warrior cell
MASK_CELL = (0, 0)
SHEETS = {
    'classes_small.tga': {'alpha': False, 'grey': False},
    'classes_small_bw.tga': {'alpha': False, 'grey': True},
    'classes_small_alpha.tga': {'alpha': True, 'grey': False},
    'classes_small_alpha_bw.tga': {'alpha': True, 'grey': True},
}


def cell_box(cell):
    column, row = cell
    return (column * CELL, row * CELL, (column + 1) * CELL, (row + 1) * CELL)


def paint(sheet, icon, cell, alpha, grey):
    tile = icon.convert('RGBA').resize((CELL, CELL), Image.LANCZOS)
    if grey:
        # Details greys out classes that are not in the group: keep the alpha, drop the colour
        luminance = tile.convert('L')
        tile = Image.merge('RGBA', (luminance, luminance, luminance, tile.getchannel('A')))

    if alpha:
        tile.putalpha(sheet.crop(cell_box(MASK_CELL)).getchannel('A'))
    else:
        # The plain sheets are opaque, drawn over black
        backdrop = Image.new('RGBA', (CELL, CELL), (0, 0, 0, 255))
        backdrop.alpha_composite(tile)
        tile = backdrop

    sheet.paste(tile, cell_box(cell)[:2])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--client', default='C:\\Users\\alexi\\Documents\\GitHub\\CleanWOTLK')
    args = parser.parse_args()

    source_dir = os.path.join(args.client, 'Interface', 'AddOns', 'Details', 'images')
    if not os.path.isdir(source_dir):
        print(f'Details is not installed in {args.client}: nothing to paint.')
        return 0

    definitions = json.load(open(DEFINITIONS, encoding='utf8'))['classes']
    painted = [d for d in definitions if d.get('detailsCell') and d.get('classIcon')]
    cells = [tuple(d['detailsCell']) for d in painted]
    assert all(cell in FREE_CELLS for cell in cells), f'detailsCell must be one of {sorted(FREE_CELLS)}'
    assert len(set(cells)) == len(cells), 'two classes claim the same Details cell'

    os.makedirs(OUTPUT, exist_ok=True)
    for name, style in SHEETS.items():
        # Always start from the installed sheet: if Details ships new art, it is kept and only our cell changes
        own = os.path.join(OUTPUT, name)
        sheet = Image.open(os.path.join(source_dir, name)).convert('RGBA')
        for definition in painted:
            icon = Image.open(os.path.join(REPO, definition['classIcon']))
            paint(sheet, icon, tuple(definition['detailsCell']), style['alpha'], style['grey'])
        sheet.save(own, compression=None)
        shutil.copyfile(own, os.path.join(source_dir, name))

    names = ', '.join(f"{d['name']} {d['detailsCell']}" for d in painted)
    print(f'Details class icons painted: {names}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
