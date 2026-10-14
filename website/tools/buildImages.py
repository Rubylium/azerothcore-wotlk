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


def nine_slice(prefix, corner, thickness):
    """A frame's corners and edges in one 3x3 image for CSS border-image, each edge against the outer side of its
    cell (the middle stays empty). The edges are plain bars: brought to the cell's length along their run only."""
    size = corner * 3
    frame = Image.new('RGBA', (size, size))
    piece = lambda part: Image.open(UI / f'{prefix}-{part}.png').convert('RGBA')
    frame.paste(piece('cornertopleft'), (0, 0))
    frame.paste(piece('cornertopright'), (2 * corner, 0))
    frame.paste(piece('cornerbottomleft'), (0, 2 * corner))
    frame.paste(piece('cornerbottomright'), (2 * corner, 2 * corner))
    frame.paste(piece('edgetop').resize((corner, thickness), Image.LANCZOS), (corner, 0))
    frame.paste(piece('edgebottom').resize((corner, thickness), Image.LANCZOS), (corner, size - thickness))
    frame.paste(piece('edgeleft').resize((thickness, corner), Image.LANCZOS), (0, corner))
    frame.paste(piece('edgeright').resize((thickness, corner), Image.LANCZOS), (size - thickness, corner))
    return frame


def title_plate(width):
    """The neutral title plate as the client draws it: the middle strip runs underneath, the two ends (which fade
    out towards the middle) are laid over it"""
    left = Image.open(UI / 'ui-frame-neutral-titleleft.png').convert('RGBA')
    middle = Image.open(UI / 'ui-frame-neutral-titlemiddle.png').convert('RGBA')
    right = Image.open(UI / 'ui-frame-neutral-titleright.png').convert('RGBA')
    plate = Image.new('RGBA', (width, left.height))
    for x in range(60, width - 60, middle.width):
        plate.alpha_composite(middle, (x, 0))
    plate.alpha_composite(left, (0, 0))
    plate.alpha_composite(right, (width - right.width, 0))
    return plate


def compose():
    """The retail pieces (tools/extractRetailUi.py) joined into what the CSS uses"""
    nine_slice('plunderstorm-nineslice', 166, 30).save(UI / 'frame-gold.png', optimize=True)
    row('128-redbutton-left.png', '128-redbutton-right.png').save(UI / 'button-red.png', optimize=True)
    row('128-redbutton-left-pressed.png', '128-redbutton-right-pressed.png').save(UI / 'button-red-pressed.png',
                                                                                    optimize=True)
    title_plate(600).save(UI / 'title-plate.png', optimize=True)


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
