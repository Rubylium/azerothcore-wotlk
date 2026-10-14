"""The site's interface pieces (buttons, frames, dividers), cut from the retail client's UI atlases (run from the repo
root). Retail art is sharper than 3.3.5's: the 2x pieces are drawn for high-density screens.

    python website/tools/extractRetailUi.py [--retail "<retail WoW>/_retail_"] [--casclib <CascLib.dll>] [names...]

Reads the local retail install with CascLib (as localTools/interface/extractRetailUi.py does) and the atlas
tables cached in localTools/interface/cache/talents (UiTextureAtlas / UiTextureAtlasMember, from wago.tools).
Writes website/public/img/ui/<name>.png, cropped exactly to the atlas member: never scaled or stretched. The outputs
are committed, so the site never needs a retail client.
"""
import argparse
import csv
import io
import sys
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'localTools' / 'interface'))
from extractRetailUi import CascStorage, read_blp  # noqa: E402

ATLAS_CSV = REPO / 'localTools' / 'interface' / 'cache' / 'talents'
CACHE = REPO / 'localTools' / 'interface' / 'cache' / 'website'
OUT = REPO / 'website' / 'public' / 'img' / 'ui'

# What the site uses
PIECES = [
    # The big red button (Play, Create an account): a left cap and a long right part, joined into a 3-slice
    '128-redbutton-left', '128-redbutton-right', '128-redbutton-left-pressed', '128-redbutton-right-pressed',
    '128-redbutton-highlight',
    # The gold frame of the panels (Plunderstorm's nine-slice)
    'plunderstorm-nineslice-cornertopleft', 'plunderstorm-nineslice-cornertopright',
    'plunderstorm-nineslice-cornerbottomleft', 'plunderstorm-nineslice-cornerbottomright',
    '_plunderstorm-nineslice-edgetop', '_plunderstorm-nineslice-edgebottom',
    '!plunderstorm-nineslice-edgeleft', '!plunderstorm-nineslice-edgeright',
    # The wood and gold title plate of the section headings, and the wood behind the top bar
    'ui-frame-neutral-titleleft', '_ui-frame-neutral-titlemiddle', 'ui-frame-neutral-titleright',
    'UI-Frame-Neutral-BackgroundTile',
    # Ornaments
    'banner-bgfiligree', 'parchmentpopup-filigree', 'questlog_divider',
]


def load_atlases():
    atlases = {row['ID']: row for row in csv.DictReader(open(ATLAS_CSV / 'UiTextureAtlas.csv', encoding='utf-8'))}
    members = {}
    for row in csv.DictReader(open(ATLAS_CSV / 'UiTextureAtlasMember.csv', encoding='utf-8')):
        members[row['CommittedName'].lower()] = row
    return atlases, members


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--retail', default='C:/Program Files (x86)/World of Warcraft/_retail_')
    parser.add_argument('--casclib', default=str(REPO / 'data' / 'custom' / 'diabloLootSounds' / 'tools' / 'CascLib.dll'))
    parser.add_argument('--out', default=str(OUT))
    parser.add_argument('names', nargs='*')
    args = parser.parse_args()

    atlases, members = load_atlases()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    CACHE.mkdir(parents=True, exist_ok=True)
    storage = None
    sheets = {}
    for name in args.names or PIECES:
        member = members.get(name.lower())
        if not member:
            print(f'not in the atlas tables: {name}')
            continue
        atlas = atlases[member['UiTextureAtlasID']]
        file_data_id = int(atlas['FileDataID'])
        if file_data_id not in sheets:
            cached = CACHE / f'{file_data_id}.blp'
            if not cached.exists():
                storage = storage or CascStorage(args.casclib, args.retail)
                cached.write_bytes(storage.read(file_data_id))
            sheet = read_blp(str(cached))
            # Atlas coordinates are in the atlas's own size; a sheet stored bigger scales them
            sheets[file_data_id] = (sheet, sheet.width / int(atlas['AtlasWidth']), sheet.height / int(atlas['AtlasHeight']))
        sheet, sx, sy = sheets[file_data_id]
        box = (round(int(member['CommittedLeft']) * sx), round(int(member['CommittedTop']) * sy),
               round(int(member['CommittedRight']) * sx), round(int(member['CommittedBottom']) * sy))
        piece = sheet.crop(box)
        target = out / (name.lstrip('_!').lower() + '.png')
        piece.save(target, optimize=True)
        print(f'{piece.width:4}x{piece.height:<4} {target.name}')


if __name__ == '__main__':
    main()
