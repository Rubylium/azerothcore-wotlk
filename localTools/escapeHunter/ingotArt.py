"""Vrogar's ingot marks, as the ground indicator builder draws them (localTools/groundIndicators, shapes.json
`picture`: "localTools/escapeHunter/ingotArt.py:<function>"; looks 94860-94867, ForgeMaster.cpp LOOK_INGOT_MARK and
LOOK_INGOT_SIGIL).

Les Lingots: each ingot is held by the two players wearing its mark. The marks are the game's own raid target icons
(art/raidIcon_*.png, read from the client's Interface\\TargetingFrame\\UI-RaidTargetingIcon_1-4.blp): what every
player already reads at a glance.

- mark(icon): over a player's head (a carried billboard): the icon alone, RGBA.
- sigil(icon): on the ingot's floor (an image, its black keyed): the same icon in a ring of its colour, RGB on black.
"""
import os

from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.path.join(HERE, 'art')

# The icons read from art/ (the client build tracks them by these names: their size, the game's own)
ICONS = {
    'raidIcon_star.png': (64, 64),
    'raidIcon_circle.png': (64, 64),
    'raidIcon_diamond.png': (64, 64),
    'raidIcon_triangle.png': (64, 64),
}

# Each icon's colour: its ring on the floor
COLOURS = {
    'star': (255, 222, 60),
    'circle': (255, 140, 40),
    'diamond': (200, 90, 255),
    'triangle': (90, 230, 80),
}


def icon(name, size):
    """The raid icon `name` at size x size (the game's 64 x 64, scaled up smooth), RGBA"""
    image = Image.open(os.path.join(ART, f'raidIcon_{name}.png')).convert('RGBA')
    return image.resize((size, size), Image.LANCZOS)


def mark(name):
    """The mark over a player's head: the icon, a soft dark edge behind it to read on any ground. 256 x 256 RGBA"""
    size = 256
    picture = icon(name, 216)
    out = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    shadow = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    alpha = picture.getchannel('A').point(lambda value: min(255, value * 2) * 140 // 255)
    shadow.paste((0, 0, 0, 255), (20, 22), alpha)
    out.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(6)))
    out.alpha_composite(picture, (20, 20))
    return out


def sigil(name):
    """The ingot's floor: the icon in the middle, a ring of its colour at the edge. 1024 x 1024 RGB on black"""
    size = 1024
    out = Image.new('RGB', (size, size), (0, 0, 0))
    draw = ImageDraw.Draw(out)
    colour = COLOURS[name]
    margin = 40
    draw.ellipse((margin, margin, size - margin, size - margin), outline=colour, width=34)
    inner = (int(colour[0] * 0.45), int(colour[1] * 0.45), int(colour[2] * 0.45))
    draw.ellipse((margin + 60, margin + 60, size - margin - 60, size - margin - 60), outline=inner, width=10)
    picture = icon(name, 520)
    out.paste(picture.convert('RGB'), ((size - 520) // 2, (size - 520) // 2), picture)
    return out.filter(ImageFilter.GaussianBlur(0.8))


def stack_mark():
    """La Fonte's soak over a healer: gold chevrons from four sides closing on a point - the half of the group joins
    whoever wears it. 256 x 256 RGBA"""
    size = 256
    scale = 4
    big = size * scale
    out = Image.new('RGBA', (big, big), (0, 0, 0, 0))
    draw = ImageDraw.Draw(out)
    centre = big / 2
    gold = (255, 205, 70, 255)
    dark = (60, 30, 0, 200)
    for angle in (0, 90, 180, 270):
        # A chevron pointing at the middle, its tip 30% out
        tip = 0.20 * big
        wing = 0.40 * big
        spread = 0.16 * big

        def point(along, across):
            if angle == 0:
                return (centre + across, centre - along)
            if angle == 90:
                return (centre + along, centre + across)
            if angle == 180:
                return (centre - across, centre + along)
            return (centre - along, centre - across)
        shape = [point(tip, 0), point(wing, -spread), point(wing - 0.08 * big, 0), point(wing, spread)]
        draw.polygon(shape, fill=gold, outline=dark)
    draw.ellipse((centre - 0.06 * big, centre - 0.06 * big, centre + 0.06 * big, centre + 0.06 * big), fill=gold,
                 outline=dark, width=6)
    return out.resize((size, size), Image.LANCZOS)
