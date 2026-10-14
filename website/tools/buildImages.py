"""The site's pictures, made from the art the launcher and the client already use (run from the repo root).

    python website/tools/buildImages.py

Each picture keeps its own aspect: only resized and compressed, never stretched or cropped here (the page crops with
object-fit where a frame asks for it). The outputs are committed, so the site never needs this script to run.
"""
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[2]
OUT = REPO / 'website' / 'public' / 'img'
LAUNCHER = REPO / 'clientPatcher' / 'launcher' / 'Assets'
INTERFACE = REPO / 'localTools' / 'interface' / 'assets'

# name -> (source, longest side, quality)
PICTURES = {
    'hero.jpg': (REPO / 'clientPatcher' / 'assets' / 'login' / 'LoginBackdrop-source.png', 1920, 82),
    'hero-wide.jpg': (REPO / 'clientPatcher' / 'assets' / 'login' / 'LoginBackdropWide-source.png', 2400, 82),
    # Portraits for the home page's cards (no raid boss a player has not met yet)
    'portrait-challenge.jpg': (INTERFACE / 'challengeGod' / 'portrait.png', 900, 82),
    'portrait-frontier.jpg': (INTERFACE / 'frontier' / 'colossus-1.png', 900, 82),
}


def save(source: Path, target: Path, longest: int, quality: int):
    image = Image.open(source)
    if image.mode in ('RGBA', 'LA', 'P'):
        image = image.convert('RGBA')
    scale = min(1.0, longest / max(image.size))
    if scale < 1.0:
        image = image.resize((round(image.width * scale), round(image.height * scale)), Image.LANCZOS)
    if target.suffix == '.png':
        image.save(target, optimize=True)
    else:
        image.convert('RGB').save(target, quality=quality, optimize=True, progressive=True)


UI = OUT / 'ui'


def row(*names):
    """Retail pieces side by side (a 3-slice: left cap, stretchable middle, right cap)"""
    pieces = [Image.open(UI / name).convert('RGBA') for name in names]
    joined = Image.new('RGBA', (sum(piece.width for piece in pieces), max(piece.height for piece in pieces)))
    x = 0
    for piece in pieces:
        joined.paste(piece, (x, 0))
        x += piece.width
    return joined


def nine_slice(prefix, size):
    """A frame's corners and edges in one 3x3 image, for CSS border-image (the middle stays empty)"""
    frame = Image.new('RGBA', (size * 3, size * 3))
    cells = {
        'cornertopleft': (0, 0), 'edgetop': (1, 0), 'cornertopright': (2, 0),
        'edgeleft': (0, 1), 'edgeright': (2, 1),
        'cornerbottomleft': (0, 2), 'edgebottom': (1, 2), 'cornerbottomright': (2, 2),
    }
    for part, (column, line) in cells.items():
        frame.paste(Image.open(UI / f'{prefix}-{part}-2x.png').convert('RGBA'), (column * size, line * size))
    return frame


def compose():
    """The retail pieces (tools/extractRetailUi.py) joined into what the CSS uses"""
    nine_slice('ui-frame-diamondmetal', 128).save(UI / 'frame-metal.png', optimize=True)
    row('ui-frame-diamondmetal-header-cornerleft-2x.png', 'ui-frame-diamondmetal-header-tile-2x.png',
        'ui-frame-diamondmetal-header-cornerright-2x.png').save(UI / 'header-metal.png', optimize=True)
    row('128-redbutton-left.png', '128-redbutton-right.png').save(UI / 'button-red.png', optimize=True)
    row('128-redbutton-left-pressed.png', '128-redbutton-right-pressed.png').save(UI / 'button-red-pressed.png',
                                                                                    optimize=True)
    row('ui-frame-neutral-titleleft.png', 'ui-frame-neutral-titlemiddle.png',
        'ui-frame-neutral-titleright.png').save(UI / 'title-plate.png', optimize=True)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, (source, longest, quality) in PICTURES.items():
        save(source, OUT / name, longest, quality)
    # The news pictures, as the launcher shows them (news-<Image>.jpg)
    for source in LAUNCHER.glob('news-*.jpg'):
        save(source, OUT / source.name, 960, 82)
    save(LAUNCHER / 'logo.png', OUT / 'logo.png', 480, 0)
    save(REPO / 'clientPatcher' / 'assets' / 'logo' / 'WorldOfWarcraft-Evolution.png', OUT / 'logo-large.png', 1100, 0)
    compose()
    for target in sorted(OUT.glob('*.*')):
        print(f'{target.stat().st_size // 1024:5} KB  {target.name}')


if __name__ == '__main__':
    main()
