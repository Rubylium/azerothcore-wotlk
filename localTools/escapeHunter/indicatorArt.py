"""Le Traqueur d'évadés's painted marks, as the ground indicator builder draws them (localTools/groundIndicators,
shapes.json `picture`: "localTools/escapeHunter/indicatorArt.py:<function>"; the script's looks 94860-94868,
modules/mod-stat-growth/src/EscapeHunter.cpp Looks).

Each function returns the picture of one look. The paintings are read from art/ (the brief:
.agents/plans/escape-hunter/escape-hunter.ASSETS.md), painted on pure black; until one is there, a plain placeholder
drawn here takes its place, so the fight can be played, and the next client build picks the painting up by itself. A
painting is never stretched: one at another aspect than the brief's stops the build.

- Marks over a player (billboards): RGBA, their black made see-through here.
- Marks on the floor (images): RGB on black; the builder keys the black (shapes.json `keyed`).
"""
import math
import os

from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.path.join(HERE, 'art')

# The brief's size of each painting (width, height)
SIZES = {
    'quarry_mark.png': (256, 256),
    'prey_mark.png': (256, 256),
    'track_mark.png': (256, 256),
    'lantern.png': (256, 512),
    'rustle.png': (512, 256),
    'snare_ring.png': (1024, 1024),
    'snare_circle.png': (1024, 1024),
    'pack_sigil.png': (1024, 1024),
    'hunt_sigil.png': (1024, 1024),
}


def painting(name):
    """The painting art/<name> at the brief's size (RGB, on black), or None while it is not there"""
    path = os.path.join(ART, name)
    if not os.path.exists(path):
        return None
    width, height = SIZES[name]
    image = Image.open(path).convert('RGB')
    if abs(image.width / image.height - width / height) > 0.01:
        raise SystemExit(f'{path}: {image.width} x {image.height}, the brief asks {width} x {height} '
                         f'(never stretched: crop it to that aspect)')
    if image.size != (width, height):
        image = image.resize((width, height), Image.LANCZOS)
    return image


def keyed(image):
    """A painting on black as RGBA: its black see-through, its colours kept (an overhead mark)"""
    rgb = image.convert('RGB')
    pixels = rgb.load()
    out = Image.new('RGBA', rgb.size)
    target = out.load()
    for y in range(rgb.height):
        for x in range(rgb.width):
            r, g, b = pixels[x, y]
            alpha = min(255, max(r, g, b) * 6)
            if alpha == 0:
                target[x, y] = (0, 0, 0, 0)
                continue
            scale = 255.0 / max(alpha, 1)
            target[x, y] = (min(255, int(r * scale)), min(255, int(g * scale)), min(255, int(b * scale)), alpha)
    return out


def glow(image, radius):
    """A soft halo under what is drawn"""
    halo = image.filter(ImageFilter.GaussianBlur(radius))
    return Image.alpha_composite(halo, image)


# --- Placeholders: plain shapes in the fight's colours, readable from afar -------------------------------------------
EMBER = (255, 96, 32, 255)
AMBER = (255, 190, 70, 255)
FEL = (130, 255, 60, 255)
BLOOD = (220, 30, 30, 255)


def overhead(name, draw_placeholder):
    image = painting(name)
    if image is not None:
        return keyed(image)
    width, height = SIZES[name]
    canvas = Image.new('RGBA', (width, height), (0, 0, 0, 0))
    draw_placeholder(ImageDraw.Draw(canvas), width, height)
    return glow(canvas, 6)


def floor(name, draw_placeholder):
    image = painting(name)
    if image is not None:
        return image
    width, height = SIZES[name]
    canvas = Image.new('RGB', (width, height), (0, 0, 0))
    draw_placeholder(ImageDraw.Draw(canvas), width, height)
    return canvas


def quarry_mark():
    """L'Hallali: over a quarry - a hunter's crosshair, blood red"""
    def draw(d, w, h):
        c = w / 2
        d.ellipse((c - 90, c - 90, c + 90, c + 90), outline=BLOOD, width=14)
        d.ellipse((c - 18, c - 18, c + 18, c + 18), fill=BLOOD)
        for dx, dy in ((0, -1), (0, 1), (-1, 0), (1, 0)):
            d.line((c + dx * 60, c + dy * 60, c + dx * 120, c + dy * 120), fill=BLOOD, width=14)
    return overhead('quarry_mark.png', draw)


def prey_mark():
    """La Proie: over a player leaving traps behind - a felhound's paw print, ember orange"""
    def draw(d, w, h):
        c = w / 2
        d.ellipse((c - 55, c - 10, c + 55, c + 85), fill=EMBER)
        for x, y in ((-70, -45), (-25, -85), (25, -85), (70, -45)):
            d.ellipse((c + x - 22, c + y - 28, c + x + 22, c + y + 28), fill=EMBER)
    return overhead('prey_mark.png', draw)


def track_mark():
    """Le Pistage: over a tracked player - an eye, fel green"""
    def draw(d, w, h):
        c = w / 2
        d.ellipse((c - 110, c - 55, c + 110, c + 55), outline=FEL, width=14)
        d.ellipse((c - 35, c - 35, c + 35, c + 35), fill=FEL)
    return overhead('track_mark.png', draw)


def lantern():
    """Les Lanternes: a beater's lantern on a pole at the hall's edge, amber light (upright, 256 x 512)"""
    def draw(d, w, h):
        c = w / 2
        d.rectangle((c - 6, 200, c + 6, h - 10), fill=(120, 80, 40, 255))
        d.rectangle((c - 50, 80, c + 50, 210), outline=AMBER, width=10)
        d.ellipse((c - 35, 105, c + 35, 185), fill=AMBER)
        d.polygon([(c - 60, 80), (c + 60, 80), (c, 30)], fill=(120, 80, 40, 255))
    return overhead('lantern.png', draw)


def rustle():
    """La Traque: where he lurks at the edge - two eyes glowing in the dark, fel green (512 x 256)"""
    def draw(d, w, h):
        cy = h / 2
        for cx in (w / 2 - 80, w / 2 + 80):
            d.polygon([(cx - 60, cy), (cx, cy - 22), (cx + 60, cy), (cx, cy + 22)], fill=FEL)
    return overhead('rustle.png', draw)


def snare_ring():
    """Le Collet: its ring flashed - from 40% of the radius to the edge, a rope noose's ring, amber"""
    def draw(d, w, h):
        c = w / 2
        outer = w * 0.49
        inner = outer * 0.4
        for r in range(int(inner), int(outer), 6):
            shade = int(140 + 80 * (r - inner) / (outer - inner))
            d.ellipse((c - r, c - r, c + r, c + r), outline=(shade, int(shade * 0.65), 30), width=4)
        d.ellipse((c - inner, c - inner, c + inner, c + inner), outline=(255, 200, 80), width=16)
        d.ellipse((c - outer, c - outer, c + outer, c + outer), outline=(255, 200, 80), width=16)
    return floor('snare_ring.png', draw)


def snare_circle():
    """Le Collet: its circle flashed - iron jaws closing, the whole disc, amber"""
    def draw(d, w, h):
        c = w / 2
        r = w * 0.49
        d.ellipse((c - r, c - r, c + r, c + r), fill=(150, 90, 25))
        for k in range(16):
            a = 2 * math.pi * k / 16
            d.polygon([(c + math.cos(a) * r, c + math.sin(a) * r),
                       (c + math.cos(a + 0.12) * r * 0.7, c + math.sin(a + 0.12) * r * 0.7),
                       (c + math.cos(a + 0.24) * r, c + math.sin(a + 0.24) * r)], fill=(255, 210, 90))
    return floor('snare_circle.png', draw)


def pack_sigil():
    """L'Hallali: the group's place under him - a pack's sigil, gold"""
    def draw(d, w, h):
        c = w / 2
        r = w * 0.47
        d.ellipse((c - r, c - r, c + r, c + r), outline=(255, 200, 80), width=20)
        d.ellipse((c - r * 0.8, c - r * 0.8, c + r * 0.8, c + r * 0.8), outline=(200, 150, 50), width=10)
        d.polygon([(c, c - r * 0.55), (c + r * 0.35, c + r * 0.4), (c - r * 0.35, c + r * 0.4)],
                  outline=(255, 200, 80), width=12)
    return floor('pack_sigil.png', draw)


def hunt_sigil():
    """Under him in the middle while a puzzle runs - the Traqueur's own mark, fel green and ember"""
    def draw(d, w, h):
        c = w / 2
        r = w * 0.47
        d.ellipse((c - r, c - r, c + r, c + r), outline=(130, 255, 60), width=18)
        for a in (math.pi / 4, 3 * math.pi / 4):
            d.line((c + math.cos(a) * r * 0.8, c + math.sin(a) * r * 0.8,
                    c - math.cos(a) * r * 0.8, c - math.sin(a) * r * 0.8), fill=(255, 96, 32), width=22)
    return floor('hunt_sigil.png', draw)
