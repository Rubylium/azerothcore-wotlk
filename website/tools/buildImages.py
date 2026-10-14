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
    'pestifere.jpg': (REPO / 'clientPatcher' / 'assets' / 'launcher' / 'background-pestifere-source.png', 1920, 82),
    'raid-hollowvoice.jpg': (INTERFACE / 'hollowVoice' / 'backdrop.png', 1536, 82),
    'raid-planetarium.jpg': (INTERFACE / 'challengeGod' / 'backdrop.png', 1536, 82),
    'crest-barbarian.jpg': (INTERFACE / 'classes' / 'barbarian.png', 600, 85),
    'crest-necromancer.jpg': (INTERFACE / 'classes' / 'necromancer.png', 600, 85),
    'crest-pestifere.jpg': (INTERFACE / 'classes' / 'pestifere.png', 600, 85),
    'crest-oathblade.jpg': (INTERFACE / 'classes' / 'oathblade.png', 600, 85),
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


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, (source, longest, quality) in PICTURES.items():
        save(source, OUT / name, longest, quality)
    # The news pictures, as the launcher shows them (news-<Image>.jpg)
    for source in LAUNCHER.glob('news-*.jpg'):
        save(source, OUT / source.name, 960, 82)
    save(LAUNCHER / 'logo.png', OUT / 'logo.png', 480, 0)
    for target in sorted(OUT.iterdir()):
        print(f'{target.stat().st_size // 1024:5} KB  {target.name}')


if __name__ == '__main__':
    main()
