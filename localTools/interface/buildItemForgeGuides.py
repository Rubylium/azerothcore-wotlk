"""Layout guides for the Forge's painted art (.agents/plans/item-forge/item-forge.ASSETS.md): the exact canvases of
the paintings that must register with the window's code (the anvil stage, the masterpiece banner), every zone drawn
and labelled where the code puts it. The window itself keeps the shell all our windows share; no frame is painted.
Given to the image AI as layout references; the paintings must keep these canvases and zones.

All the numbers are UI pixels at 1x; every canvas is painted at exactly 2x.

Usage: python localTools/interface/buildItemForgeGuides.py
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

REPO = Path(__file__).resolve().parents[2]
OUT = REPO / "clientPatcher" / "assets" / "itemForge" / "reference"
SCALE = 2

# The anvil stage, in UI pixels inside its 434 x 240 window
STAGE = (434, 240)
STAGE_ZONES = [
    ("item name strip - quiet, dark", (0, 0, 434, 34), (120, 160, 220)),
    ("forge mouth (arched)", (120, 34, 194, 92), (255, 120, 40)),
    ("item icon 58 x 58 - left clear", (188, 91, 58, 58), (255, 70, 70)),
    ("anvil top face, just under the icon", (140, 150, 160, 10), (232, 180, 90)),
    ("hammer resting here (drawn by code)", (290, 96, 120, 90), (90, 200, 120)),
]

BANNER = (480, 110)
BANNER_ZONES = [
    ("icon socket 64 x 64", (26, 23, 64, 64), (232, 180, 90)),
    ("text area - plain", (112, 14, 350, 82), (120, 160, 220)),
]


def font(size):
    for name in ("segoeui.ttf", "arial.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def guide(size, zones, name, extra=()):
    image = Image.new("RGB", (size[0] * SCALE, size[1] * SCALE), (24, 22, 20))
    draw = ImageDraw.Draw(image)
    label = font(13 * SCALE // 2 + 4)
    for text, (x, y, w, h), colour in list(zones) + list(extra):
        box = [x * SCALE, y * SCALE, (x + w) * SCALE - 1, (y + h) * SCALE - 1]
        draw.rectangle(box, outline=colour, width=2)
        # A leading "_" puts the label at the zone's bottom, clear of the zones inside it
        if text.startswith("_"):
            draw.text((box[0] + 6, box[3] - 24), text[1:], fill=colour, font=label)
        elif text:
            draw.text((box[0] + 6, box[1] + 4), text, fill=colour, font=label)
    title = f"{name}: canvas {size[0] * SCALE} x {size[1] * SCALE} (drawn {size[0]} x {size[1]})"
    draw.text((10, image.height - 30), title, fill=(230, 230, 230), font=label)
    image.save(OUT / f"{name}Guide.png")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    guide(STAGE, STAGE_ZONES, "stage")
    guide(BANNER, BANNER_ZONES, "banner")
    print("guides written to", OUT)


if __name__ == "__main__":
    main()
