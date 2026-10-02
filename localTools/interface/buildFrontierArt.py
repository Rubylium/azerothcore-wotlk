"""Compiles Le Front du Nord's art (mod-stat-growth src/frontier, FrontierUI.lua) from the paintings in
localTools/interface/assets/frontier into the files the 3.3.5 client loads.

- Interface\\Frontier\\Crest<n>         the tier crests on the zone banner (256x256)
- Interface\\Frontier\\RiftIcon         the rift's map pin and tracker icon (128x128)
- Interface\\Frontier\\ColossusIcon<n>  each Colosse's head, cut square from its portrait, for its toast (256x256)
- Interface\\Icons\\INV_Frontier_FrostShard   the Éclat de givre's item icon (64x64 TGA, shipped by patchFiles.js
                                            from client-assets/compiled; its ItemDisplayInfo row is the patcher's)

The crests and the rift icon were painted on flat black: the black around them is keyed out (only the black joined
to the picture's edge, so the dark inside an emblem stays) and their outline softened. Shipped uncompressed (the
Paragon writer): DXT would band their gradients.

Usage: python localTools/interface/buildFrontierArt.py
"""
import importlib.util
import os
import struct

import numpy
from PIL import Image, ImageFilter
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SOURCE = os.path.join(HERE, "assets", "frontier")
OUT = os.path.join(REPO, "clientPatcher", "interface", "Interface", "Frontier")
ICONS = os.path.join(REPO, "modules", "mod-stat-growth", "client-assets", "compiled")

# Each Colosse's head in its 1024x1536 portrait: its centre (shares of width / height) and the square's side (share of
# the width)
COLOSSUS_HEADS = {
    1: (0.50, 0.20, 0.72),
    2: (0.56, 0.24, 0.56),
    3: (0.46, 0.22, 0.56),
    4: (0.48, 0.19, 0.52),
}

# Darker than this on every channel, and joined to the edge: background
BLACK = 26


def load_blp_writer():
    path = os.path.join(HERE, "buildParagonArt.py")
    spec = importlib.util.spec_from_file_location("buildParagonArt", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.writeRawBlp


def key_black(image):
    """The black background joined to the picture's edge made transparent, the outline feathered over a pixel."""
    rgba = numpy.asarray(image.convert("RGBA"), dtype=numpy.float32)
    dark = rgba[..., :3].max(axis=2) < BLACK
    labels, _ = ndimage.label(dark)
    edge = numpy.unique(numpy.concatenate([labels[0], labels[-1], labels[:, 0], labels[:, -1]]))
    background = numpy.isin(labels, edge[edge > 0])
    # Soft: the picture's own near-black rim fades into the keyed-out black instead of ending in a hard step
    keep = ndimage.gaussian_filter((~background).astype(numpy.float32), 1.2)
    rgba[..., 3] = numpy.clip(keep, 0.0, 1.0) * 255.0
    return Image.fromarray(rgba.round().astype(numpy.uint8), "RGBA")


def square(image, size):
    return image.resize((size, size), Image.Resampling.LANCZOS)


def colossus_head(index):
    painting = Image.open(os.path.join(SOURCE, f"colossus-{index}.png")).convert("RGBA")
    cx, cy, side = COLOSSUS_HEADS[index]
    half = painting.width * side / 2
    x, y = painting.width * cx, painting.height * cy
    left = int(round(min(max(x - half, 0), painting.width - 2 * half)))
    top = int(round(min(max(y - half, 0), painting.height - 2 * half)))
    crop = painting.crop((left, top, left + int(2 * half), top + int(2 * half)))
    return square(crop, 256).filter(ImageFilter.UnsharpMask(radius=1.2, percent=40, threshold=2))


def write_icon_tga(image, path, size=64):
    """Uncompressed 32-bit BGRA, top-down: the shape of the module's other icons (buildParagonIcons.py)."""
    image = image.convert("RGBA").resize((size, size), Image.Resampling.LANCZOS)
    header = bytearray(18)
    header[2] = 2
    struct.pack_into("<HH", header, 12, size, size)
    header[16] = 32
    header[17] = 0x28
    pixels = bytearray()
    for r, g, b, a in image.getdata():
        pixels += bytes((b, g, r, a))
    with open(path, "wb") as handle:
        handle.write(bytes(header))
        handle.write(bytes(pixels))


def main():
    write_blp = load_blp_writer()
    os.makedirs(OUT, exist_ok=True)
    for tier in range(1, 5):
        crest = key_black(Image.open(os.path.join(SOURCE, f"crest-{tier}.png")))
        write_blp(square(crest, 256), os.path.join(OUT, f"Crest{tier}.blp"))
    write_blp(square(key_black(Image.open(os.path.join(SOURCE, "rift-icon.png"))), 128),
              os.path.join(OUT, "RiftIcon.blp"))
    for index in COLOSSUS_HEADS:
        write_blp(colossus_head(index), os.path.join(OUT, f"ColossusIcon{index}.blp"))
    write_icon_tga(Image.open(os.path.join(SOURCE, "shard-icon.png")),
                   os.path.join(ICONS, "INV_Frontier_FrostShard.tga"))
    print(f"Front du Nord art written to {OUT} and the shard icon to {ICONS}")


if __name__ == "__main__":
    main()
