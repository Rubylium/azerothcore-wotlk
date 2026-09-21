"""Compiles the paragon node icons from source PNGs into the 64x64 TGAs the client loads.

The board names its icons Interface\\Icons\\ParagonNode_*, which the client resolves straight to a file - a
custom icon needs no DBC entry, unlike a spell's. patchFiles.js packs everything in client-assets/compiled
into patch-Z.MPQ, so writing them there is all that is needed.

The TGA is written by hand rather than through Pillow's writer so it is byte-identical in shape to the forty
icons this module already ships (localTools/buildRogueClientAssets.ps1): uncompressed 32-bit BGRA, top-down.
Pillow writes bottom-up, which would put every icon in the game upside down.

Usage: python localTools/paragon/buildParagonIcons.py
"""
import os
import struct
import sys

from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SOURCE = os.path.join(REPO, "modules", "mod-stat-growth", "client-assets", "source", "icons")
COMPILED = os.path.join(REPO, "modules", "mod-stat-growth", "client-assets", "compiled")

SIZE = 64
PREFIX = "ParagonNode_"


def write_tga(image, path):
    """Uncompressed 32-bit BGRA, origin top-left - the same shape as the module's existing icons."""
    image = image.convert("RGBA").resize((SIZE, SIZE), Image.LANCZOS)

    header = bytearray(18)
    header[2] = 2                       # uncompressed true-colour
    struct.pack_into("<HH", header, 12, SIZE, SIZE)
    header[16] = 32                     # bits per pixel
    header[17] = 0x28                   # top-left origin (0x20) plus 8 bits of alpha

    pixels = bytearray()
    for r, g, b, a in image.getdata():
        pixels += bytes((b, g, r, a))

    with open(path, "wb") as handle:
        handle.write(bytes(header))
        handle.write(bytes(pixels))


def main():
    if not os.path.isdir(SOURCE):
        raise SystemExit("no icon sources at %s" % SOURCE)

    os.makedirs(COMPILED, exist_ok=True)
    names = sorted(name for name in os.listdir(SOURCE) if name.lower().endswith(".png"))
    if not names:
        raise SystemExit("no PNGs in %s" % SOURCE)

    built = 0
    for name in names:
        stem = os.path.splitext(name)[0]
        if not stem.startswith(PREFIX):
            print("  skipped %s (not a %s icon)" % (name, PREFIX))
            continue

        image = Image.open(os.path.join(SOURCE, name))
        if image.width != image.height:
            raise SystemExit("%s is %dx%d; node icons must be square" % (name, image.width, image.height))

        write_tga(image, os.path.join(COMPILED, stem + ".tga"))
        built += 1

    print("Built %d paragon node icons in %s" % (built, COMPILED))
    return 0


if __name__ == "__main__":
    sys.exit(main())
