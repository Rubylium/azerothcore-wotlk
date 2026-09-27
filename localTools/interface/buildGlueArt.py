"""Builds the artwork of the Evolutions glue screens.

    python buildGlueArt.py            everything
    python buildGlueArt.py login      only the login screen (no game client needed)

Writes clientPatcher/interface/Interface/Glues/Evolutions/:
- RealmBackdrop.blp: behind the realm selection. Icecrown Citadel's gate (LoadScreenIcecrownCitadel), cut below the
  game logo and the parchment border, 1024x512 for a picture about 2.44 times as wide as it is tall (the loading
  screens are drawn 4:3 from a square texture)
- RealmCard.blp: a realm's card. Arthas and Frostmourne (LoadScreenNorthrendWide, a 16:9 picture in a square
  texture), the part left of the logo, 256x512 for a picture 0.64 times as wide as it is tall
- RealmCardGlow.blp: the light around the selected card (card_glow below)

GlueXML/RealmList.lua knows those proportions and sizes (BACKDROP_ASPECT, CARD_ART_ASPECT, CARD_WIDTH/HEIGHT,
CARD_GLOW_MARGIN); change them together.

And, for the login screen (GlueXML/AccountLogin.lua), from clientPatcher/assets/login:
- LoginBackdrop.blp: the login art (LoginBackdrop-source.png, 1672x941) at its own pixels, unscaled, in the top-left
  corner of a 2048x1024 texture, the rest filled by repeating its right and bottom edges (so filtering and the
  smaller mip levels never pull in a foreign colour). Uncompressed, so the sky keeps its gradients.
  AccountLogin.lua knows the picture's size inside the texture (ART_WIDTH / ART_HEIGHT / ART_TEXTURE_*).
- LoginVignette.blp: black, its alpha an ellipse that is clear in the middle and darkens the edges and corners;
  stretched over the whole screen.
- LoginLogo.blp: the "World of Warcraft Evolution" logo (clientPatcher/assets/logo/WorldOfWarcraft-Evolution.png),
  cut to its visible part, scaled to 1024 wide in the top-left corner of a 1024x1024 texture, uncompressed with
  its alpha. The colour under transparent pixels is spread out from the logo's edge, so filtering does not draw a
  dark fringe. AccountLogin.lua knows its size in the texture (LOGO_WIDTH / LOGO_HEIGHT / LOGO_TEXTURE_SIZE).
"""
import importlib.util
import json
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw, ImageFilter

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
OUTPUT = os.path.join(REPO_ROOT, 'clientPatcher', 'interface', 'Interface', 'Glues', 'Evolutions')
LOGIN_SOURCE = os.path.join(REPO_ROOT, 'clientPatcher', 'assets', 'login', 'LoginBackdrop-source.png')
LOGO_SOURCE = os.path.join(REPO_ROOT, 'clientPatcher', 'assets', 'logo', 'WorldOfWarcraft-Evolution.png')
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
    build_login_art()
    if len(sys.argv) > 1 and sys.argv[1] == 'login':
        return
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



# --- the login screen ------------------------------------------------------------------------------------------------

LOGIN_TEXTURE_SIZE = (2048, 1024)   # AccountLogin.lua ART_TEXTURE_WIDTH, ART_TEXTURE_HEIGHT
LOGIN_ART_SIZE = (1672, 941)        # AccountLogin.lua ART_WIDTH, ART_HEIGHT


def login_backdrop():
    art = Image.open(LOGIN_SOURCE).convert('RGBA')
    if art.size != LOGIN_ART_SIZE:
        raise RuntimeError(f'{LOGIN_SOURCE} is {art.size}, AccountLogin.lua expects {LOGIN_ART_SIZE}')
    width, height = art.size
    canvas = Image.new('RGBA', LOGIN_TEXTURE_SIZE)
    canvas.paste(art, (0, 0))
    # Repeat the last column to the right, then the last row (now full width) downwards
    canvas.paste(art.crop((width - 1, 0, width, height)).resize((LOGIN_TEXTURE_SIZE[0] - width, height)), (width, 0))
    last_row = canvas.crop((0, height - 1, LOGIN_TEXTURE_SIZE[0], height))
    canvas.paste(last_row.resize((LOGIN_TEXTURE_SIZE[0], LOGIN_TEXTURE_SIZE[1] - height)), (0, height))
    return canvas


def login_vignette():
    size = 256
    mask = Image.new('L', (size, size), 0)
    pixels = mask.load()
    for y in range(size):
        for x in range(size):
            dx = (x + 0.5) / size * 2 - 1
            dy = (y + 0.5) / size * 2 - 1
            # 0 at the centre, 1 at the middle of each edge, about 1.41 in the corners
            distance = (dx * dx + dy * dy) ** 0.5
            t = min(max((distance - 0.55) / 0.85, 0), 1)
            pixels[x, y] = int(round(255 * 0.9 * t * t * (3 - 2 * t)))
    vignette = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    vignette.putalpha(mask)
    return vignette


LOGO_TEXTURE_SIZE = 1024           # AccountLogin.lua LOGO_TEXTURE_SIZE


def visible_part(image, threshold=8):
    """The image cut to the pixels that are more than faintly visible."""
    box = image.getchannel('A').point(lambda value: 255 if value > threshold else 0).getbbox()
    return image.crop(box)


def bleed_colour(image, passes=8):
    """Gives transparent pixels the colour of the nearest visible ones (alpha unchanged), so that filtering between
    the logo's edge and the empty space around it, or a smaller mip level, never mixes in black."""
    import numpy

    pixels = numpy.asarray(image.convert('RGBA'), dtype=numpy.float64)
    alpha = pixels[..., 3:4] / 255.0
    colour = pixels[..., :3]
    weight = alpha.copy()
    total = colour * alpha
    filled_colour = numpy.where(alpha > 0, colour, 0.0)
    known = (alpha[..., 0] > 0)
    for step in range(passes):
        radius = 2 ** step
        blurred_total = numpy.asarray(Image.fromarray(numpy.clip(total, 0, 255).astype(numpy.uint8))
                                      .filter(ImageFilter.BoxBlur(radius)), dtype=numpy.float64)
        blurred_weight = numpy.asarray(Image.fromarray((numpy.clip(weight[..., 0], 0, 1) * 255).astype(numpy.uint8))
                                       .filter(ImageFilter.BoxBlur(radius)), dtype=numpy.float64)[..., None] / 255.0
        reach = (blurred_weight[..., 0] > 0.002) & ~known
        estimate = blurred_total / numpy.maximum(blurred_weight, 1e-6)
        filled_colour[reach] = estimate[reach]
        known = known | reach
    out = numpy.concatenate([numpy.clip(filled_colour, 0, 255), pixels[..., 3:4]], axis=2).astype(numpy.uint8)
    return Image.fromarray(out, 'RGBA')


def login_logo():
    logo = visible_part(Image.open(LOGO_SOURCE).convert('RGBA'))
    width = LOGO_TEXTURE_SIZE
    height = round(logo.height * width / logo.width)
    logo = logo.resize((width, height), Image.Resampling.LANCZOS)
    canvas = Image.new('RGBA', (LOGO_TEXTURE_SIZE, LOGO_TEXTURE_SIZE), (0, 0, 0, 0))
    canvas.paste(logo, (0, 0))
    print(f'LoginLogo: the logo is {width}x{height} in the {LOGO_TEXTURE_SIZE}x{LOGO_TEXTURE_SIZE} texture')
    return bleed_colour(canvas)


def build_login_art():
    writer.writeRawBlp(login_backdrop(), os.path.join(OUTPUT, 'LoginBackdrop.blp'))
    writer.writeRawBlp(login_vignette(), os.path.join(OUTPUT, 'LoginVignette.blp'))
    writer.writeRawBlp(login_logo(), os.path.join(OUTPUT, 'LoginLogo.blp'))
    print(f'Built the login screen art in {OUTPUT}')


if __name__ == '__main__':
    main()
