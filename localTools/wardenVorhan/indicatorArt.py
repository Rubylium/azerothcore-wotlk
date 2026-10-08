"""Gardien-chef Vorhan's painted marks, as the ground indicator builder draws them (localTools/groundIndicators,
shapes.json `picture`: "localTools/wardenVorhan/indicatorArt.py:<function>").

Each function returns the picture of one shape. The paintings are read from art/ (the brief:
.agents/plans/warden-vorhan/warden-vorhan.ASSETS.md); until one is there, a placeholder drawn here takes its place, so
the fight can be tested, and the next build picks the painting up by itself. A painting is never stretched: it is
resized at its own aspect only, and one painted at another aspect than the brief's stops the build.

- The gaze: one eye opening, 64 frames in an atlas: its lids drawn here, the eye and its halo painted (eye_open.png,
  eye_glow.png; until they are there, the globe of gaze_open.png and a drawn halo), its glow growing, breathing on
  the last frames, the burst.
- The numbers 1 to 8 and the roll call pairs ("1-2" ...), rendered here (painted numerals are unreliable): ember-red
  numerals in Cinzel, a black-iron edge and a red glow round them, readable on any floor and against any sky.

Pictures painted on pure black come back as RGB (the builder keys their black to transparent); the rest as RGBA.
"""
import math
import os

import numpy
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
ART = os.path.join(HERE, 'art')
FONT = os.path.join(REPO_ROOT, 'clientPatcher', 'launcher', 'Assets', 'Fonts', 'Cinzel-Bold.ttf')

# The brief's size of each painting (width, height): its aspect is checked, its pixels resized to it
SIZES = {
    'gaze_closed.png': (512, 512),
    'gaze_half.png': (512, 512),
    'gaze_open.png': (512, 512),
    'cell_iron.png': (1024, 1024),
    'cell_runes.png': (1024, 1024),
    'rollcall_mark.png': (512, 512),
    'wall_band_1.png': (1024, 256),
    'wall_band_2.png': (1024, 256),
    'wall_band_3.png': (1024, 256),
    'wall_band_4.png': (1024, 256),
    'eye_open.png': (1024, 1024),
    'eye_glow.png': (1024, 1024),
    'curfew_ring.png': (1024, 1024),
    'curfew_mark.png': (256, 256),
}


def painting(name):
    """The painting art/<name> at the brief's size, or its placeholder (RGB, on black)"""
    width, height = SIZES[name]
    path = os.path.join(ART, name)
    if not os.path.exists(path):
        return PLACEHOLDERS[name.split('.')[0]](width, height)
    image = Image.open(path).convert('RGB')
    if abs(image.width / image.height - width / height) > 0.01:
        raise SystemExit(f'{path}: {image.width} x {image.height}, the brief asks {width} x {height} '
                         f'(never stretched: crop it to that aspect)')
    if image.size != (width, height):
        image = image.resize((width, height), Image.LANCZOS)
    return image


def as_array(image):
    return numpy.asarray(image.convert('RGB'), dtype=numpy.float64) / 255.0


def to_image(array_):
    return Image.fromarray(numpy.clip(array_ * 255.0 + 0.5, 0, 255).astype(numpy.uint8))


def blur(array_, radius):
    """Gaussian blur of a float array (H x W or H x W x C) through PIL, per channel"""
    if array_.ndim == 2:
        return as_float(to_l(array_).filter(ImageFilter.GaussianBlur(radius)))
    return numpy.stack([blur(array_[..., c], radius) for c in range(array_.shape[2])], axis=2)


def to_l(array_):
    return Image.fromarray(numpy.clip(array_ * 255.0 + 0.5, 0, 255).astype(numpy.uint8), 'L')


def as_float(image):
    return numpy.asarray(image, dtype=numpy.float64) / 255.0


def grid(width, height):
    """Pixel centres in [-1, 1] (x right, y down) and their radius"""
    xs = (numpy.arange(width) + 0.5) / width * 2.0 - 1.0
    ys = (numpy.arange(height) + 0.5) / height * 2.0 - 1.0
    x, y = numpy.meshgrid(xs, ys)
    return x, y, numpy.hypot(x, y)


def smooth(edge0, edge1, value):
    t = numpy.clip((value - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def noise(width, height, scale, seed):
    """Soft value noise in [0, 1]: random cells upscaled smoothly"""
    rng = numpy.random.default_rng(seed)
    cells = rng.random((max(2, height // scale), max(2, width // scale)))
    return as_float(to_l(cells).resize((width, height), Image.BICUBIC))


# --- Numbers ------------------------------------------------------------------------------------------------------

EMBER_TOP = numpy.array([1.0, 0.86, 0.45])
EMBER_BOTTOM = numpy.array([0.85, 0.16, 0.04])
IRON = numpy.array([0.07, 0.06, 0.055])
GLOW = numpy.array([1.0, 0.18, 0.05])


def numeral(text, width, height, margin=0.12, glow=1.0):
    """text in ember-red Cinzel on a black-iron edge with a red glow, fitted to width x height (RGBA)"""
    scale = 4
    big_w, big_h = width * scale, height * scale
    # The largest size that fits inside the margins, its edge and glow included
    size = big_h
    while size > 8:
        font = ImageFont.truetype(FONT, size)
        stroke = max(2, size // 11)
        box = font.getbbox(text, stroke_width=stroke)
        if box[2] - box[0] <= big_w * (1 - 2 * margin) and box[3] - box[1] <= big_h * (1 - 2 * margin):
            break
        size = int(size * 0.95)
    x = (big_w - (box[2] - box[0])) / 2 - box[0]
    y = (big_h - (box[3] - box[1])) / 2 - box[1]

    def mask(stroke_width):
        image = Image.new('L', (big_w, big_h))
        ImageDraw.Draw(image).text((x, y), text, font=font, fill=255, stroke_width=stroke_width, stroke_fill=255)
        return as_float(image.resize((width, height), Image.LANCZOS))

    fill = mask(0)
    edge = mask(stroke * 1)
    # Its glow: a wide red halo round the edge
    halo = numpy.clip(blur(edge, width * 0.06) * 1.6, 0.0, 1.0) * glow
    rows = numpy.linspace(0.0, 1.0, height)[:, None, None]
    ember = EMBER_TOP * (1 - rows) + EMBER_BOTTOM * rows
    # A hotter core inside each stroke
    core = numpy.clip(blur(fill, width * 0.012) - 0.45, 0.0, 1.0)[..., None] / 0.55
    ember = numpy.clip(ember + core * 0.35, 0.0, 1.0)
    colour = GLOW * numpy.ones((height, width, 1))
    alpha = halo * 0.85
    # The iron edge over the glow, the ember fill over the edge
    colour = colour * (1 - edge[..., None]) + IRON * edge[..., None]
    alpha = alpha * (1 - edge) + edge
    colour = colour * (1 - fill[..., None]) + ember * fill[..., None]
    rgba = numpy.concatenate([colour, alpha[..., None]], axis=2)
    return Image.fromarray(numpy.clip(rgba * 255.0 + 0.5, 0, 255).astype(numpy.uint8), 'RGBA')


def overhead_number(number):
    """The number over a player's head (a billboard): 256 x 256"""
    return numeral(str(number), 256, 256, margin=0.1)


def cell_number(number):
    """A cell's number, its own breathing layer over the iron seal: 256 x 256, inside the seal's empty middle"""
    return numeral(str(number), 256, 256, margin=0.2)


def rollcall_number(pair):
    """A roll call mark's pair ("1-2"), over the plaque's flat middle: 256 x 256"""
    return numeral(pair.replace('-', '–'), 256, 256, margin=0.2)


# --- The gaze -----------------------------------------------------------------------------------------------------
#
# One eye floating on the warden, nothing round it: no plate, no frame, its outside clear. Lids that part slowly (an
# almond opening from a glowing slit), the eye behind them its painting (eye_open.png: one open eye alone on black,
# about 55% of the width; until it is there, the globe of gaze_open.png, its iron left out), its slit pupil widening,
# the eye heating and its red glow spreading (eye_glow.png, the halo alone on black; a drawn one until then), then
# breathing faster, then the burst - a red glow, dim enough never to flash. 64 frames of 256 x 128 (the eye is twice
# as wide as it is tall), 8 x 8 in a 2048 x 1024 atlas; shapes.json VW_Gaze's timeline (gaze_timeline) plays them.

GAZE_FRAME = (256, 128)
GAZE_GRID = 8
# Frames 0 to GAZE_OPENING - 1 open the eye; then the open eye at a growing glow (the timeline swaps them back and
# forth, faster and faster), the last the burst
GAZE_OPENING = 52
GAZE_BREATHING = [0.85, 0.95, 1.05, 1.15, 1.25, 1.35, 1.45, 1.55, 1.65, 1.75, 1.85]
GAZE_BURST = 2.4
# The globe in gaze_open.png: its middle and the radius of it sampled, as shares of the painting's width; and the eye
# in eye_open.png and eye_glow.png (centred, 55% of the width)
GAZE_BALL = (0.5, 0.363, 0.244)
EYE_PAINTED = (0.5, 0.5, 0.275)
# A painted eye's black keyed to clear (its pupil kept: the middle of the eye stays solid)
EYE_KNEE = 0.12
# The eye on its frame: half its width (of the frame's width), and how far the lids open at most (of that half)
EYE_HALF_WIDTH = 0.34
EYE_OPEN = 0.5
# Each frame drawn this many times bigger, then shrunk to its cell
GAZE_SUPERSAMPLE = 3


def eye_source():
    """What the eye is drawn from: (eye picture, its (x, y, radius) as shares of its width, keyed, halo picture or
    None)"""
    glow = as_array(painting('eye_glow.png')) if os.path.exists(os.path.join(ART, 'eye_glow.png')) else None
    if os.path.exists(os.path.join(ART, 'eye_open.png')):
        return as_array(painting('eye_open.png')), EYE_PAINTED, True, glow
    return as_array(painting('gaze_open.png')), GAZE_BALL, False, glow


def gaze_frame(source, opening, heat, glow, width, height):
    """One frame (RGBA float): opening 0 (a slit) to 1, heat 0 to 1 (the eye's colour), glow the red halo"""
    ball, place, keyed_eye, halo_picture = source
    xs = (numpy.arange(width) + 0.5) - width / 2.0
    ys = (numpy.arange(height) + 0.5) - height / 2.0
    x, y = numpy.meshgrid(xs, ys)
    half = EYE_HALF_WIDTH * width
    u, v = x / half, y / half
    pixel = 1.0 / half
    across = numpy.clip(1.0 - u ** 2, 0.0, 1.0)

    # The lids: an almond, open `opening` of the way, its corners pointed
    aperture = EYE_OPEN * opening * across ** 0.8
    inside = smooth(-1.5 * pixel, 1.5 * pixel, aperture - numpy.abs(v))

    # The eyeball behind them, its slit pupil widening as the eye opens (the globe's middle spread sideways)
    dilation = 0.55 + 1.05 * smooth(0.15, 1.0, opening)
    spread = numpy.exp(-(u / 0.3) ** 2)
    su = u * (spread / dilation + (1.0 - spread))
    bh, bw = ball.shape[:2]
    cx, cy, radius = place[0] * bw, place[1] * bw, place[2] * bw
    sx = numpy.clip(cx + su * radius, 0, bw - 1).astype(int)
    sy = numpy.clip(cy + v * radius, 0, bh - 1).astype(int)
    eye = ball[sy, sx]
    if keyed_eye:
        # Its painted black clear, but for the eye's middle (the pupil is black too)
        solid = numpy.clip(eye.max(axis=2) / EYE_KNEE, 0.0, 1.0)
        core = smooth(0.75, 0.5, numpy.hypot(u, v / 0.8))
        inside = inside * numpy.maximum(solid, core)
    # Cold and dim shut, hot as it opens, the pupil's rim burning
    eye = eye * (0.45 + 0.65 * heat)
    rim = numpy.exp(-((numpy.abs(u) * dilation - 0.06) / 0.06) ** 2) * numpy.clip(1 - numpy.abs(v) / 0.6, 0, 1)
    eye = eye + (rim * heat * 0.55)[..., None] * numpy.array([1.0, 0.55, 0.15])
    # Shadowed under the lids' edges
    eye = eye * (0.55 + 0.45 * smooth(0.0, 0.25, aperture - numpy.abs(v)))[..., None]

    # The lids' edge: a burning line, the slit of a shut eye
    edge = numpy.exp(-((numpy.abs(v) - aperture) / (1.8 * pixel + 0.012)) ** 2) * smooth(1.02, 0.85, numpy.abs(u))
    edge_colour = numpy.array([1.0, 0.28, 0.05]) * (0.6 + 0.4 * heat)

    # The red glow round the eye, wider as it grows (an ellipse, fading out well inside the frame)
    hx = 1.05 + 0.25 * min(glow, 2.0)
    hy = 0.18 + 0.30 * opening + 0.08 * min(glow, 2.0)
    halo = numpy.exp(-((u / hx) ** 2 + (v / hy) ** 2) * 2.2) * min(0.85, 0.28 * glow + 0.06)
    if halo_picture is not None:
        # The painted halo, at the eye's scale, its brightness its alpha; softly faded before the frame's edges (the
        # frame is half as tall as the painting) rather than cut
        gh, gw = halo_picture.shape[:2]
        gx = numpy.clip(place[0] * gw + u * place[2] * gw, 0, gw - 1).astype(int)
        gy = numpy.clip(place[1] * gw + v * place[2] * gw, 0, gh - 1).astype(int)
        painted = halo_picture[gy, gx]
        fade = numpy.exp(-((x / (0.42 * width)) ** 6 + (y / (0.4 * height)) ** 6))
        strength = min(1.0, 0.3 * glow + 0.05) * fade
        halo_alpha = numpy.clip(painted.max(axis=2) * 1.4, 0.0, 1.0) * strength
        halo_colour = numpy.where(halo_alpha[..., None] > 1e-4, painted * strength[..., None], 0.0)
    else:
        halo_alpha = halo
        halo_colour = numpy.array([0.85, 0.12, 0.03]) * halo[..., None]

    # Back to front, premultiplied: the glow, the eye, its edge
    colour = halo_colour
    alpha = halo_alpha.copy()
    colour = colour * (1 - inside[..., None]) + eye * inside[..., None]
    alpha = alpha * (1 - inside) + inside
    edge_alpha = numpy.clip(edge * 0.95, 0.0, 1.0)
    colour = colour * (1 - edge_alpha[..., None]) + edge_colour * edge_alpha[..., None]
    alpha = alpha * (1 - edge_alpha) + edge_alpha
    if glow > 2.0:
        # The burst: the eye itself flaring ember orange (never white)
        colour = colour + inside[..., None] * numpy.array([0.35, 0.15, 0.02]) * (glow - 2.0)
    # Clear at the frame's edges, whatever the glow does there
    border = smooth(0.0, 0.06, numpy.minimum.reduce([x / width + 0.5, 0.5 - x / width, y / height + 0.5,
                                                      0.5 - y / height]))
    colour *= border[..., None]
    alpha *= border
    return numpy.concatenate([numpy.clip(colour, 0.0, 1.0), numpy.clip(alpha, 0.0, 1.0)[..., None]], axis=2)


def gaze_frames():
    """(opening, heat, glow) of each frame"""
    frames = []
    for index in range(GAZE_OPENING):
        t = index / (GAZE_OPENING - 1)
        # Slow at first, the lids parting faster mid-way, settling wide open
        opening = smooth(0.04, 0.92, t) * 0.97 + 0.03 * t
        frames.append((opening, smooth(0.0, 1.0, t), 0.15 + 0.7 * t))
    frames += [(1.0, 1.0, glow) for glow in GAZE_BREATHING]
    frames.append((1.0, 1.0, GAZE_BURST))
    return frames


def gaze_atlas():
    """The gaze's frames, 8 x 8 of 256 x 128, row by row (RGBA)"""
    source = eye_source()
    width, height = GAZE_FRAME
    frames = gaze_frames()
    assert len(frames) == GAZE_GRID * GAZE_GRID
    atlas = Image.new('RGBA', (width * GAZE_GRID, height * GAZE_GRID))
    for index, (opening, heat, glow) in enumerate(frames):
        big = gaze_frame(source, opening, heat, glow, width * GAZE_SUPERSAMPLE, height * GAZE_SUPERSAMPLE)
        # Shrunk on premultiplied colour: the soft edges keep their colour
        small = numpy.stack([as_float(to_l(big[..., c]).resize((width, height), Image.LANCZOS)) for c in range(4)],
                            axis=2)
        small[..., :3] = numpy.where(small[..., 3:4] > 1e-4, small[..., :3] / numpy.maximum(small[..., 3:4], 1e-4),
                                     0.0)
        tile = Image.fromarray(numpy.clip(small * 255.0 + 0.5, 0, 255).astype(numpy.uint8), 'RGBA')
        atlas.paste(tile, ((index % GAZE_GRID) * width, (index // GAZE_GRID) * height))
    return atlas


def gaze_timeline():
    """VW_Gaze's timeline (shapes.json): the opening over 5 s, the glow breathing faster to 6 s (up two, back one,
    each swap quicker), the burst"""
    keys = [[int(round(5000 * index / GAZE_OPENING)), index] for index in range(GAZE_OPENING)]
    steps = []
    for level in range(len(GAZE_BREATHING) - 1):
        steps += [level + 1, level]
    time, period = 5000.0, 150.0
    for step in steps:
        if time >= 5960:
            break
        keys.append([int(time), GAZE_OPENING + step])
        time += period
        period = max(45.0, period * 0.88)
    keys.append([6000, GAZE_OPENING + len(GAZE_BREATHING)])
    return keys


# --- The other paintings ------------------------------------------------------------------------------------------

def cell_iron():
    return painting('cell_iron.png')


def cell_runes():
    return painting('cell_runes.png')


def rollcall_mark():
    return painting('rollcall_mark.png')


def wall_band(frame):
    return painting(f'wall_band_{frame}.png')


def shockwave_ring():
    """The wave that raises the wall, at its full size (the model grows it from the middle): a front of fel fire at
    its edge, dim and broken, and a band of dark iron ash just behind it - no white, nothing that flashes. 1024 x 1024,
    RGBA, the front's outside at the picture's edge."""
    size = 1024
    _, _, radius = grid(size, size)
    broken = 0.55 + 0.45 * noise(size, size, 18, 61)
    fine = noise(size, size, 5, 62)
    # The front: a soft band peaking just inside the edge, its outer side the sharper
    front = numpy.where(radius < 0.95, numpy.exp(-((radius - 0.95) / 0.05) ** 2),
                        numpy.exp(-((radius - 0.95) / 0.02) ** 2)) * broken
    flames = numpy.clip(fine - 0.55, 0.0, 1.0) * 2.0 * smooth(0.80, 0.93, radius) * smooth(1.0, 0.96, radius)
    glow = numpy.clip(front * 0.8 + flames * 0.5, 0.0, 1.0)
    # The ash behind it: darkening the floor, fading out towards the middle
    ash = smooth(0.55, 0.85, radius) * smooth(0.97, 0.9, radius) * (0.6 + 0.4 * noise(size, size, 10, 63)) * 0.55
    cracks = numpy.clip(noise(size, size, 3, 64) - 0.8, 0.0, 1.0) * 3.0 * smooth(0.6, 0.85, radius) * \
        smooth(0.95, 0.88, radius)
    fel = numpy.array([0.30, 0.72, 0.12])
    iron = numpy.array([0.05, 0.045, 0.04])
    alpha = numpy.clip(glow * 0.8 + ash * (1 - glow) + cracks * 0.3, 0.0, 0.85)
    weight = numpy.where(alpha > 1e-4, (glow * 0.8 + cracks * 0.3) / numpy.maximum(alpha, 1e-4), 0.0)
    colour = iron * (1 - numpy.clip(weight, 0.0, 1.0))[..., None] + fel * numpy.clip(weight, 0.0, 1.0)[..., None]
    alpha *= smooth(1.0, 0.985, radius)
    rgba = numpy.concatenate([colour, alpha[..., None]], axis=2)
    return Image.fromarray(numpy.clip(rgba * 255.0 + 0.5, 0, 255).astype(numpy.uint8), 'RGBA')


# --- The isolation: the blow that throws his tank, and the seal's burst ---------------------------------------------

STRIKE_ANGLE = 44.0


def strike_cone():
    """The blow, from his fist out (a cone of STRIKE_ANGLE degrees, its apex at the middle of the left edge): streaks of
    ember fire driven outwards, hottest at his fist, and the blow's front, a broken arc of fire near its end. Nothing
    white. 1024 x 1024, RGBA."""
    size = 1024
    xs = (numpy.arange(size) + 0.5) / size
    ys = (numpy.arange(size) + 0.5) / size * 2.0 - 1.0
    x, y = numpy.meshgrid(xs, ys)
    radius = numpy.hypot(x, y * 0.5)
    angle = numpy.degrees(numpy.arctan2(y * 0.5, x))
    half = STRIKE_ANGLE / 2.0
    across = numpy.clip(numpy.abs(angle) / half, 0.0, 2.0)
    inside = smooth(1.0, 0.7, across) * smooth(1.0, 0.94, radius)
    # Streaks along the blow: noise stretched along the radius (sampled on angle and a slow radius)
    rng = numpy.random.default_rng(71)
    lanes = rng.random(40)
    lane = numpy.interp((angle + half * 1.2) / (half * 2.4) * 39.0, numpy.arange(40), lanes)
    lane = lane * lane * (3.0 - 2.0 * lane)
    streaks = numpy.clip(lane * 1.4 - 0.3, 0.0, 1.0) * (0.5 + 0.5 * noise(size, size, 60, 72))
    heat = smooth(0.95, 0.05, radius)
    front = numpy.exp(-((radius - 0.82) / 0.08) ** 2) * (0.6 + 0.4 * noise(size, size, 14, 73))
    # A body of fire over the whole blow, its streaks and its front on it: seen as a blast, not a spark at the fist
    body = 0.45 + 0.25 * noise(size, size, 30, 74)
    glow = numpy.clip(body + streaks * 0.6 + front + smooth(0.25, 0.0, radius) * 0.5, 0.0, 1.0)
    glow *= inside
    hot = numpy.array([1.0, 0.62, 0.18])
    deep = numpy.array([0.75, 0.10, 0.03])
    mix = numpy.clip(heat * 0.8 + front * 0.6, 0.0, 1.0)[..., None]
    colour = deep * (1 - mix) + hot * mix
    alpha = numpy.clip(glow * 0.95, 0.0, 0.9)
    rgba = numpy.concatenate([colour, alpha[..., None]], axis=2)
    return Image.fromarray(numpy.clip(rgba * 255.0 + 0.5, 0, 255).astype(numpy.uint8), 'RGBA')


def push_trail():
    """Where the thrown tank skidded: two broad scorched furrows along the line (from its start, left, to where it
    landed, right) over a dark burnt band, embers burning in them, brightest where the blow struck. 1024 x 192, RGBA."""
    width, height = 1024, 192
    xs = (numpy.arange(width) + 0.5) / width
    ys = (numpy.arange(height) + 0.5) / height * 2.0 - 1.0
    x, y = numpy.meshgrid(xs, ys)
    wobble = (noise(width, height, 160, 81) - 0.5) * 0.12
    furrows = numpy.zeros((height, width))
    for middle in (-0.38, 0.38):
        furrows = numpy.maximum(furrows, numpy.exp(-((y - middle - wobble) / 0.24) ** 2))
    band = numpy.exp(-(y / 0.7) ** 2) * 0.55
    ends = smooth(0.0, 0.05, x) * smooth(1.0, 0.82, x)
    scorch = numpy.maximum(furrows, band) * ends * (0.65 + 0.35 * noise(width, height, 12, 82))
    embers = numpy.clip(noise(width, height, 4, 83) - 0.5, 0.0, 1.0) * 3.0 * furrows * smooth(1.0, 0.0, x)
    hot = smooth(0.6, 0.0, x) * furrows
    glow = numpy.clip(embers + hot * 0.9, 0.0, 1.0) * ends
    ash = numpy.array([0.06, 0.04, 0.03])
    ember = numpy.array([1.0, 0.42, 0.08])
    alpha = numpy.clip(scorch * 0.85 + glow * 0.8, 0.0, 0.92)
    weight = numpy.where(alpha > 1e-4, glow * 0.8 / numpy.maximum(alpha, 1e-4), 0.0)
    weight = numpy.clip(weight, 0.0, 1.0)[..., None]
    colour = ash * (1 - weight) + ember * weight
    rgba = numpy.concatenate([colour, alpha[..., None]], axis=2)
    return Image.fromarray(numpy.clip(rgba * 255.0 + 0.5, 0, 255).astype(numpy.uint8), 'RGBA')


# The seal's burst is drawn 40 yards wide (shapes.json VW_SealBurst scale): its lethal ring at 16 of them
SEAL_LETHAL_SHARE = 16.0 / 40.0


def seal_burst():
    """The seal going off round the isolated tank, at its full size (the model grows it from the middle): a broad
    front of crimson fire racing out to the room's walls, and inside it the lethal ring - a hard, bright band of the
    seal's broken runes at SEAL_LETHAL_SHARE of the radius, the floor scorched black within it. No white.
    1024 x 1024, RGBA, the front's outside at the picture's edge."""
    size = 1024
    x, y, radius = grid(size, size)
    angle = numpy.arctan2(y, x)
    broken = 0.55 + 0.45 * noise(size, size, 20, 91)
    front = numpy.where(radius < 0.93, numpy.exp(-((radius - 0.93) / 0.07) ** 2),
                        numpy.exp(-((radius - 0.93) / 0.025) ** 2)) * broken
    lethal = SEAL_LETHAL_SHARE
    # The wave between the lethal ring and the front: a red haze thinning outwards
    haze = smooth(0.95, lethal, radius) * smooth(lethal - 0.02, lethal + 0.06, radius) * \
        (0.35 + 0.25 * noise(size, size, 24, 95))
    links = (numpy.cos(angle * 18.0) > -0.6).astype(float)
    ring = numpy.exp(-((radius - lethal) / 0.022) ** 2) * (0.75 + 0.25 * links)
    runes = numpy.exp(-((radius - lethal * 0.78) / 0.012) ** 2) * links * (0.5 + 0.5 * noise(size, size, 6, 92))
    scorch = smooth(lethal, lethal - 0.05, radius) * (0.6 + 0.4 * noise(size, size, 16, 94)) * 0.75
    cracks = numpy.clip(noise(size, size, 3, 93) - 0.75, 0.0, 1.0) * 3.5 * smooth(lethal, lethal * 0.3, radius)
    glow = numpy.clip(front * 0.9 + ring + runes * 0.8 + cracks * 0.5 + haze, 0.0, 1.0)
    crimson = numpy.array([0.85, 0.07, 0.05])
    ember = numpy.array([1.0, 0.5, 0.12])
    char = numpy.array([0.04, 0.02, 0.02])
    alpha = numpy.clip(glow * 0.9 + scorch * (1 - glow), 0.0, 0.92)
    weight = numpy.clip(numpy.where(alpha > 1e-4, glow * 0.9 / numpy.maximum(alpha, 1e-4), 0.0), 0.0, 1.0)
    hue = numpy.clip(front + ring * 0.6, 0.0, 1.0)[..., None]
    lit = crimson * (1 - hue) + ember * hue
    colour = char * (1 - weight[..., None]) + lit * weight[..., None]
    alpha *= smooth(1.0, 0.985, radius)
    rgba = numpy.concatenate([colour, alpha[..., None]], axis=2)
    return Image.fromarray(numpy.clip(rgba * 255.0 + 0.5, 0, 255).astype(numpy.uint8), 'RGBA')


# --- Coup de hache: the line his axe splits the floor along ---------------------------------------------------------

def axe_line(hit, long=False):
    """His axe's line, the whole of it (four pieces of the line drawn in a chain, shapes.json VW_Axe*): along it (left,
    where he stands, to right) a crack in the floor. Warning: the crack glowing, its edges burnt dark red, embers rising
    from it - the line's whole width marked, faintly, to its borders. Blow (hit): the crack torn open, fire bursting
    from it across the width. On black (the builder keys it to clear). 2048 x 512, RGB; long: 4096 x 512, the line
    drawn as one model 8 times as long as it is wide (VW_AxeSwing*)."""
    width, height = (4096 if long else 2048), 512
    xs = (numpy.arange(width) + 0.5) / width
    ys = (numpy.arange(height) + 0.5) / height * 2.0 - 1.0
    x, y = numpy.meshgrid(xs, ys)
    rng = numpy.random.default_rng(121)
    # The crack wanders along the line's middle, a zigzag of short straight runs
    knots = numpy.linspace(0.0, 1.0, 40)
    offsets = rng.uniform(-0.18, 0.18, knots.size)
    middle = numpy.interp(x, knots, offsets)
    crack = numpy.exp(-((y - middle) / 0.035) ** 2)
    rim = numpy.exp(-((y - middle) / 0.16) ** 2)
    ends = smooth(0.0, 0.03, x) * smooth(1.0, 0.97, x)
    border = (numpy.exp(-((numpy.abs(y) - 0.92) / 0.03) ** 2)) * 0.45
    fill = 0.12 * smooth(1.0, 0.85, numpy.abs(y))
    embers = numpy.clip(noise(width, height, 6, 122) - 0.62, 0.0, 1.0) * 2.5 * numpy.exp(-((y - middle) / 0.4) ** 2)
    if hit:
        flames = numpy.clip(noise(width, height, 18, 123) * 1.3 - 0.25, 0.0, 1.0) * smooth(0.95, 0.2, numpy.abs(y))
        heat = numpy.clip(crack * 1.2 + rim * 0.8 + flames * 0.9 + embers, 0.0, 1.0)
        colour = (numpy.array([1.0, 0.62, 0.2]) * (crack + rim * 0.5)[..., None] +
                  numpy.array([0.95, 0.22, 0.05]) * (flames + embers)[..., None])
        colour = colour * ends[..., None] + numpy.array([0.5, 0.05, 0.02]) * (border + fill * 2.0)[..., None]
        colour = numpy.clip(colour * (0.4 + 0.6 * heat[..., None]), 0.0, 1.0)
    else:
        colour = (numpy.array([1.0, 0.45, 0.1]) * (crack * 0.85)[..., None] +
                  numpy.array([0.7, 0.08, 0.03]) * (rim * 0.45 + embers * 0.7)[..., None]) * ends[..., None]
        colour = colour + numpy.array([0.6, 0.06, 0.03]) * (border + fill)[..., None]
        colour = numpy.clip(colour, 0.0, 1.0)
    rgb = numpy.clip(colour * 255.0 + 0.5, 0, 255).astype(numpy.uint8)
    return Image.fromarray(rgb, 'RGB')


# --- The cells' bars and the curfew --------------------------------------------------------------------------------

def cell_bars():
    """One side of a cell's cage (a curtain piece, one of 16 round its 3 yards: 1.17 yards wide): two bars of red-hot
    iron and the two rails holding them, glowing on black (the builder keys black to clear), hottest at the floor.
    128 x 512, RGB."""
    width, height = 128, 512
    xs = (numpy.arange(width) + 0.5) / width
    ys = (numpy.arange(height) + 0.5) / height
    x, y = numpy.meshgrid(xs, ys)
    bars = numpy.zeros((height, width))
    for middle in (0.25, 0.75):
        bars = numpy.maximum(bars, numpy.exp(-((x - middle) / 0.06) ** 2))
    rails = numpy.zeros((height, width))
    for middle in (0.06, 0.94):
        rails = numpy.maximum(rails, numpy.exp(-((y - middle) / 0.018) ** 2))
    iron = numpy.maximum(bars, rails)
    heat = 0.55 + 0.45 * y
    glow = numpy.clip(iron * heat * (0.8 + 0.2 * noise(width, height, 24, 101)), 0.0, 1.0)
    halo = numpy.clip(blur(iron, width * 0.05) * 0.35, 0.0, 1.0) * heat
    core = numpy.array([1.0, 0.55, 0.15])
    edge = numpy.array([0.75, 0.12, 0.04])
    colour = core * (glow ** 1.5)[..., None] + edge * numpy.clip(glow + halo, 0.0, 1.0)[..., None] * 0.6
    rgb = numpy.clip(colour * 255.0 + 0.5, 0, 255).astype(numpy.uint8)
    return Image.fromarray(rgb, 'RGB')


def curfew_ring():
    """The curfew round the whole room, at its full size: a clock's dial at the edge - twelve heavy hour bars, minute
    ticks, a rim - burning red, and a faint red haze inside it. The model breathes it faster and faster until the
    bell. 1024 x 1024, RGBA. The painting art/curfew_ring.png takes its place once it is there (on pure black: the
    builder keys it, shapes.json VW_CurfewRing `keyed`)."""
    if os.path.exists(os.path.join(ART, 'curfew_ring.png')):
        return painting('curfew_ring.png')
    size = 1024
    x, y, radius = grid(size, size)
    angle = numpy.arctan2(y, x)
    rim = numpy.exp(-((radius - 0.95) / 0.012) ** 2) + numpy.exp(-((radius - 0.80) / 0.008) ** 2) * 0.7
    hour = numpy.abs(numpy.remainder(angle / (2.0 * numpy.pi) * 12.0 + 0.5, 1.0) - 0.5)
    bars = smooth(0.06, 0.03, hour) * smooth(0.79, 0.82, radius) * smooth(0.95, 0.92, radius)
    minute = numpy.abs(numpy.remainder(angle / (2.0 * numpy.pi) * 60.0 + 0.5, 1.0) - 0.5)
    ticks = smooth(0.12, 0.06, minute) * smooth(0.88, 0.9, radius) * smooth(0.95, 0.93, radius) * 0.6
    dial = numpy.clip(rim + bars + ticks, 0.0, 1.0)
    haze = smooth(0.2, 0.8, radius) * smooth(0.95, 0.8, radius) * 0.18
    glow = numpy.clip(blur(dial, size * 0.006) * 1.4, 0.0, 1.0)
    red = numpy.array([0.9, 0.12, 0.05])
    hot = numpy.array([1.0, 0.5, 0.15])
    alpha = numpy.clip(glow * 0.95 + haze, 0.0, 0.95)
    colour = red * numpy.ones((size, size, 1)) * (1 - dial[..., None]) + hot * dial[..., None]
    alpha *= smooth(1.0, 0.985, radius)
    rgba = numpy.concatenate([colour, alpha[..., None]], axis=2)
    return Image.fromarray(numpy.clip(rgba * 255.0 + 0.5, 0, 255).astype(numpy.uint8), 'RGBA')


def curfew_mark():
    """Over each head while the curfew is called: an hourglass in the numbers' style (ember fill, black-iron edge, red
    glow). 256 x 256, RGBA. The painting art/curfew_mark.png (on pure black) takes its place once it is there."""
    if os.path.exists(os.path.join(ART, 'curfew_mark.png')):
        rgb = as_array(painting('curfew_mark.png'))[..., :3]
        alpha = numpy.clip(rgb.max(axis=2) / 0.12, 0.0, 1.0)
        colour = numpy.where(alpha[..., None] > 1e-4, rgb / numpy.maximum(alpha[..., None], 1e-4), 0.0)
        rgba = numpy.concatenate([numpy.clip(colour, 0.0, 1.0), alpha[..., None]], axis=2)
        return Image.fromarray(numpy.clip(rgba * 255.0 + 0.5, 0, 255).astype(numpy.uint8), 'RGBA')
    width = height = 256
    scale = 4
    big = width * scale

    def mask(grow):
        image = Image.new('L', (big, big))
        draw = ImageDraw.Draw(image)
        g = grow * scale
        top, bottom, left, right, middle = 0.16 * big, 0.84 * big, 0.28 * big, 0.72 * big, 0.5 * big
        neck = 0.05 * big
        draw.polygon([(left - g, top + 0.06 * big - g), (right + g, top + 0.06 * big - g),
                      (middle + neck + g, middle), (middle - neck - g, middle)], fill=255)
        draw.polygon([(middle - neck - g, middle), (middle + neck + g, middle),
                      (right + g, bottom - 0.06 * big + g), (left - g, bottom - 0.06 * big + g)], fill=255)
        for row in (top, bottom - 0.06 * big):
            draw.rectangle([left - 0.06 * big - g, row - g, right + 0.06 * big + g, row + 0.06 * big + g], fill=255)
        return as_float(image.resize((width, height), Image.LANCZOS))

    fill = mask(0)
    edge = mask(6)
    halo = numpy.clip(blur(edge, width * 0.06) * 1.6, 0.0, 1.0)
    rows = numpy.linspace(0.0, 1.0, height)[:, None, None]
    ember = EMBER_TOP * (1 - rows) + EMBER_BOTTOM * rows
    colour = GLOW * numpy.ones((height, width, 1))
    alpha = halo * 0.85
    colour = colour * (1 - edge[..., None]) + IRON * edge[..., None]
    alpha = alpha * (1 - edge) + edge
    colour = colour * (1 - fill[..., None]) + ember * fill[..., None]
    rgba = numpy.concatenate([colour, alpha[..., None]], axis=2)
    return Image.fromarray(numpy.clip(rgba * 255.0 + 0.5, 0, 255).astype(numpy.uint8), 'RGBA')


# --- Placeholders: drawn here until the paintings are in art/ ------------------------------------------------------

def iron_texture(width, height, seed):
    """Worn black iron: dark grey, mottled, a few scratches of lighter grey"""
    mottled = 0.10 + 0.08 * noise(width, height, 24, seed) + 0.05 * noise(width, height, 6, seed + 1)
    return numpy.stack([mottled * 1.05, mottled, mottled * 0.95], axis=2)


def rivets(width, height, centres, radius):
    """Round rivet heads lit from the top left: (shade, coverage)"""
    ys, xs = numpy.mgrid[0:height, 0:width]
    shade = numpy.zeros((height, width))
    cover = numpy.zeros((height, width))
    for cx, cy in centres:
        d = numpy.hypot(xs - cx, ys - cy) / radius
        inside = smooth(1.0, 0.85, d)
        light = numpy.clip(1.0 - numpy.hypot(xs - cx + radius * 0.35, ys - cy + radius * 0.35) / radius, 0.0, 1.0)
        shade = numpy.maximum(shade, inside * (0.18 + 0.35 * light))
        cover = numpy.maximum(cover, inside)
    return shade, cover


def placeholder_gaze(width, height, opening):
    """A demonic eye in a keyhole-shaped iron plate, its iron lids `opening` (0 closed, 1 open)"""
    x, y, _ = grid(width, height)
    iron = iron_texture(width, height, 7)
    # The keyhole: a round top over a tapering foot
    top = numpy.hypot(x, (y + 0.25) * 1.0) < 0.62
    foot = (numpy.abs(x) < 0.22 + 0.18 * (y + 0.1)) & (y > -0.1) & (y < 0.85)
    plate = (top | foot).astype(numpy.float64)
    plate = blur(plate, width * 0.004)
    rim = numpy.clip(plate - blur(plate, width * 0.02) * 0.9, 0.0, 1.0) * 2.0
    image = iron * plate[..., None] + rim[..., None] * 0.12
    # Bars and rivets round the plate
    bars = ((numpy.abs(y + 0.25) < 0.05) & (numpy.abs(x) > 0.55) & (numpy.abs(x) < 0.95)).astype(float)
    image += iron * bars[..., None] * 1.3
    centres = [(width * (0.5 + 0.48 * math.cos(a)), height * (0.375 + 0.48 * math.sin(a)))
               for a in numpy.linspace(math.pi * 0.9, math.pi * 2.1, 7)]
    shade, cover = rivets(width, height, centres, width * 0.028)
    image = image * (1 - cover[..., None]) + shade[..., None]
    # The eye: an orange-red iris, a slit pupil, its glow growing with the opening
    ex, ey = x / 0.42, (y + 0.25) / 0.30
    eye = numpy.hypot(ex, ey)
    heat = 0.35 + 0.65 * opening
    iris = numpy.clip(1.2 - eye, 0.0, 1.0)
    colour = numpy.stack([numpy.full_like(iris, 0.9 * heat + 0.1), 0.25 * heat * iris + 0.05,
                          0.03 + 0.25 * iris ** 3 * opening], axis=2) * smooth(1.05, 0.9, eye)[..., None]
    veins = numpy.clip(noise(width, height, 5, 3) - 0.6, 0.0, 1.0) * 2.5 * smooth(1.0, 0.6, eye)
    colour += veins[..., None] * numpy.array([0.5, 0.05, 0.0]) * opening
    slit = (0.035 + 0.04 * opening) * numpy.clip(1.0 - ey ** 2, 0.0, 1.0)
    pupil = smooth(slit + 0.012, slit, numpy.abs(x))
    colour *= (1 - 0.92 * pupil)[..., None]
    # The lids: iron shutters over the eye, a thin red line where they meet
    opened_height = max(0.02, opening)
    lids = numpy.abs(ey) > opened_height * numpy.sqrt(numpy.clip(1 - ex ** 2, 0.0, 1.0))
    lids = blur(lids.astype(float), width * 0.003) * smooth(1.1, 0.95, eye)
    lid_iron = iron * 1.4 + numpy.array([0.04, 0.0, 0.0])
    image = image * (1 - smooth(1.1, 0.95, eye)[..., None]) + \
        (colour * (1 - lids[..., None]) + lid_iron * lids[..., None]) * smooth(1.1, 0.95, eye)[..., None]
    seam = numpy.exp(-(ey / 0.04) ** 2) * smooth(1.0, 0.8, numpy.abs(ex)) * (1 - opening) * 0.9
    image += seam[..., None] * numpy.array([1.0, 0.15, 0.03])
    # Cracks glowing red on the plate as it opens
    cracks = numpy.clip(noise(width, height, 3, 11) - 0.78, 0.0, 1.0) * 4.0 * plate * opening
    image += cracks[..., None] * numpy.array([0.9, 0.2, 0.03])
    # Fel green at the plate's rim (the citadel's glow)
    image += (rim * 0.5)[..., None] * numpy.array([0.15, 0.6, 0.05])
    return to_image(numpy.clip(image, 0.0, 1.0))


def placeholder_cell_iron(width, height):
    """A ring of black-iron bars flat on the floor, rivets and links between them, its middle empty"""
    x, y, radius = grid(width, height)
    angle = numpy.arctan2(y, x)
    iron = iron_texture(width, height, 21)
    band = smooth(1.0, 0.97, radius) * smooth(0.80, 0.83, radius)
    bars = 16
    segment = numpy.abs(((angle / (2 * math.pi) * bars) % 1.0) - 0.5)
    bar = band * smooth(0.36, 0.30, segment)
    link = band * (1 - smooth(0.36, 0.30, segment)) * smooth(0.04, 0.02, numpy.abs(radius - 0.9))
    image = iron * (bar * 1.6 + link * 1.1)[..., None]
    # Light along each bar's top edge
    image += (bar * smooth(0.95, 0.99, radius) * 0.12)[..., None]
    centres = [(width * (0.5 + 0.45 * math.cos(a)), height * (0.5 + 0.45 * math.sin(a)))
               for a in numpy.arange(bars) * 2 * math.pi / bars]
    shade, cover = rivets(width, height, centres, width * 0.016)
    image = image * (1 - cover[..., None]) + shade[..., None]
    # A faint fel glint between the bars
    glint = band * (1 - smooth(0.36, 0.30, segment)) * 0.35 * noise(width, height, 8, 5)
    image += glint[..., None] * numpy.array([0.2, 0.8, 0.1])
    return to_image(numpy.clip(image, 0.0, 1.0))


def placeholder_cell_runes(width, height):
    """A thin ring of fel-green runes, 70% of the picture across"""
    x, y, radius = grid(width, height)
    angle = numpy.arctan2(y, x)
    rng = numpy.random.default_rng(31)
    image = Image.new('L', (width, height))
    draw = ImageDraw.Draw(image)
    runes = 24
    ring = 0.70
    for k in range(runes):
        a = k * 2 * math.pi / runes
        cx, cy = width / 2 * (1 + ring * math.cos(a)), height / 2 * (1 + ring * math.sin(a))
        size = width * 0.022
        # A rune: a few strokes on a small grid round its centre, turned to the ring
        for _ in range(3):
            points = rng.integers(-1, 2, size=(2, 2)) * size
            ca, sa = math.cos(a), math.sin(a)
            (x0, y0), (x1, y1) = points
            draw.line([(cx + x0 * ca - y0 * sa, cy + x0 * sa + y0 * ca),
                       (cx + x1 * ca - y1 * sa, cy + x1 * sa + y1 * ca)], fill=255, width=max(2, width // 220))
    strokes = as_float(image)
    line = smooth(0.006, 0.0, numpy.abs(radius - ring + 0.035)) + smooth(0.006, 0.0, numpy.abs(radius - ring - 0.035))
    line *= 0.6 + 0.4 * numpy.cos(angle * runes) ** 2
    core = numpy.clip(strokes + line * 0.8, 0.0, 1.0)
    glow = blur(core, width * 0.01) * 1.6
    image = core[..., None] * numpy.array([0.75, 1.0, 0.45]) + glow[..., None] * numpy.array([0.15, 0.85, 0.08])
    return to_image(numpy.clip(image, 0.0, 1.0))


def placeholder_rollcall_mark(width, height):
    """A round iron plaque: a thick rim with four rivets, an ember-red glow along it, a flat dark middle"""
    x, y, radius = grid(width, height)
    iron = iron_texture(width, height, 41)
    rim = smooth(1.0, 0.96, radius) * smooth(0.74, 0.78, radius)
    middle = smooth(0.78, 0.74, radius)
    image = iron * (rim * 1.5 + middle * 0.75)[..., None]
    glow = numpy.exp(-((radius - 0.76) / 0.03) ** 2) + numpy.exp(-((radius - 0.97) / 0.02) ** 2) * 0.6
    image += glow[..., None] * numpy.array([0.85, 0.18, 0.04])
    centres = [(width * (0.5 + 0.43 * math.cos(a)), height * (0.5 + 0.43 * math.sin(a)))
               for a in (math.pi / 4, 3 * math.pi / 4, 5 * math.pi / 4, 7 * math.pi / 4)]
    shade, cover = rivets(width, height, centres, width * 0.04)
    image = image * (1 - cover[..., None]) + shade[..., None]
    return to_image(numpy.clip(image, 0.0, 1.0))


def bolt(draw, start, end, width, rng, depth, wrap):
    """Fel lightning between two points: midpoint displacement, drawn wrapped across the band's sides"""
    (x0, y0), (x1, y1) = start, end
    if depth == 0:
        for shift in (-wrap, 0, wrap):
            draw.line([(x0 + shift, y0), (x1 + shift, y1)], fill=255, width=width)
        return
    mx = (x0 + x1) / 2 + rng.normal() * abs(y1 - y0 + x1 - x0) * 0.18
    my = (y0 + y1) / 2 + rng.normal() * abs(x1 - x0 + y1 - y0) * 0.12
    bolt(draw, (x0, y0), (mx, my), width, rng, depth - 1, wrap)
    bolt(draw, (mx, my), (x1, y1), width, rng, depth - 1, wrap)


def placeholder_wall_band(width, height, frame):
    """Fel lightning between black-iron bars, brightest at the bottom; the bars the same in every frame"""
    rng = numpy.random.default_rng(100 + frame)
    image = Image.new('L', (width, height))
    draw = ImageDraw.Draw(image)
    for _ in range(14):
        x = rng.uniform(0, width)
        bolt(draw, (x, height), (x + rng.uniform(-width * 0.12, width * 0.12), rng.uniform(0, height * 0.6)),
             max(2, width // 400), rng, 6, width)
    for _ in range(8):
        y = rng.uniform(height * 0.45, height)
        x = rng.uniform(0, width)
        bolt(draw, (x, y), (x + rng.uniform(width * 0.1, width * 0.25), y + rng.normal() * height * 0.1),
             max(1, width // 600), rng, 5, width)
    strokes = as_float(image)
    rows = numpy.linspace(0.0, 1.0, height)[:, None]
    fade = 0.25 + 0.75 * rows ** 1.2
    floor = numpy.exp(-((1.0 - rows) / 0.08) ** 2) * (0.6 + 0.4 * noise(width, height, 16, frame))
    core = numpy.clip(strokes * fade + floor * 0.7, 0.0, 1.0)
    glow = blur(core, height * 0.03) * 1.8
    colour = core[..., None] * numpy.array([0.8, 1.0, 0.5]) + glow[..., None] * numpy.array([0.2, 0.9, 0.1])
    # Four bars across the band, the same in every frame (the band tiles side to side)
    xs = (numpy.arange(width) + 0.5) / width
    bar = smooth(0.022, 0.016, numpy.abs(((xs * 4) % 1.0) - 0.5) / 4)[None, :] * numpy.ones((height, 1))
    iron = iron_texture(width, height, 51) * 1.6
    colour = colour * (1 - bar[..., None]) + iron * bar[..., None]
    return to_image(numpy.clip(colour, 0.0, 1.0))


PLACEHOLDERS = {
    'gaze_closed': lambda w, h: placeholder_gaze(w, h, 0.0),
    'gaze_half': lambda w, h: placeholder_gaze(w, h, 0.5),
    'gaze_open': lambda w, h: placeholder_gaze(w, h, 1.0),
    'cell_iron': placeholder_cell_iron,
    'cell_runes': placeholder_cell_runes,
    'rollcall_mark': placeholder_rollcall_mark,
    'wall_band_1': lambda w, h: placeholder_wall_band(w, h, 1),
    'wall_band_2': lambda w, h: placeholder_wall_band(w, h, 2),
    'wall_band_3': lambda w, h: placeholder_wall_band(w, h, 3),
    'wall_band_4': lambda w, h: placeholder_wall_band(w, h, 4),
}
