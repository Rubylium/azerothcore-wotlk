"""Builds the Paragon UI texture package from approved source artwork."""

import io
import math
import os
import struct

from PIL import Image


repoRoot = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sourceRoot = os.path.join(repoRoot, "clientPatcher", "assets", "paragon", "source")
outputRoot = os.path.join(repoRoot, "clientPatcher", "interface", "Interface", "Paragon")

assetSizes = {
    "Paragon-Node-Minor": (128, 128),
    "Paragon-Node-Notable": (128, 128),
    "Paragon-Node-Keystone": (256, 256),
    "Paragon-Node-Glow": (128, 128),
    # 64 tall, not 16: see padLinkBar. The bar itself is still only a quarter of that.
    "Paragon-Link": (64, 64),
    "Paragon-Link-Spark": (64, 64),
}

sourceFiles = {
    "Paragon-Node-Minor": "nodeMinor.png",
    "Paragon-Node-Notable": "nodeNotable.png",
    "Paragon-Node-Keystone": "nodeKeystone.png",
    "Paragon-Link": "link.png",
    "Paragon-Link-Spark": "linkSpark.png",
}


def loadSource(name):
    return Image.open(os.path.join(sourceRoot, sourceFiles[name])).convert("RGBA")


def cropVisible(image, threshold=2):
    alpha = image.getchannel("A").point(lambda value: 255 if value > threshold else 0)
    bounds = alpha.getbbox()
    return image.crop(bounds) if bounds else image


def resizeExact(image, size):
    return image.resize(size, Image.Resampling.LANCZOS)


def makeSeamless(image, size, horizontal=True):
    image = cropVisible(image)
    if horizontal:
        half = resizeExact(image, (size[0] // 2, size[1]))
        result = Image.new("RGBA", size)
        result.alpha_composite(half, (0, 0))
        result.alpha_composite(half.transpose(Image.Transpose.FLIP_LEFT_RIGHT), (size[0] // 2, 0))
    else:
        half = resizeExact(image, (size[0], size[1] // 2))
        result = Image.new("RGBA", size)
        result.alpha_composite(half, (0, 0))
        result.alpha_composite(half.transpose(Image.Transpose.FLIP_TOP_BOTTOM), (0, size[1] // 2))
    return result


def makeRadialGlow(size):
    width, height = size
    pixels = bytearray(width * height * 4)
    centerX = (width - 1) / 2
    centerY = (height - 1) / 2
    radius = min(width, height) / 2
    for y in range(height):
        for x in range(width):
            distance = math.hypot(x - centerX, y - centerY) / radius
            value = max(0.0, min(1.0, 1.0 - distance))
            alpha = round(255 * value * value * (3.0 - 2.0 * value))
            offset = (y * width + x) * 4
            pixels[offset:offset + 4] = bytes((255, 255, 255, alpha))
    return Image.frombytes("RGBA", size, bytes(pixels))


def mipChain(image):
    current = image.convert("RGBA")
    while True:
        yield current
        if min(current.size) == 1:
            return
        current = current.resize(
            (max(1, current.width // 2), max(1, current.height // 2)),
            Image.Resampling.LANCZOS,
        )


def writeDxt3Blp(image, path):
    mipmaps = []
    for mipmap in mipChain(image):
        dds = io.BytesIO()
        mipmap.save(dds, format="DDS", pixel_format="DXT3")
        encoded = dds.getvalue()[128:]
        expectedSize = max(1, (mipmap.width + 3) // 4) * max(1, (mipmap.height + 3) // 4) * 16
        if len(encoded) != expectedSize:
            raise RuntimeError(f"Unexpected DXT3 size for {mipmap.size}: {len(encoded)} != {expectedSize}")
        mipmaps.append(encoded)

    dataOffset = 148 + 256 * 4
    offsets = [0] * 16
    sizes = [0] * 16
    cursor = dataOffset
    for index, mipmap in enumerate(mipmaps):
        offsets[index] = cursor
        sizes[index] = len(mipmap)
        cursor += len(mipmap)
    header = struct.pack(
        "<4sIBBBBII16I16I",
        b"BLP2", 1, 2, 8, 1, 1, image.width, image.height, *offsets, *sizes,
    )
    with open(path, "wb") as output:
        output.write(header)
        output.write(bytes(256 * 4))
        for mipmap in mipmaps:
            output.write(mipmap)


def writeRawBlp(image, path):
    mipmaps = list(mipChain(image))
    offsets = [0] * 16
    sizes = [0] * 16
    cursor = 148
    encodedMipmaps = []
    for index, mipmap in enumerate(mipmaps):
        red, green, blue, alpha = mipmap.split()
        encoded = Image.merge("RGBA", (blue, green, red, alpha)).tobytes()
        offsets[index] = cursor
        sizes[index] = len(encoded)
        cursor += len(encoded)
        encodedMipmaps.append(encoded)
    header = struct.pack(
        "<4sIBBBBII16I16I",
        b"BLP2", 1, 3, 8, 8, 1, image.width, image.height, *offsets, *sizes,
    )
    with open(path, "wb") as output:
        output.write(header)
        for encoded in encodedMipmaps:
            output.write(encoded)


# How tall the painted bar is inside the link texture. The rest is deliberately empty.
LINK_BAR_HEIGHT = 16


def padLinkBar(bar, size):
    """Centres the bar in a taller, transparent texture.

    A link is drawn by rotating the texture with the eight-argument SetTexCoord, and the region it is drawn
    into is the bounding box of the rotated bar. The corners of that box fall outside the texture - up to ten
    times outside for a long diagonal - and the client clamps, sampling whatever is on the nearest edge. With
    a bar that is opaque to its top and bottom edges, that smears the edge colour across the entire box, and
    every link becomes a solid diagonal slab instead of a line. Transparent margins make the clamped sample
    transparent, so only the bar is drawn.

    Paragon.lua compensates by drawing at LINK_WIDTH x (texture height / bar height), so the visible bar
    keeps its intended thickness.
    """
    padded = Image.new("RGBA", size, (0, 0, 0, 0))
    padded.paste(bar, (0, (size[1] - bar.height) // 2))
    return padded


# A square icon is turned into a round one by drawing the socket over it: the corners land on metal and
# only the hole shows. That needs the metal to reach at least sqrt(2) times the hole radius, or the corners
# escape past the outer edge of the ring. The painted minor and notable rings reach 56 against holes of 45
# and 44, where they would need 64 and 62, so their holes are narrowed here until the geometry works.
#
# The band is filled with the colour of the metal already at that radius, darkened towards the middle, so it
# reads as the socket being deeper rather than as a patch stuck over the art.
ICON_CROP_RATIO = 1.4142135623730951
COLLAR_MARGIN = 0.96              # leave a little slack rather than sitting exactly on the limit
COLLAR_SHADOW = 0.62              # how dark the innermost edge of the new band goes


def socketRadii(image, opaque=200):
    """Hole radius (largest over all angles) and the radius metal covers at every angle."""
    alpha = image.getchannel("A")
    size = image.size[0]
    middle = size / 2.0
    limit = int(middle) - 1
    holeRadius, coverRadius = 0.0, float(limit)
    for step in range(720):
        theta = math.radians(step / 2.0)
        cos, sin = math.cos(theta), math.sin(theta)
        start = None
        for r in range(limit):
            value = alpha.getpixel((int(middle + r * cos), int(middle + r * sin)))
            if start is None:
                if value >= opaque:
                    start = r
            elif value < opaque:
                coverRadius = min(coverRadius, r)
                break
        else:
            if start is not None:
                coverRadius = min(coverRadius, float(limit))
        if start is not None:
            holeRadius = max(holeRadius, start)
    return holeRadius, coverRadius


def _sampleRing(source, size, radius, samples=720, smoothing=11):
    """The colour of the metal all the way round at one radius, smoothed.

    Sampling per pixel instead would pick up the grain of the painting and lay it down as radial streaks,
    which is precisely what the first attempt at this did.
    """
    middle = size / 2.0
    raw = []
    for index in range(samples):
        theta = 2.0 * math.pi * index / samples
        x = middle + radius * math.cos(theta)
        y = middle + radius * math.sin(theta)
        x0, y0 = int(x), int(y)
        fx, fy = x - x0, y - y0
        total = [0.0, 0.0, 0.0]
        for dx, dy, weight in ((0, 0, (1 - fx) * (1 - fy)), (1, 0, fx * (1 - fy)),
                               (0, 1, (1 - fx) * fy), (1, 1, fx * fy)):
            px = min(size - 1, max(0, x0 + dx))
            py = min(size - 1, max(0, y0 + dy))
            pixel = source[px, py]
            for channel in range(3):
                total[channel] += pixel[channel] * weight
        raw.append(total)

    half = smoothing // 2
    smoothed = []
    for index in range(samples):
        total = [0.0, 0.0, 0.0]
        for offset in range(-half, half + 1):
            sample = raw[(index + offset) % samples]
            for channel in range(3):
                total[channel] += sample[channel]
        smoothed.append(tuple(value / smoothing for value in total))
    return smoothed


def tightenSocketHole(image):
    """Narrows the hole until an icon filling it can be cropped by the ring."""
    holeRadius, coverRadius = socketRadii(image)
    target = (coverRadius / ICON_CROP_RATIO) * COLLAR_MARGIN
    if target >= holeRadius:
        return image                                    # already deep enough, nothing to do

    size = image.size[0]
    middle = size / 2.0
    source = image.load()
    out = image.copy()
    pixels = out.load()

    samples = 720
    ring = _sampleRing(source, size, holeRadius + 2.0, samples=samples)
    span = max(holeRadius - target, 1.0)

    for y in range(size):
        for x in range(size):
            if source[x, y][3] >= 200:
                continue                                # leave the painted ring exactly as it is
            dx, dy = x + 0.5 - middle, y + 0.5 - middle
            radius = math.sqrt(dx * dx + dy * dy)
            if radius < target or radius > holeRadius + 1.0:
                continue

            theta = math.atan2(dy, dx) % (2.0 * math.pi)
            red, green, blue = ring[int(theta / (2.0 * math.pi) * samples) % samples]
            # darkest at the inner lip, full strength where it meets the painted ring
            depth = (radius - target) / span
            shade = COLLAR_SHADOW + (1.0 - COLLAR_SHADOW) * depth
            # feather the very inside edge so the hole does not gain a hard rim of its own
            edge = min(1.0, (radius - target) / 1.5)
            alpha = int(255 * edge)
            pixels[x, y] = (int(red * shade), int(green * shade), int(blue * shade), alpha)
    return out


def buildAssets():
    assets = {}
    for name, size in assetSizes.items():
        if name == "Paragon-Node-Glow":
            assets[name] = makeRadialGlow(size)
        elif name == "Paragon-Link":
            assets[name] = padLinkBar(makeSeamless(loadSource(name), (size[0], LINK_BAR_HEIGHT),
                                                   horizontal=True), size)
        elif name in ("Paragon-Node-Minor", "Paragon-Node-Notable"):
            assets[name] = tightenSocketHole(resizeExact(loadSource(name), size))
        else:
            assets[name] = resizeExact(loadSource(name), size)
    return assets


def validateAsset(name, image):
    expectedSize = assetSizes[name]
    if image.size != expectedSize:
        raise RuntimeError(f"{name} is {image.size}; expected {expectedSize}")
    if image.mode != "RGBA":
        raise RuntimeError(f"{name} must be RGBA")
    if name.startswith("Paragon-Node-") and name != "Paragon-Node-Glow":
        if image.getchannel("A").getpixel((image.width // 2, image.height // 2)) != 0:
            raise RuntimeError(f"{name} center is not transparent")
    if name == "Paragon-Link":
        if image.crop((0, 0, 1, image.height)).tobytes() != image.crop((image.width - 1, 0, image.width, image.height)).tobytes():
            raise RuntimeError(f"{name} does not tile horizontally")
        # The margins are the whole point of this texture's shape; without them every link draws as a slab.
        alpha = image.getchannel("A")
        if max(alpha.crop((0, 0, image.width, 1)).getdata()) != 0 \
                or max(alpha.crop((0, image.height - 1, image.width, image.height)).getdata()) != 0:
            raise RuntimeError(f"{name} must be transparent along its top and bottom edges")


def main():
    os.makedirs(outputRoot, exist_ok=True)
    for name, image in buildAssets().items():
        validateAsset(name, image)
        pngPath = os.path.join(outputRoot, name + ".png")
        blpPath = os.path.join(outputRoot, name + ".blp")
        image.save(pngPath, optimize=True)
        if name == "Paragon-Node-Glow":
            writeRawBlp(image, blpPath)
            encoding = "raw BGRA"
        else:
            writeDxt3Blp(image, blpPath)
            encoding = "DXT3"
        print(f"Built {name}: {image.width}x{image.height}, {encoding}")


if __name__ == "__main__":
    main()
