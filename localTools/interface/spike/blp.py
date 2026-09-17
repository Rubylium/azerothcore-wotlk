"""Minimal BLP2 reader: falls back to Pillow except for uncompressed BGRA (encoding 3)."""
import struct
import sys

from PIL import Image


def read_blp(path):
    data = open(path, 'rb').read()
    if data[:4] == b'BLP2':
        _, encoding, alpha_depth, alpha_encoding, has_mips = struct.unpack_from('<IBBBB', data, 4)
        width, height = struct.unpack_from('<II', data, 12)
        offsets = struct.unpack_from('<16I', data, 20)
        if encoding == 3:
            raw = data[offsets[0]:offsets[0] + width * height * 4]
            return Image.frombuffer('RGBA', (width, height), raw, 'raw', 'BGRA', 0, 1).copy()
    return Image.open(path).convert('RGBA')


if __name__ == '__main__':
    for path in sys.argv[1:]:
        image = read_blp(path)
        background = Image.new('RGBA', image.size, (40, 40, 40, 255))
        background.alpha_composite(image)
        background.convert('RGB').save(path + '.preview.png')
        print(path, image.size)
