"""Gardien-chef Vorhan's painted marks, as the ground indicator builder draws them (localTools/groundIndicators,
shapes.json `picture`: "localTools/wardenVorhan/indicatorArt.py:<function>").

Each function returns the picture of one shape. The paintings are read from art/ (the brief:
.agents/plans/warden-vorhan/warden-vorhan.ASSETS.md); until one is there, a placeholder drawn here takes its place, so
the fight can be tested, and the next build picks the painting up by itself. A painting is never stretched: it is
resized at its own aspect only, and one painted at another aspect than the brief's stops the build.

- The gaze: an atlas of frames (4 x 4) built from three key paintings (closed, half open, open): the eyelids opening
  over the eye by masks and blends, the red glow growing, breathing on the last frames, and the burst.
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


def keyed(rgb, knee):
    """Black keyed to transparent: alpha from brightness (full from `knee` up), colour unpremultiplied. Dark iron
    stays as dark iron over the scene rather than vanishing as it does added to it."""
    alpha = numpy.clip(rgb.max(axis=2, keepdims=True) / knee, 0.0, 1.0)
    colour = numpy.where(alpha > 1e-4, rgb / numpy.maximum(alpha, 1e-4), 0.0)
    return numpy.concatenate([numpy.clip(colour, 0.0, 1.0), alpha], axis=2)


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

GAZE_FRAME = 256
GAZE_GRID = 4
# The opening, frame by frame: (stage, t, glow). Stage 0 the closed eye, 1 closed to half open, 2 half to open,
# 3 open; glow the red bloom added over it. Frames 12 to 14 are the breathing (shapes.json VW_Gaze's timeline
# alternates them faster and faster), 15 the burst.
GAZE_FRAMES = [
    (0, 0.0, 0.00), (0, 0.0, 0.10), (0, 0.0, 0.22),
    (1, 0.2, 0.25), (1, 0.4, 0.30), (1, 0.6, 0.36), (1, 0.8, 0.42), (1, 1.0, 0.48),
    (2, 0.25, 0.55), (2, 0.5, 0.62), (2, 0.75, 0.70), (2, 1.0, 0.78),
    (3, 1.0, 0.70), (3, 1.0, 0.95), (3, 1.0, 1.20),
    (3, 1.0, 2.20),
]
# A frame's border fades to clear over this share of it: the atlas's neighbours never bleed into one another
GAZE_BORDER = 0.035
# Alpha keying of the gaze (its black-iron plate reads as iron, not as a faint glow)
GAZE_KNEE = 0.15


def eye_region(closed, opened):
    """The eye: where the open painting differs most from the closed one. (cx, cy, rx, ry) in pixels."""
    difference = blur(numpy.abs(opened - closed).max(axis=2), closed.shape[1] * 0.01)
    weights = numpy.clip(difference - 0.35 * difference.max(), 0.0, None)
    ys, xs = numpy.mgrid[0:closed.shape[0], 0:closed.shape[1]]
    total = weights.sum()
    cx, cy = (weights * xs).sum() / total, (weights * ys).sum() / total
    lit = weights > 0
    rx = max(8.0, (numpy.percentile(xs[lit], 97) - numpy.percentile(xs[lit], 3)) / 2.0)
    ry = max(8.0, (numpy.percentile(ys[lit], 97) - numpy.percentile(ys[lit], 3)) / 2.0)
    return cx, cy, rx, ry


def lid_mask(shape, eye, opening):
    """The eye's opening between the lids: an ellipse `opening` (0-1) of the eye's height, soft edged"""
    cx, cy, rx, ry = eye
    ys, xs = numpy.mgrid[0:shape[0], 0:shape[1]]
    height = max(opening, 1e-3) * ry
    # A lens: the lids meet at the corners, open most at the middle
    across = numpy.clip(1.0 - ((xs - cx) / rx) ** 2, 0.0, 1.0)
    inside = numpy.abs(ys - cy) - height * numpy.sqrt(across)
    return smooth(2.5, -2.5, inside) * (opening > 0)


def bloom(frame, eye, amount):
    """Red glow from the eye's bright parts, spreading wider as it grows"""
    if amount <= 0:
        return frame
    cx, cy, rx, ry = eye
    hot = numpy.clip(frame[..., 0] - 0.55, 0.0, 1.0) * (frame[..., 0] > frame[..., 2])
    spread = blur(hot, frame.shape[1] * (0.02 + 0.03 * amount))
    _, _, radius = grid(frame.shape[1], frame.shape[0])
    ys, xs = numpy.mgrid[0:frame.shape[0], 0:frame.shape[1]]
    around = numpy.exp(-(((xs - cx) / (rx * (1.4 + amount))) ** 2 + ((ys - cy) / (ry * (1.6 + amount))) ** 2))
    glow = numpy.clip(spread * 2.2 * amount + around * 0.35 * amount, 0.0, 1.5)
    out = frame + glow[..., None] * numpy.array([1.0, 0.22, 0.06])
    if amount > 1.5:
        # The burst: a white-hot core over the eye
        core = numpy.exp(-(((xs - cx) / (rx * 0.9)) ** 2 + ((ys - cy) / (ry * 0.9)) ** 2))
        out = out + core[..., None] * numpy.array([1.0, 0.75, 0.55]) * (amount - 1.5)
    return numpy.clip(out, 0.0, 1.0)


def gaze_frame(closed, half, opened, eye, stage, t, glow):
    if stage == 0:
        frame = closed
    elif stage == 1:
        # The plate's cracks spread as the lids part over the half-open eye
        base = closed * (1 - t) + half * t
        mask = lid_mask(closed.shape, eye, 0.5 * t)[..., None]
        frame = base * (1 - mask) + half * mask
    elif stage == 2:
        base = half * (1 - t) + opened * t
        mask = lid_mask(closed.shape, eye, 0.5 + 0.5 * t)[..., None]
        frame = base * (1 - mask) + opened * mask
    else:
        frame = opened
    return bloom(frame, eye, glow)


def gaze_atlas():
    """The gaze's 16 frames, 4 x 4 of 256 pixels, row by row (RGBA, alpha keyed)"""
    closed, half, opened = (as_array(painting(f'gaze_{name}.png')) for name in ('closed', 'half', 'open'))
    eye = eye_region(closed, opened)
    atlas = Image.new('RGBA', (GAZE_FRAME * GAZE_GRID, GAZE_FRAME * GAZE_GRID))
    x, y, _ = grid(GAZE_FRAME, GAZE_FRAME)
    border = smooth(1.0, 1.0 - 2 * GAZE_BORDER, numpy.maximum(numpy.abs(x), numpy.abs(y)))
    for index, (stage, t, glow) in enumerate(GAZE_FRAMES):
        frame = to_image(gaze_frame(closed, half, opened, eye, stage, t, glow))
        frame = as_array(frame.resize((GAZE_FRAME, GAZE_FRAME), Image.LANCZOS))
        rgba = keyed(frame, GAZE_KNEE)
        rgba[..., 3] *= border
        tile = Image.fromarray(numpy.clip(rgba * 255.0 + 0.5, 0, 255).astype(numpy.uint8), 'RGBA')
        atlas.paste(tile, ((index % GAZE_GRID) * GAZE_FRAME, (index // GAZE_GRID) * GAZE_FRAME))
    return atlas


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
