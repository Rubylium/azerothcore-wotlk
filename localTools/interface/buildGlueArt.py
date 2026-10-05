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
- LoginForeground1/2.blp: the art's foreground (the terrace, the statues, the braziers and banners, the near trees
  and side cliffs; the mask from buildLoginMask.py), cut like LoginBackdrop with the mask as its alpha: the login
  screen draws it again over its clouds, mist and birds so they pass behind the statues.
- LoginClouds.blp, LoginMist.blp: soft white wisps (512x256, 512x128), tiling across, faded at the top and bottom:
  the clouds drifting over the sky and the mist rolling between the cliffs (tinted by the login screen).
- LoginRays.blp: the sun's rays, white streaks fanning out from the middle of a 512x512 texture, fading outwards.
- LoginBird.blp: a bird's silhouette in four frames of its wing beat (64x64 each, side by side in 256x64).
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
LOGIN_WIDE_SOURCE = os.path.join(REPO_ROOT, 'clientPatcher', 'assets', 'login', 'LoginBackdropWide-source.png')
LOGIN_MASK = os.path.join(REPO_ROOT, 'clientPatcher', 'assets', 'login', 'LoginForeground-mask.png')
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

LOGIN_TILE_SIZE = (2048, 1024)      # AccountLogin.lua ART_TILE_WIDTH, ART_HEIGHT
LOGIN_ART_SIZE = (3642, 1024)       # AccountLogin.lua ART_WIDTH, ART_HEIGHT


def login_backdrop():
    """The ultrawide art (32:9, so a 21:9 or 32:9 window shows it whole and a 16:9 one its middle), upscaled to
    1024 pixels high and cut into two 2048x1024 tiles side by side, the second padded by repeating its last column"""
    source = Image.open(LOGIN_WIDE_SOURCE).convert('RGB')
    art = source.resize(LOGIN_ART_SIZE, Image.Resampling.LANCZOS)
    art = art.filter(ImageFilter.UnsharpMask(radius=1.2, percent=60, threshold=2)).convert('RGBA')
    tiles = []
    for index in range(2):
        left = index * LOGIN_TILE_SIZE[0]
        piece = art.crop((left, 0, min(left + LOGIN_TILE_SIZE[0], LOGIN_ART_SIZE[0]), LOGIN_ART_SIZE[1]))
        tile = Image.new('RGBA', LOGIN_TILE_SIZE)
        tile.paste(piece, (0, 0))
        if piece.width < LOGIN_TILE_SIZE[0]:
            edge = piece.crop((piece.width - 1, 0, piece.width, piece.height))
            tile.paste(edge.resize((LOGIN_TILE_SIZE[0] - piece.width, piece.height)), (piece.width, 0))
        tiles.append(tile)
    return tiles


def login_foreground():
    """The foreground's tiles: the art (as login_backdrop makes it) with the mask as its alpha, its edge softened by a
    pixel"""
    source = Image.open(LOGIN_WIDE_SOURCE).convert('RGB')
    art = source.resize(LOGIN_ART_SIZE, Image.Resampling.LANCZOS)
    art = art.filter(ImageFilter.UnsharpMask(radius=1.2, percent=60, threshold=2)).convert('RGBA')
    mask = Image.open(LOGIN_MASK).convert('L').resize(LOGIN_ART_SIZE, Image.Resampling.LANCZOS)
    art.putalpha(mask.filter(ImageFilter.GaussianBlur(1.0)))
    tiles = []
    for index in range(2):
        left = index * LOGIN_TILE_SIZE[0]
        piece = art.crop((left, 0, min(left + LOGIN_TILE_SIZE[0], LOGIN_ART_SIZE[0]), LOGIN_ART_SIZE[1]))
        tile = Image.new('RGBA', LOGIN_TILE_SIZE, (0, 0, 0, 0))
        tile.paste(piece, (0, 0))
        tiles.append(tile)
    return tiles


def tiling_noise(width, height, scales, seed):
    """Smooth noise in 0-1 that tiles across (it wraps at the left and right edges): white noise blurred at a few
    scales, summed with the larger scales weighing more"""
    import numpy
    from scipy.ndimage import gaussian_filter

    random = numpy.random.default_rng(seed)
    total = numpy.zeros((height, width))
    weight = 0.0
    for scale, amount in scales:
        layer = gaussian_filter(random.random((height, width)), sigma=scale, mode='wrap')
        layer = (layer - layer.min()) / max(layer.max() - layer.min(), 1e-6)
        total += layer * amount
        weight += amount
    return total / weight


def soft_wisps(width, height, scales, seed, threshold, top_fade, bottom_fade):
    """White wisps, their alpha the noise above a threshold, faded to nothing toward the top and bottom edges"""
    import numpy

    noise = tiling_noise(width, height, scales, seed)
    alpha = numpy.clip((noise - threshold) / (1 - threshold), 0, 1) ** 1.4
    rows = (numpy.arange(height) + 0.5) / height
    fade = numpy.clip(rows / top_fade, 0, 1) * numpy.clip((1 - rows) / bottom_fade, 0, 1)
    alpha *= (fade * fade * (3 - 2 * fade))[:, None]
    pixels = numpy.zeros((height, width, 4), numpy.uint8)
    pixels[..., :3] = 255
    pixels[..., 3] = (alpha * 255).astype(numpy.uint8)
    return Image.fromarray(pixels, 'RGBA')


def login_clouds():
    return soft_wisps(512, 256, [(36, 1.0), (18, 0.45)], seed=7, threshold=0.45, top_fade=0.35, bottom_fade=0.35)


def login_mist():
    return soft_wisps(512, 128, [(30, 1.0), (14, 0.5), (6, 0.25)], seed=11, threshold=0.3, top_fade=0.5,
                      bottom_fade=0.3)


def login_rays():
    """Streaks fanning out from the middle: each angle's brightness a few dozen narrow peaks of random widths and
    strengths, fading with the distance from the middle (and right at it, where the sun's own glow takes over)"""
    import numpy

    size = 512
    random = numpy.random.default_rng(5)
    angles = numpy.linspace(0, 2 * numpy.pi, 2048, endpoint=False)
    profile = numpy.zeros_like(angles)
    for _ in range(36):
        centre = random.uniform(0, 2 * numpy.pi)
        width = random.uniform(0.01, 0.05)
        strength = random.uniform(0.3, 1.0)
        distance = numpy.angle(numpy.exp(1j * (angles - centre)))
        profile += strength * numpy.exp(-(distance / width) ** 2)
    profile = numpy.clip(profile / profile.max(), 0, 1)
    y, x = numpy.mgrid[0:size, 0:size] + 0.5
    dx, dy = x / size * 2 - 1, y / size * 2 - 1
    radius = numpy.sqrt(dx * dx + dy * dy)
    theta = numpy.mod(numpy.arctan2(dy, dx), 2 * numpy.pi)
    streak = profile[(theta / (2 * numpy.pi) * len(profile)).astype(int) % len(profile)]
    falloff = numpy.clip(1 - radius, 0, 1) ** 1.6 * numpy.clip(radius / 0.08, 0, 1)
    pixels = numpy.zeros((size, size, 4), numpy.uint8)
    pixels[..., :3] = 255
    pixels[..., 3] = (numpy.clip(streak * falloff, 0, 1) * 255).astype(numpy.uint8)
    return Image.fromarray(pixels, 'RGBA')


def login_bird():
    """A bird seen from below and behind as it flies: a slim body and two wings, raised, level, lowered and level
    again over the four frames; drawn four times larger and scaled down for soft edges"""
    frame, scale = 64, 4
    sheet = Image.new('RGBA', (frame * 4 * scale, frame * scale), (0, 0, 0, 0))
    draw = ImageDraw.Draw(sheet)
    for index, lift in enumerate((0.55, 0.1, -0.35, 0.1)):
        cx, cy = (index * frame + frame / 2) * scale, frame / 2 * scale
        draw.ellipse((cx - 3 * scale, cy - 2 * scale, cx + 3 * scale, cy + 3 * scale), fill=(20, 16, 14, 255))
        for side in (-1, 1):
            # Each wing: from the shoulder out to its tip, bent at the wrist
            wrist = (cx + side * 12 * scale, cy - lift * 10 * scale)
            tip = (cx + side * 26 * scale, cy - lift * 22 * scale + 4 * scale)
            draw.polygon([(cx + side * 2 * scale, cy - 1 * scale), wrist, tip,
                          (wrist[0] + side * 1 * scale, wrist[1] + 4 * scale),
                          (cx + side * 2 * scale, cy + 2 * scale)], fill=(20, 16, 14, 255))
    return sheet.resize((frame * 4, frame), Image.Resampling.LANCZOS)


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
    for index, tile in enumerate(login_backdrop(), 1):
        writer.writeRawBlp(tile, os.path.join(OUTPUT, f'LoginBackdrop{index}.blp'))
    for index, tile in enumerate(login_foreground(), 1):
        writer.writeRawBlp(tile, os.path.join(OUTPUT, f'LoginForeground{index}.blp'))
    writer.writeRawBlp(login_clouds(), os.path.join(OUTPUT, 'LoginClouds.blp'))
    writer.writeRawBlp(login_mist(), os.path.join(OUTPUT, 'LoginMist.blp'))
    writer.writeRawBlp(login_rays(), os.path.join(OUTPUT, 'LoginRays.blp'))
    writer.writeRawBlp(login_bird(), os.path.join(OUTPUT, 'LoginBird.blp'))
    writer.writeRawBlp(login_vignette(), os.path.join(OUTPUT, 'LoginVignette.blp'))
    writer.writeRawBlp(login_logo(), os.path.join(OUTPUT, 'LoginLogo.blp'))
    print(f'Built the login screen art in {OUTPUT}')


if __name__ == '__main__':
    main()
