"""Builds the 3.3.5 Glue-screen logo texture from the approved Evolutions artwork."""

import io
import os
import struct

from PIL import Image


REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LOGO_ROOT = os.path.join(REPO_ROOT, 'clientPatcher', 'assets', 'logo')
SOURCE_PATH = os.path.join(LOGO_ROOT, 'WorldOfWarcraft-Evolutions.png')
OUTPUT_PATH = os.path.join(LOGO_ROOT, 'Glues-WoW-WotLKLogo.blp')
TARGET_SIZE = (512, 256)


def write_blp2(image, path):
    """Writes DXT3 BLP2 with a complete mip chain, matching the original Glue logo format."""
    image = image.convert('RGBA')
    width, height = image.size
    mipmaps = []
    current = image
    while True:
        dds = io.BytesIO()
        current.save(dds, format='DDS', pixel_format='DXT3')
        encoded = dds.getvalue()[128:]
        expected_size = max(1, (current.width + 3) // 4) * max(1, (current.height + 3) // 4) * 16
        if len(encoded) != expected_size:
            raise RuntimeError(f'Unexpected DXT3 size for {current.size}: {len(encoded)} != {expected_size}')
        mipmaps.append(encoded)
        # Blizzard's original 512x256 Glue logo stops at 2x1 (nine mip levels).
        if min(current.size) == 1:
            break
        current = current.resize((max(1, current.width // 2), max(1, current.height // 2)),
                                 Image.Resampling.LANCZOS)

    data_offset = 148 + 256 * 4
    offsets = [0] * 16
    sizes = [0] * 16
    cursor = data_offset
    for index, mipmap in enumerate(mipmaps):
        offsets[index] = cursor
        sizes[index] = len(mipmap)
        cursor += len(mipmap)
    header = struct.pack('<4sIBBBBII16I16I', b'BLP2', 1, 2, 8, 1, 1,
                         width, height, *offsets, *sizes)

    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as output:
        output.write(header)
        output.write(bytes(256 * 4))
        for mipmap in mipmaps:
            output.write(mipmap)


def main():
    logo = Image.open(SOURCE_PATH).convert('RGBA')
    logo.thumbnail(TARGET_SIZE, Image.Resampling.LANCZOS)
    canvas = Image.new('RGBA', TARGET_SIZE, (0, 0, 0, 0))
    canvas.alpha_composite(logo, ((TARGET_SIZE[0] - logo.width) // 2, (TARGET_SIZE[1] - logo.height) // 2))
    write_blp2(canvas, OUTPUT_PATH)
    print(f'Built {OUTPUT_PATH} ({TARGET_SIZE[0]}x{TARGET_SIZE[1]})')


if __name__ == '__main__':
    main()
