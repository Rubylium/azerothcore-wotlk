"""Builds the proc glow drawn on an action button (FrameXML/SpellStates.lua) from retail's own: the two flipbooks of
retail's spell alert, UI-HUD-ActionBar-Proc-Start-Flipbook (a ring closing onto the button, sparks) and
UI-HUD-ActionBar-Proc-Loop-Flipbook (the gold border with its running shine), 30 frames each, 5 columns by 6 rows.

They are read from a local retail install through localTools/retailImport's tool (its `extract`: the atlas texture
5199404, 2048x1024, raw BGRA), cut out by their UiTextureAtlasMember rectangles, and laid out again as 8 by 4 frames
of a power-of-two size, as the 3.3.5 client wants: ProcStart 1024x512 (128 px frames), ProcLoop 512x256 (64 px).
Shipped uncompressed (the Paragon sky's writer): DXT would band the glow.

Usage: python localTools/interface/buildProcGlowArt.py [--atlas <an already extracted 5199404.blp>]
"""
import argparse
import importlib.util
import os
import struct
import subprocess
import tempfile

import numpy
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(REPO, "clientPatcher", "interface", "Interface", "SpellAlert")
TOOL = os.path.join(REPO, "localTools", "retailImport", "RetailImport", "bin", "Release", "net10.0", "RetailImport.exe")

ATLAS_FILE = 5199404
# UiTextureAtlasMember (retail 12.x): left, top, right, bottom in the atlas
FLIPBOOKS = {
    "ProcStart": ((1, 1, 843, 1011), 128),     # UI-HUD-ActionBar-Proc-Start-Flipbook
    "ProcLoop": ((845, 1, 1178, 401), 64),     # UI-HUD-ActionBar-Proc-Loop-Flipbook
}
COLUMNS, ROWS, FRAMES = 5, 6, 30
OUT_COLUMNS, OUT_ROWS = 8, 4


def load_blp_writer():
    path = os.path.join(HERE, "buildParagonArt.py")
    spec = importlib.util.spec_from_file_location("buildParagonArt", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.writeRawBlp


def read_raw_blp(path):
    """A BLP2 of encoding 3 (raw BGRA), its first mipmap."""
    data = open(path, "rb").read()
    if data[:4] != b"BLP2" or data[8] != 3:
        raise SystemExit(f"{path}: not a raw BGRA BLP2")
    width, height = struct.unpack_from("<II", data, 12)
    offset = struct.unpack_from("<I", data, 20)[0]
    pixels = numpy.frombuffer(data, dtype=numpy.uint8, count=width * height * 4, offset=offset)
    return Image.fromarray(pixels.reshape(height, width, 4)[:, :, [2, 1, 0, 3]].copy(), "RGBA")


def relayout(atlas, rectangle, frame_size):
    left, top, right, bottom = rectangle
    cell_width, cell_height = (right - left) / COLUMNS, (bottom - top) / ROWS
    sheet = Image.new("RGBA", (OUT_COLUMNS * frame_size, OUT_ROWS * frame_size), (0, 0, 0, 0))
    for frame in range(FRAMES):
        column, row = frame % COLUMNS, frame // COLUMNS
        box = (round(left + column * cell_width), round(top + row * cell_height),
               round(left + (column + 1) * cell_width), round(top + (row + 1) * cell_height))
        cell = atlas.crop(box).resize((frame_size, frame_size), Image.LANCZOS)
        sheet.paste(cell, ((frame % OUT_COLUMNS) * frame_size, (frame // OUT_COLUMNS) * frame_size))
    return sheet


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--atlas")
    args = parser.parse_args()
    path = args.atlas
    if not path:
        path = os.path.join(tempfile.mkdtemp(prefix="procglow-"), f"{ATLAS_FILE}.blp")
        subprocess.run([TOOL, "extract", str(ATLAS_FILE), path], check=True)
    atlas = read_raw_blp(path)
    write = load_blp_writer()
    os.makedirs(OUT, exist_ok=True)
    for name, (rectangle, frame_size) in FLIPBOOKS.items():
        target = os.path.join(OUT, name + ".blp")
        write(relayout(atlas, rectangle, frame_size), target)
        print("wrote", os.path.relpath(target, REPO))


if __name__ == "__main__":
    main()
