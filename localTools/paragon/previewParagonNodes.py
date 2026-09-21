"""Composites each socket over an icon at the size the client draws it, so the crop can be judged.

An icon is turned into a round one by drawing the socket over it: the square's corners land on metal and only
the circle of the hole shows. Whether that works is a question of a few pixels, and checking it at the
texture's own resolution is misleading - the keystone once cleared its metal by one part in two hundred,
which passes any "did a corner escape" test and is still plainly a square once the socket is drawn at eighty
pixels. So this renders at the drawn size, the way the client will, and magnifies afterwards.

Sizes and scales are read out of Paragon.lua, so this cannot drift away from what actually ships.

Usage: python localTools/paragon/previewParagonNodes.py [output.png]
"""
import os
import re
import sys

from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ART = os.path.join(REPO, "clientPatcher", "interface", "Interface", "Paragon")
LUA = os.path.join(REPO, "clientPatcher", "interface", "Interface", "FrameXML", "Paragon.lua")
OUTPUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, "paragon-nodes-preview.png")

ZOOM = 5
MARBLE = (13, 13, 13)            # the inset the nodes are drawn on

text = open(LUA, encoding="utf-8").read()
sizes = [int(v) for v in re.search(
    r"NODE_SIZE = \{ \[0\] = (\d+), \[1\] = (\d+), \[2\] = (\d+)", text).groups()]
scales = [float(v) for v in re.search(
    r"ICON_SCALE = \{ \[0\] = ([\d.]+), \[1\] = ([\d.]+), \[2\] = ([\d.]+)", text).groups()]
CASES = list(zip(["Paragon-Node-Minor", "Paragon-Node-Notable", "Paragon-Node-Keystone"], sizes, scales))


def checker(side):
    """A checkerboard, because a straight edge in one is unmistakable where a dark spell icon hides it."""
    icon = Image.new("RGBA", (side, side), (215, 215, 215, 255))
    step = max(2, side // 8)
    for y in range(side):
        for x in range(side):
            if ((x // step) + (y // step)) % 2:
                icon.putpixel((x, y), (120, 120, 120, 255))
    return icon


panels = []
for socket, drawn, scale in CASES:
    ring = Image.open(os.path.join(ART, socket + ".png")).convert("RGBA").resize(
        (drawn, drawn), Image.LANCZOS)

    cell = Image.new("RGBA", (drawn + 8, drawn + 8), MARBLE + (255,))
    side = max(2, int(round(drawn * scale)))
    cell.alpha_composite(checker(side), ((cell.width - side) // 2, (cell.height - side) // 2))
    cell.alpha_composite(ring, (4, 4))
    panels.append(cell.resize((cell.width * ZOOM, cell.height * ZOOM), Image.NEAREST))

width = sum(panel.width for panel in panels) + 20 * (len(panels) + 1)
strip = Image.new("RGB", (width, max(panel.height for panel in panels) + 40), (30, 30, 34))
x = 20
for panel in panels:
    strip.paste(panel, (x, 20), panel)
    x += panel.width + 20
strip.save(OUTPUT)

print(OUTPUT)
for socket, drawn, scale in CASES:
    print("  %-9s drawn %2dpx, icon %.2f -> %2dpx square inside the hole"
          % (socket.split("-")[-1], drawn, scale, round(drawn * scale)))
print("  Any straight edge visible in a checkerboard means the crop is not biting.")
