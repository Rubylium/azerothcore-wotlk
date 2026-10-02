"""Paints the custom classes' icons into Details' class icon sheets.

Details draws every class icon from one sheet (a 4x4 grid) and a class's cell from its own class_coords table, as
fractions of the sheet. It has no cell for a custom class, so its bars fall back to the "unknown" icon. The stock
sheet is 128x128 with three free cells; the custom classes outgrew them, so the sheets are drawn at 256x256 (64 px
cells): every fraction Details holds stays right, and a free cell holds either one class at 64 px or four at 32 px
(the stock sheet's own size), one per quadrant. A class claims `detailsCell` [column, row] or [column, row,
quadrant] (0 top left, 1 top right, 2 bottom left, 3 bottom right) in localTools/customClasses/classes.json, and
DetailsCustomClasses points class_coords at it.

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
# The sheets' size: twice the stock 128, each cell 64 px
SIZE = 256
CELL = SIZE // 4
# The cells a custom class may take, whole or in quadrants. Every other cell is a stock class icon.
FREE_CELLS = {(2, 2), (3, 2), (0, 3)}
# The alpha sheets cut every icon to the same soft-cornered shape: borrow it from the warrior cell
MASK_CELL = (0, 0)
SHEETS = {
    'classes_small.tga': {'alpha': False, 'grey': False},
    'classes_small_bw.tga': {'alpha': False, 'grey': True},
    'classes_small_alpha.tga': {'alpha': True, 'grey': False},
    'classes_small_alpha_bw.tga': {'alpha': True, 'grey': True},
}


def cell_box(cell):
    """a class's box: a whole cell, or a quadrant of one"""
    column, row = cell[0], cell[1]
    left, top = column * CELL, row * CELL
    if len(cell) == 2:
        return (left, top, left + CELL, top + CELL)
    half = CELL // 2
    left += (cell[2] % 2) * half
    top += (cell[2] // 2) * half
    return (left, top, left + half, top + half)


def paint(sheet, icon, cell, alpha, grey):
    box = cell_box(cell)
    size = box[2] - box[0]
    tile = icon.convert('RGBA').resize((size, size), Image.LANCZOS)
    if grey:
        # Details greys out classes that are not in the group: keep the alpha, drop the colour
        luminance = tile.convert('L')
        tile = Image.merge('RGBA', (luminance, luminance, luminance, tile.getchannel('A')))

    if alpha:
        tile.putalpha(sheet.crop(cell_box(MASK_CELL)).getchannel('A').resize((size, size), Image.LANCZOS))
    else:
        # The plain sheets are opaque, drawn over black
        backdrop = Image.new('RGBA', (size, size), (0, 0, 0, 255))
        backdrop.alpha_composite(tile)
        tile = backdrop

    sheet.paste(tile, box[:2])


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
    assert all(cell[:2] in FREE_CELLS and (len(cell) == 2 or cell[2] in range(4)) for cell in cells), \
        f'detailsCell must be one of {sorted(FREE_CELLS)}, optionally with a quadrant 0-3'
    assert len(set(cells)) == len(cells), 'two classes claim the same Details cell'
    whole = {cell for cell in cells if len(cell) == 2}
    assert not any(cell[:2] in whole for cell in cells if len(cell) == 3), 'a whole cell also holds quadrants'
    # A cell split in quadrants is cleared first: a quadrant no class claims stays empty
    split = {cell[:2] for cell in cells if len(cell) == 3}

    os.makedirs(OUTPUT, exist_ok=True)
    for name, style in SHEETS.items():
        # Always start from the installed sheet: if Details ships new art, it is kept and only our cell changes
        own = os.path.join(OUTPUT, name)
        sheet = Image.open(os.path.join(source_dir, name)).convert('RGBA')
        if sheet.size != (SIZE, SIZE):
            # The stock sheet: drawn again at twice its size, every fraction Details holds still right
            sheet = sheet.resize((SIZE, SIZE), Image.LANCZOS)
        for cell in split:
            fill = (0, 0, 0, 0) if style['alpha'] else (0, 0, 0, 255)
            sheet.paste(Image.new('RGBA', (CELL, CELL), fill), cell_box(cell)[:2])
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
