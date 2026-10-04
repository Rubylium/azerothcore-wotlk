"""Compiles the frames drawn around a boss-touched item's icon (ItemFramesVoice.lua) into the BLP2 textures the 3.3.5
client loads. Each frame is a painting (localTools/interface/assets/<boss>/item-frame.png, transparent, its middle
empty) re-framed so that its opening is a square in the texture's centre: the opening is found in the alpha, the
painting is cut around it (padded where it runs short) and stretched to a square, so the client only has to lay the
texture over the icon. Shipped uncompressed (the Paragon sky's writer): DXT would band the gold.

The generator leaves a reddish matte along the frame's outer edge; its half-transparent pixels take the colour of the
opaque frame next to them instead.

Usage: python localTools/interface/buildItemFrameArt.py
"""
import importlib.util
import os

import numpy
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
ASSETS = os.path.join(HERE, "assets")
OUT = os.path.join(REPO, "clientPatcher", "interface", "Interface", "ItemFrames")

SIZE = 128  # drawn about 50 px wide: a small texture keeps the client's sampling from shimmering

FRAMES = {"Voice": "hollowVoice"}


def load_blp_writer():
    path = os.path.join(HERE, "buildParagonArt.py")
    spec = importlib.util.spec_from_file_location("buildParagonArt", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.writeRawBlp


def defringe(image):
    rgba = numpy.asarray(image, dtype=numpy.float32).copy()
    alpha = rgba[..., 3]
    solid = (alpha >= 250).astype(numpy.float32)
    weight = numpy.asarray(Image.fromarray((solid * 255).astype(numpy.uint8)).filter(ImageFilter.GaussianBlur(4)),
                           dtype=numpy.float32) + 1e-3
    for channel in range(3):
        premultiplied = Image.fromarray((rgba[..., channel] * solid).astype(numpy.uint8))
        spread = numpy.asarray(premultiplied.filter(ImageFilter.GaussianBlur(4)), dtype=numpy.float32)
        neighbour = numpy.clip(spread / weight * 255, 0, 255)
        edge = alpha < 250
        rgba[..., channel] = numpy.where(edge, neighbour, rgba[..., channel])
    return Image.fromarray(rgba.round().astype(numpy.uint8), "RGBA")


def opening(alpha):
    """The empty middle: walked out from the centre along its row and column."""
    height, width = alpha.shape
    row, column = alpha[height // 2], alpha[:, width // 2]

    def walk(line, start, step):
        index = start
        while 0 <= index < len(line) and line[index] <= 128:
            index += step
        return index

    return (walk(row, width // 2, -1) + 1, walk(column, height // 2, -1) + 1,
            walk(row, width // 2, 1), walk(column, height // 2, 1))


def build(name, folder, write):
    painting = defringe(Image.open(os.path.join(ASSETS, folder, "item-frame.png")).convert("RGBA"))
    alpha = numpy.asarray(painting)[..., 3]
    left, top, right, bottom = opening(alpha)
    centre_x, centre_y = (left + right) / 2, (top + bottom) / 2
    box_left, box_top, box_right, box_bottom = painting.getbbox()
    # The opening's share of the texture: as large as the painting lets it, the same on both axes
    scale = max(2 * max(centre_x - box_left, box_right - centre_x) / (right - left),
                2 * max(centre_y - box_top, box_bottom - centre_y) / (bottom - top))
    half_width, half_height = (right - left) * scale / 2, (bottom - top) * scale / 2
    crop = painting.crop((round(centre_x - half_width), round(centre_y - half_height),
                          round(centre_x + half_width), round(centre_y + half_height)))
    texture = crop.resize((SIZE, SIZE), Image.Resampling.LANCZOS)
    os.makedirs(OUT, exist_ok=True)
    texture.save(os.path.join(OUT, f"ItemFrame-{name}.png"))
    write(texture, os.path.join(OUT, f"ItemFrame-{name}.blp"))
    print(f"wrote ItemFrame-{name}: opening {right - left}x{bottom - top} of the painting, "
          f"{1 / scale:.4f} of the texture (ItemFramesVoice.lua OPENING)")


def main():
    write = load_blp_writer()
    for name, folder in FRAMES.items():
        build(name, folder, write)


if __name__ == "__main__":
    main()
