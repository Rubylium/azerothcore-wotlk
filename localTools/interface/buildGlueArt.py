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
- LoginBackdrop1/2.blp: the far layer of the ultrawide login art (LoginBackdropWide-source.png, upscaled to
  3642x1024 and cut into two 2048x1024 tiles): the sky, the far city, the mist. Where the near layer and the
  banners' cloth stand (LoginLayers-mask.png, from buildLoginMask.py) it is painted in from around them, so the far
  layer can slide behind them; the birds painted near the sun are painted out (the login screen flies its own).
  Uncompressed, so the sky keeps its gradients.
- LoginForeground1/2.blp: the near layer (the terrace, the statues, the braziers, the banners' poles, the trees and
  the side cliffs), cut the same way with the mask as its alpha: it never moves.
- LoginBanners.blp: the two banners' cloth, each in a 128x512 column of a 256x512 texture, the colour under its
  transparent pixels spread from its edge; the login screen waves it in strips (AccountLogin.lua BANNERS).
- LoginFalls.blp: the waterfalls' flow, white streaks scrolling down within each fall's own shape, 16 frames of a
  loop side by side per fall, at half the art's resolution (AccountLogin.lua FALLS, where each fall's frames are).
- LoginClouds.blp, LoginMist.blp: soft white wisps (512x256, 512x128), tiling across, faded at the top and bottom:
  the clouds drifting over the sky and the mist rolling between the cliffs (tinted by the login screen).
- LoginRays.blp: the sun's rays, white streaks fanning out from the middle of a 512x512 texture, fading outwards.
- LoginBird.blp: a bird seen from ahead, in eight frames of its wing beat (64x64 each, side by side in 512x64).
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
LOGIN_MASK = os.path.join(REPO_ROOT, 'clientPatcher', 'assets', 'login', 'LoginLayers-mask.png')
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


# The birds painted in the sky near the sun (art pixels): painted out of the far layer
PAINTED_BIRDS = (2040, 215, 2240, 325)
# The waterfalls (art pixels; buildLoginMask.py WATERFALLS, which says which are in the near layer)
WATERFALLS = [
    (125, 416, 177, 568), (233, 160, 279, 326), (340, 146, 376, 303), (439, 480, 513, 687), (1437, 576, 1481, 678),
    (1501, 614, 1525, 690), (1657, 422, 1702, 520), (1762, 454, 1845, 574),
    (2174, 477, 2228, 574), (2852, 428, 2912, 584), (2995, 442, 3029, 539), (3251, 556, 3339, 704),
    (3348, 260, 3414, 436),
]
FALL_FRAMES = 16                    # AccountLogin.lua FALL_FRAMES
FALL_PERIOD = 64                    # art pixels the streaks scroll down over one loop (AccountLogin.lua FALL_PERIOD)
BANNER_COLUMN = (128, 512)          # AccountLogin.lua BANNER_COLUMN


def login_art():
    """The ultrawide art (32:9, so a 21:9 or 32:9 window shows it whole and a 16:9 one its middle), upscaled to
    1024 pixels high"""
    source = Image.open(LOGIN_WIDE_SOURCE).convert('RGB')
    art = source.resize(LOGIN_ART_SIZE, Image.Resampling.LANCZOS)
    return art.filter(ImageFilter.UnsharpMask(radius=1.2, percent=60, threshold=2))


def login_layers():
    """The near layer's and the cloth's masks (0-255 numpy arrays) at the art's size"""
    import numpy

    layers = Image.open(LOGIN_MASK).convert('RGB').resize(LOGIN_ART_SIZE, Image.Resampling.LANCZOS)
    pixels = numpy.asarray(layers)
    return pixels[..., 0], pixels[..., 2]


def art_tiles(art, pad):
    """The art cut into two 2048x1024 tiles side by side; pad: the second filled out by repeating its last column
    (else left clear)"""
    tiles = []
    for index in range(2):
        left = index * LOGIN_TILE_SIZE[0]
        piece = art.crop((left, 0, min(left + LOGIN_TILE_SIZE[0], LOGIN_ART_SIZE[0]), LOGIN_ART_SIZE[1]))
        tile = Image.new('RGBA', LOGIN_TILE_SIZE, (0, 0, 0, 0))
        tile.paste(piece, (0, 0))
        if pad and piece.width < LOGIN_TILE_SIZE[0]:
            edge = piece.crop((piece.width - 1, 0, piece.width, piece.height))
            tile.paste(edge.resize((LOGIN_TILE_SIZE[0] - piece.width, piece.height)), (piece.width, 0))
        tiles.append(tile)
    return tiles


def login_backdrop():
    """The far layer: the art, painted in (OpenCV's inpainting, at the source's size) where the near layer and the
    cloth stand - a few pixels wider, so no edge of theirs is left on it - and where the painted birds fly"""
    import cv2
    import numpy

    source = Image.open(LOGIN_WIDE_SOURCE).convert('RGB')
    pixels = cv2.cvtColor(numpy.asarray(source), cv2.COLOR_RGB2BGR)
    height, width = pixels.shape[:2]
    layers = cv2.imread(LOGIN_MASK)
    hole = numpy.where((layers[..., 2] > 0) | (layers[..., 0] > 0), 255, 0).astype(numpy.uint8)
    hole = cv2.dilate(hole, numpy.ones((7, 7), numpy.uint8))
    # The painted birds: what is clearly darker than the sky around it
    scale = width / LOGIN_ART_SIZE[0]
    x0, y0, x1, y1 = (int(value * scale) for value in PAINTED_BIRDS)
    patch = cv2.cvtColor(pixels[y0:y1, x0:x1], cv2.COLOR_BGR2GRAY).astype(numpy.int32)
    sky = cv2.medianBlur(pixels[y0:y1, x0:x1], 15)
    dark = (cv2.cvtColor(sky, cv2.COLOR_BGR2GRAY).astype(numpy.int32) - patch) > 22
    birds = cv2.dilate(numpy.where(dark, 255, 0).astype(numpy.uint8), numpy.ones((5, 5), numpy.uint8))
    hole[y0:y1, x0:x1] = numpy.maximum(hole[y0:y1, x0:x1], birds)
    filled = cv2.inpaint(pixels, hole, 6, cv2.INPAINT_TELEA)
    # Inside the larger holes the painting-in streaks: soften it there (only its edge ever shows)
    depth = cv2.distanceTransform(hole, cv2.DIST_L2, 5)
    softened = cv2.GaussianBlur(filled, (0, 0), 6)
    blend = numpy.clip((depth - 6) / 20, 0, 1)[..., None]
    filled = (filled * (1 - blend) + softened * blend).astype(numpy.uint8)
    far = Image.fromarray(cv2.cvtColor(filled, cv2.COLOR_BGR2RGB)).resize(LOGIN_ART_SIZE, Image.Resampling.LANCZOS)
    far = far.filter(ImageFilter.UnsharpMask(radius=1.2, percent=60, threshold=2))
    return art_tiles(far.convert('RGBA'), pad=True)


def login_foreground():
    """The near layer's tiles: the art with the near mask as its alpha, its edge softened by a pixel"""
    near, _ = login_layers()
    art = login_art().convert('RGBA')
    art.putalpha(Image.fromarray(near).filter(ImageFilter.GaussianBlur(1.0)))
    return art_tiles(bleed_colour(art), pad=False)


def login_banners():
    """The two banners' cloth (left to right), each at the top left of its 128x512 column, the art with the cloth
    mask as its alpha; prints where each sits in the art, for AccountLogin.lua BANNERS"""
    import cv2

    _, cloth = login_layers()
    art = login_art().convert('RGBA')
    art.putalpha(Image.fromarray(cloth).filter(ImageFilter.GaussianBlur(0.7)))
    count, _, stats, _ = cv2.connectedComponentsWithStats((cloth > 127).astype('uint8'))
    pieces = sorted((stats[index] for index in range(1, count)), key=lambda stat: -stat[cv2.CC_STAT_AREA])[:2]
    sheet = Image.new('RGBA', (BANNER_COLUMN[0] * 2, BANNER_COLUMN[1]), (0, 0, 0, 0))
    print('BANNERS:')
    for column, stat in enumerate(sorted(pieces, key=lambda stat: stat[cv2.CC_STAT_LEFT])):
        left, top = int(stat[cv2.CC_STAT_LEFT]) - 2, int(stat[cv2.CC_STAT_TOP]) - 2
        width, height = int(stat[cv2.CC_STAT_WIDTH]) + 4, int(stat[cv2.CC_STAT_HEIGHT]) + 4
        assert width <= BANNER_COLUMN[0] and height <= BANNER_COLUMN[1], (width, height)
        sheet.paste(art.crop((left, top, left + width, top + height)), (column * BANNER_COLUMN[0], 0))
        print(f'    {{ x = {left}, y = {top}, width = {width}, height = {height} }},')
    return bleed_colour(sheet)


def login_falls():
    """Each waterfall's flow: its shape (the bright, pale, vertically streaked pixels of its box), filled with white
    streaks - noise stretched downwards, tiling every FALL_PERIOD pixels - that scroll one period down over the loop's
    frames. At half the art's resolution, the frames of a fall side by side, the falls packed in rows; prints each
    fall's place in the texture, for AccountLogin.lua FALLS."""
    import cv2
    import numpy
    from scipy.ndimage import gaussian_filter

    art = numpy.asarray(login_art()).astype(numpy.float32) / 255
    near, _ = login_layers()
    hls = cv2.cvtColor(art, cv2.COLOR_RGB2HLS)
    gray = cv2.cvtColor(art, cv2.COLOR_RGB2GRAY)
    across = numpy.abs(cv2.Sobel(gray, cv2.CV_32F, 1, 0, ksize=3))
    down = numpy.abs(cv2.Sobel(gray, cv2.CV_32F, 0, 1, ksize=3))
    streaked = cv2.GaussianBlur(across, (0, 0), 3) / (cv2.GaussianBlur(down, (0, 0), 3) + 0.02)
    water = ((hls[..., 1] > 0.5) & (hls[..., 2] < 0.4) & (streaked > 1.2)).astype(numpy.float32)
    random = numpy.random.default_rng(3)
    half = FALL_PERIOD // 2
    texture_width, cursor_x, cursor_y, row_height = 1024, 0, 0, 0
    cells = []
    for x0, y0, x1, y1 in WATERFALLS:
        width, height = (x1 - x0 + 1) // 2, (y1 - y0 + 1) // 2
        shape = cv2.resize(water[y0:y1, x0:x1], (width, height), interpolation=cv2.INTER_AREA)
        shape = cv2.morphologyEx(shape, cv2.MORPH_CLOSE, numpy.ones((7, 3), numpy.uint8))
        shape = numpy.clip(cv2.GaussianBlur(shape, (0, 0), 1.2) * 1.6, 0, 1)
        # Fade in at the top and out at the bottom, so the box's edges never show
        rows = (numpy.arange(height) + 0.5) / height
        shape *= (numpy.clip(rows / 0.12, 0, 1) * numpy.clip((1 - rows) / 0.18, 0, 1))[:, None]
        streaks = gaussian_filter(random.random((half, width)), sigma=(8, 0.6), mode='wrap')
        streaks = (streaks - streaks.min()) / max(streaks.max() - streaks.min(), 1e-6)
        streaks = numpy.clip((streaks - 0.45) / 0.4, 0, 1) ** 1.5
        if cursor_x + width * FALL_FRAMES > texture_width:
            cursor_x, cursor_y, row_height = 0, cursor_y + row_height + 1, 0
        cells.append((cursor_x, cursor_y, width, height, shape, streaks))
        cursor_x += width * FALL_FRAMES + 1
        row_height = max(row_height, height)
    texture_height = 1 << (cursor_y + row_height - 1).bit_length()
    assert texture_height <= 1024, texture_height
    sheet = numpy.zeros((texture_height, texture_width, 4), numpy.uint8)
    sheet[..., :3] = 255
    print(f'FALLS ({texture_width}x{texture_height}: left, top, width, height in the texture):')
    for (x0, y0, x1, y1), (left, top, width, height, shape, streaks) in zip(WATERFALLS, cells):
        for frame in range(FALL_FRAMES):
            shift = frame * half // FALL_FRAMES
            rows = (numpy.arange(height) - shift) % half
            alpha = streaks[rows] * shape
            sheet[top:top + height, left + frame * width:left + (frame + 1) * width, 3] = (alpha * 255).astype(
                numpy.uint8)
        in_near = bool(near[(y0 + y1) // 2, (x0 + x1) // 2] > 127)
        print(f'    {{ {x0}, {y0}, {x1}, {y1}, {left}, {top}, {width}, {height}, {str(in_near).lower()} }},')
    return Image.fromarray(sheet, 'RGBA')


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
    """A bird seen from ahead as it flies, in eight frames of one wing beat: a small body and two long wings, each
    bent at the wrist, the hand lagging the arm (so the tips flick down after the wrists and up after them); the
    wings' leading edge a smooth curve, their trailing edge thinner toward the tip. Drawn eight times larger and
    scaled down, white (the login screen tints it)."""
    import math

    frame, scale = 64, 8
    sheet = Image.new('L', (frame * 8 * scale, frame * scale), 0)
    draw = ImageDraw.Draw(sheet)

    def point(cx, cy, x, y):
        return cx + x * scale, cy + y * scale

    for index in range(8):
        phase = index / 8 * 2 * math.pi
        cx, cy = (index * frame + frame / 2) * scale, (frame / 2 + 2) * scale
        arm = math.sin(phase)                       # 1: wings up, -1: down
        hand = math.sin(phase - 0.9)
        for side in (-1, 1):
            shoulder = (side * 2.5, -0.5)
            wrist = (side * 12.5, -arm * 8.5 - 1.5)
            tip = (wrist[0] + side * 13.5 * math.cos(hand * 0.5), wrist[1] - hand * 9.0 + 2.5)
            # Leading edge out to the tip, trailing edge back, a little below, thinner toward the tip
            lead = [shoulder, ((shoulder[0] + wrist[0]) / 2, (shoulder[1] + wrist[1]) / 2 - 1.2), wrist,
                    ((wrist[0] + tip[0]) / 2, (wrist[1] + tip[1]) / 2 - 0.6), tip]
            trail = [((wrist[0] + tip[0]) / 2 - side * 0.8, (wrist[1] + tip[1]) / 2 + 2.4),
                     (wrist[0] - side * 1.2, wrist[1] + 4.4),
                     ((shoulder[0] + wrist[0]) / 2, (shoulder[1] + wrist[1]) / 2 + 4.2), (side * 2.0, 3.0)]
            draw.polygon([point(cx, cy, x, y) for x, y in lead + trail], fill=255)
        draw.ellipse((cx - 3.2 * scale, cy - 2.4 * scale, cx + 3.2 * scale, cy + 3.4 * scale), fill=255)
        draw.ellipse((cx - 1.8 * scale, cy - 4.0 * scale, cx + 1.8 * scale, cy - 0.6 * scale), fill=255)
    alpha = sheet.resize((frame * 8, frame), Image.Resampling.LANCZOS)
    bird = Image.new('RGBA', alpha.size, (255, 255, 255, 0))
    bird.putalpha(alpha)
    return bird


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
    writer.writeRawBlp(login_banners(), os.path.join(OUTPUT, 'LoginBanners.blp'))
    writer.writeRawBlp(login_falls(), os.path.join(OUTPUT, 'LoginFalls.blp'))
    writer.writeRawBlp(login_clouds(), os.path.join(OUTPUT, 'LoginClouds.blp'))
    writer.writeRawBlp(login_mist(), os.path.join(OUTPUT, 'LoginMist.blp'))
    writer.writeRawBlp(login_rays(), os.path.join(OUTPUT, 'LoginRays.blp'))
    writer.writeRawBlp(login_bird(), os.path.join(OUTPUT, 'LoginBird.blp'))
    writer.writeRawBlp(login_vignette(), os.path.join(OUTPUT, 'LoginVignette.blp'))
    writer.writeRawBlp(login_logo(), os.path.join(OUTPUT, 'LoginLogo.blp'))
    print(f'Built the login screen art in {OUTPUT}')


if __name__ == '__main__':
    main()
