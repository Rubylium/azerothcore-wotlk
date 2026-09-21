"""Draws the paragon window as the client will, so its layout can be checked without launching the game.

Composites the real retail atlas art at the real anchor offsets from Paragon.lua: the metal nine-slice, the
rock panel ground, the title band, the sunken marble inset, and the board itself. It is not a pixel-perfect
emulator - it does not run Lua - but it puts every rectangle where the client puts it, which is enough to
catch a ground that does not reach its border or a title sitting in a gap.

Usage: python localTools/paragon/previewParagonFrame.py [output.png]
"""
import math
import os
import re
import sys

from PIL import Image, ImageDraw

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RETAIL = os.path.join(REPO, "clientPatcher", "interface", "Interface", "RetailUI")
PARAGON = os.path.join(REPO, "clientPatcher", "interface", "Interface", "Paragon")
LUA = os.path.join(REPO, "clientPatcher", "interface", "Interface", "FrameXML", "Paragon.lua")
BOARD = os.path.join(REPO, "clientPatcher", "interface", "Interface", "FrameXML", "ParagonBoard.lua")

OUTPUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, "paragon-frame-preview.png")

# The nine-slice, exactly as RetailUI.lua lays it out: corner art size, which corner, and its offset.
CORNERS = {
    "TOPLEFT": ("ui-frame-portraitmetal-cornertopleft-2x", 75, -13, 16),
    "TOPRIGHT": ("ui-frame-metal-cornertopright-2x", 75, 4, 16),
    "BOTTOMLEFT": ("ui-frame-metal-cornerbottomleft-2x", 32, -13, -3),
    "BOTTOMRIGHT": ("ui-frame-metal-cornerbottomright-2x", 32, 4, -3),
}


def lua_number(pattern, default=None):
    match = re.search(pattern, open(LUA, encoding="utf-8").read())
    if not match:
        if default is None:
            raise SystemExit("could not read %s out of Paragon.lua" % pattern)
        return default
    return [int(v) if v.isdigit() else float(v) for v in match.groups()]


WINDOW_WIDTH, WINDOW_HEIGHT = lua_number(r"WINDOW_WIDTH, WINDOW_HEIGHT = (\d+), (\d+)")
CANVAS_LEFT, CANVAS_TOP = lua_number(r"CANVAS_LEFT, CANVAS_TOP = (\d+), (\d+)")
CANVAS_RIGHT, CANVAS_BOTTOM = lua_number(r"CANVAS_RIGHT, CANVAS_BOTTOM = (\d+), (\d+)")
BOARD_EXTENT = lua_number(r"local BOARD_EXTENT = (\d+)")[0]
DEFAULT_ZOOM = lua_number(r"local DEFAULT_ZOOM = ([\d.]+)")[0]
GROUND = lua_number(r'ground:SetPoint\("TOPLEFT", (\d+), -(\d+)\)')
TITLE_Y = lua_number(r'title:SetPoint\("TOPLEFT", frame, "TOPLEFT", (\d+), -(\d+)\)')

MARGIN = 60
canvas = Image.new("RGBA", (WINDOW_WIDTH + MARGIN * 2, WINDOW_HEIGHT + MARGIN * 2), (120, 70, 50, 255))
draw = ImageDraw.Draw(canvas)


def load(name, size=None):
    image = Image.open(os.path.join(RETAIL, name + ".tga")).convert("RGBA")
    if size:
        image = image.crop((0, 0, size, size))
    return image


def tile(image, box):
    """Fill a box by repeating the image, the way SetHorizTile/SetVertTile do."""
    left, top, right, bottom = box
    patch = Image.new("RGBA", (max(1, right - left), max(1, bottom - top)), (0, 0, 0, 0))
    for y in range(0, patch.height, image.height):
        for x in range(0, patch.width, image.width):
            patch.alpha_composite(image, (x, y))
    canvas.alpha_composite(patch, (left, top))


frameLeft, frameTop = MARGIN, MARGIN
frameRight, frameBottom = MARGIN + WINDOW_WIDTH, MARGIN + WINDOW_HEIGHT

# --- panel ground, the whole window under the border
tile(load("ui-background-rock"),
     (frameLeft + GROUND[0], frameTop + GROUND[1], frameRight - GROUND[0], frameBottom - 2))

# --- title band
streaks = load("ui-frame-toptilestreaks")
streaks = streaks.crop((0, 0, streaks.width, int(streaks.height * 0.671875)))
tile(streaks.resize((streaks.width, 43)), (frameLeft + 6, frameTop + 21, frameRight - 2, frameTop + 64))

# --- sunken inset holding the board
insetBox = (frameLeft + CANVAS_LEFT, frameTop + CANVAS_TOP,
            frameRight - CANVAS_RIGHT, frameBottom - CANVAS_BOTTOM)
tile(load("ui-background-marble"), insetBox)

# --- the board itself, centred, at the zoom it opens on
nodes = {}
text = open(BOARD, encoding="utf-8").read()
for row in re.finditer(r"\[(\d+)\] = \{ type = (\d+), x = (-?\d+), y = (-?\d+)", text):
    nodes[int(row.group(1))] = (int(row.group(2)), int(row.group(3)), int(row.group(4)))
links = [(int(a), int(b)) for a, b in re.findall(r"\{ (\d+), (\d+) \},", text)]

centreX = (insetBox[0] + insetBox[2]) / 2.0
centreY = (insetBox[1] + insetBox[3]) / 2.0
SIZE = {0: 44, 1: 60, 2: 80}


def place(x, y):
    return centreX + x * DEFAULT_ZOOM, centreY - y * DEFAULT_ZOOM


clip = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
board = ImageDraw.Draw(clip)
for a, b in links:
    if a in nodes and b in nodes:
        board.line([place(nodes[a][1], nodes[a][2]), place(nodes[b][1], nodes[b][2])],
                   fill=(150, 130, 95, 150), width=max(1, int(7 * DEFAULT_ZOOM)))
for nid, (ntype, x, y) in nodes.items():
    cx, cy = place(x, y)
    r = SIZE[ntype] * DEFAULT_ZOOM / 2
    board.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(30, 28, 26, 210),
                  outline=(150, 128, 92, 220), width=max(2, int(r / 6)))
# clip the board to the inset, which is what the scroll frame does
mask = Image.new("L", canvas.size, 0)
ImageDraw.Draw(mask).rectangle(insetBox, fill=255)
canvas.paste(clip, (0, 0), Image.composite(clip.getchannel("A"), Image.new("L", canvas.size, 0), mask))

# --- nine-slice border on top: edges first, then the corners over their ends.
#
# The atlas records a usable rectangle inside each TGA, so each piece is cropped to it before being tiled;
# the top edge is 75 tall, which is why a title at -4 lands on metal rather than in mid air.
EDGES = {
    "top": ("ui-frame-metal-edgetop-2x", 32, 75, 1.0, 0.585938),
    "bottom": ("ui-frame-metal-edgebottom-2x", 16, 32, 1.0, 1.0),
    "left": ("ui-frame-metal-edgeleft-2x", 75, 16, 0.585938, 1.0),
    "right": ("ui-frame-metal-edgeright-2x", 75, 16, 0.585938, 1.0),
}


def edgeArt(key):
    name, width, height, u, v = EDGES[key]
    art = Image.open(os.path.join(RETAIL, name + ".tga")).convert("RGBA")
    return art.crop((0, 0, max(1, int(art.width * u)), max(1, int(art.height * v)))).resize((width, height))


topArt, bottomArt = edgeArt("top"), edgeArt("bottom")
leftArt, rightArt = edgeArt("left"), edgeArt("right")

tile(topArt, (frameLeft + 62, frameTop - 16, frameRight - 62, frameTop - 16 + topArt.height))
tile(bottomArt, (frameLeft + 19, frameBottom + 3 - bottomArt.height, frameRight - 19, frameBottom + 3))
tile(leftArt, (frameLeft - 13, frameTop + 59, frameLeft - 13 + leftArt.width, frameBottom - 29))
tile(rightArt, (frameRight + 4 - rightArt.width, frameTop + 59, frameRight + 4, frameBottom - 29))

for point, (name, size, dx, dy) in CORNERS.items():
    art = load(name, size)
    x = frameLeft + dx if "LEFT" in point else frameRight + dx - art.width
    y = frameTop - dy if "TOP" in point else frameBottom - dy - art.height
    canvas.alpha_composite(art, (int(x), int(y)))

# --- title
bbox = draw.textbbox((0, 0), "Parangon")
draw.text(((frameLeft + 58 + frameRight - 26) / 2 - bbox[2] / 2, frameTop + TITLE_Y[1]),
          "Parangon", fill=(255, 209, 0, 255))

# --- annotate what to look at
draw.rectangle([frameLeft, frameTop, frameRight, frameBottom], outline=(0, 255, 0, 160))
draw.text((8, 8), "green = the frame rect; the ground must reach it on all four sides",
          fill=(0, 255, 0, 255))

canvas.convert("RGB").save(OUTPUT)
print(OUTPUT)
print("  window %dx%d, ground inset %d/%d, canvas inset %d/%d/%d/%d, title y -%d"
      % (WINDOW_WIDTH, WINDOW_HEIGHT, GROUND[0], GROUND[1],
         CANVAS_LEFT, CANVAS_TOP, CANVAS_RIGHT, CANVAS_BOTTOM, TITLE_Y[1]))
