"""Builds the Paragon UI texture package from approved source artwork."""

import io
import math
import os
import re
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

# And how far it is held back from the left and right edges, for the same reason the top and bottom are
# empty. Two columns rather than one so bilinear filtering cannot reach an opaque texel from outside.
LINK_BAR_MARGIN = 2


def padLinkBar(bar, size):
    """Centres the bar in a larger, transparent texture.

    A link is drawn by rotating the texture with the eight-argument SetTexCoord, and the region it is drawn
    into is the bounding box of the rotated bar. The corners of that box fall outside the texture - up to ten
    times outside for a long diagonal - and the client clamps, sampling whatever is on the nearest edge. With
    a bar that is opaque to its top and bottom edges, that smears the edge colour across the entire box, and
    every link becomes a solid diagonal slab instead of a line. Transparent margins make the clamped sample
    transparent, so only the bar is drawn.

    The left and right edges need the same treatment. Clamping happens in both directions, and a bar that
    runs to the texture's side edges gets those columns dragged out into the corners of the box - a curved
    claw hanging off the end of every diagonal link. Holding the bar back from the sides costs a couple of
    pixels of length at each end, which the node art covers.

    Paragon.lua compensates by drawing at LINK_WIDTH x (texture height / bar height), so the visible bar
    keeps its intended thickness.
    """
    padded = Image.new("RGBA", size, (0, 0, 0, 0))
    padded.paste(bar, ((size[0] - bar.width) // 2, (size[1] - bar.height) // 2))
    return padded


# A square icon is turned into a round one by drawing the socket over it: the corners land on metal and
# only the hole shows. That needs the metal to reach at least sqrt(2) times the hole radius, or the corners
# escape past the outer edge of the ring. The painted minor and notable rings reach 56 against holes of 45
# and 44, where they would need 64 and 62, so their holes are narrowed here until the geometry works.
#
# The band is filled with the colour of the metal already at that radius, darkened towards the middle, so it
# reads as the socket being deeper rather than as a patch stuck over the art.
ICON_CROP_RATIO = 1.4142135623730951
# How much of the theoretical limit to actually use. At 0.96 the keystone's corners cleared the metal by
# about one part in two hundred, which is "covered" on paper and plainly a square on screen once the socket is
# drawn at eighty pixels. The crop has to bite, not just touch.
COLLAR_MARGIN = 0.80
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
            assets[name] = padLinkBar(makeSeamless(loadSource(name),
                                                   (size[0] - 2 * LINK_BAR_MARGIN, LINK_BAR_HEIGHT),
                                                   horizontal=True), size)
        elif name.startswith("Paragon-Node-"):
            # Every socket, the keystone included: its ring is thick, but its hole is wide enough that the
            # corners of an icon filling it only just reached metal.
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


# --- Per-geometry link textures -------------------------------------------------------------------------
#
# Rotating one bar texture with the eight-argument SetTexCoord is the usual way to draw a line, and it does
# not work here. The coordinates it needs for a long thin bar run far outside [0,1] - measured up to 3.3 and
# -2.3 on this board - and the only links that survived on screen were the horizontals, whose coordinates are
# exactly 0 and 1. Everything with a rotation came out as a sliver or an elbow.
#
# So the rotation is baked into the art instead. Every distinct link shape on the board gets its own texture
# with the bar already drawn across it, and the frame places it axis-aligned. One texel is one board unit, so
# the board's own scale does the zooming and nothing has to be recomputed at runtime.

# Built and checked, but not packed: the frame only ever asks for the per-geometry textures.
SOURCE_ONLY = {"Paragon-Link", "Paragon-Link-Spark"}

LINK_PAD = 12            # board units of clearance around the bar, so every edge stays transparent
LINK_THICKNESS = 7.0     # the visible width of a link, in board units
LINK_SAMPLES = 3         # supersampling per axis


def potAtLeast(value):
    """The rule Paragon.lua's potAtLeast repeats. The two must agree or the art lands at the wrong size."""
    size = 16
    while size < value:
        size *= 2
    return size


def linkGeometries(boardPath):
    """Every distinct |dx| x |dy| a link on the board spans."""
    text = io.open(boardPath, encoding="utf-8").read()
    nodes = {}
    for match in re.finditer(r"\[(\d+)\] = \{ type = \d+, x = (-?\d+), y = (-?\d+)", text):
        nodes[int(match.group(1))] = (int(match.group(2)), int(match.group(3)))

    body = text.split("links = {", 1)[1]
    shapes = set()
    for a, b in re.findall(r"\{\s*(\d+)\s*,\s*(\d+)\s*\}", body):
        ax, ay = nodes[int(a)]
        bx, by = nodes[int(b)]
        shapes.add((abs(bx - ax), abs(by - ay)))
    return sorted(shapes)


def crossSection(bar):
    """The painted bar's profile across its width, averaged along its length."""
    pixels = bar.load()
    rows = []
    for y in range(bar.height):
        total = [0.0, 0.0, 0.0, 0.0]
        for x in range(bar.width):
            for channel, value in enumerate(pixels[x, y]):
                total[channel] += value
        rows.append([value / bar.width for value in total])
    return rows


def buildLinkTexture(dx, dy, profile):
    """Draws the bar corner to corner of a centred dx by dy box, in the '/' sense."""
    width, height = potAtLeast(dx + LINK_PAD), potAtLeast(dy + LINK_PAD)
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    pixels = image.load()

    # endpoints, in image coordinates: y grows downward, so '/' runs bottom-left to top-right
    cx, cy = width / 2.0, height / 2.0
    x0, y0 = cx - dx / 2.0, cy + dy / 2.0
    x1, y1 = cx + dx / 2.0, cy - dy / 2.0
    vx, vy = x1 - x0, y1 - y0
    span = math.hypot(vx, vy)
    if span < 1e-6:
        return image
    ux, uy = vx / span, vy / span

    half = LINK_THICKNESS / 2.0
    step = 1.0 / LINK_SAMPLES
    reach = int(math.ceil(half)) + 2

    for py in range(height):
        for px in range(width):
            # cheap rejection: the pixel centre must be within reach of the segment
            mx, my = px + 0.5 - x0, py + 0.5 - y0
            along = mx * ux + my * uy
            across = abs(mx * -uy + my * ux)
            if across > reach or along < -reach or along > span + reach:
                continue

            # premultiplied accumulation: colour weighted by each sample's own alpha, coverage separate
            colour = [0.0, 0.0, 0.0]
            weight = 0.0
            for sy in range(LINK_SAMPLES):
                for sx in range(LINK_SAMPLES):
                    ox = px + (sx + 0.5) * step - x0
                    oy = py + (sy + 0.5) * step - y0
                    t = ox * ux + oy * uy
                    if t < 0.0 or t > span:
                        continue
                    d = ox * -uy + oy * ux
                    if abs(d) >= half:
                        continue
                    row = profile[min(len(profile) - 1,
                                      max(0, int((d + half) / LINK_THICKNESS * len(profile))))]
                    for channel in range(3):
                        colour[channel] += row[channel] * row[3]
                    weight += row[3]

            if weight <= 0.0:
                continue
            alpha = weight / (LINK_SAMPLES * LINK_SAMPLES)
            if alpha < 0.5:
                continue
            pixels[px, py] = (int(round(colour[0] / weight)), int(round(colour[1] / weight)),
                              int(round(colour[2] / weight)), int(round(min(255.0, alpha))))
    return image


# The spark that runs along a link has the same problem the link had: it is a comet with a nose and a tail,
# and a texture cannot be rotated in 3.3.5, so on a diagonal it flew sideways. Same answer - one already
# rotated copy per link shape, drawn pointing up and to the right. The frame reaches the other three
# directions with plain axis flips, which are in-range texture coordinates and always safe.
SPARK_CANVAS = 64


def buildSparkTexture(base, dx, dy):
    """The comet, turned to point along a dx, dy link in the '/' sense."""
    angle = math.degrees(math.atan2(dy, dx))
    # PIL rotates counter-clockwise as seen on screen, which is the direction that takes a right-pointing
    # comet to an up-and-right-pointing one.
    return base.rotate(angle, resample=Image.Resampling.BICUBIC, expand=False)


def buildSparkTextures(shapes):
    base = resizeExact(loadSource("Paragon-Link-Spark"), (SPARK_CANVAS, SPARK_CANVAS))

    # Rotating inside a fixed canvas throws away anything that swings past a corner, so shrink the comet
    # until it fits within the inscribed circle before any rotation happens.
    bounds = cropVisible(base).size
    reach = math.hypot(*bounds)
    if reach > SPARK_CANVAS - 4:
        scale = (SPARK_CANVAS - 4) / reach
        shrunk = resizeExact(cropVisible(base), (max(1, int(bounds[0] * scale)),
                                                 max(1, int(bounds[1] * scale))))
        base = Image.new("RGBA", (SPARK_CANVAS, SPARK_CANVAS), (0, 0, 0, 0))
        base.alpha_composite(shrunk, ((SPARK_CANVAS - shrunk.width) // 2,
                                      (SPARK_CANVAS - shrunk.height) // 2))

    built = 0
    for dx, dy in shapes:
        image = buildSparkTexture(base, dx, dy)
        alpha = image.getchannel("A")
        for edge in (alpha.crop((0, 0, image.width, 1)),
                     alpha.crop((0, image.height - 1, image.width, image.height)),
                     alpha.crop((0, 0, 1, image.height)),
                     alpha.crop((image.width - 1, 0, image.width, image.height))):
            if max(edge.getdata()) != 0:
                raise RuntimeError(f"Paragon-Link-Spark-{dx}x{dy} is clipped by its own canvas")
        if max(alpha.getdata()) == 0:
            raise RuntimeError(f"Paragon-Link-Spark-{dx}x{dy} came out empty")

        name = f"Paragon-Link-Spark-{dx}x{dy}"
        image.save(os.path.join(outputRoot, name + ".png"), optimize=True)
        writeDxt3Blp(image, os.path.join(outputRoot, name + ".blp"))
        built += 1
    return built


def buildLinkTextures():
    boardPath = os.path.join(repoRoot, "clientPatcher", "interface", "Interface", "FrameXML",
                             "ParagonBoard.lua")
    if not os.path.exists(boardPath):
        print("No ParagonBoard.lua; skipping link textures")
        return 0, []

    bar = makeSeamless(loadSource("Paragon-Link"), (64, LINK_BAR_HEIGHT), horizontal=True)
    profile = crossSection(bar)

    shapes = linkGeometries(boardPath)
    built = 0
    for dx, dy in shapes:
        image = buildLinkTexture(dx, dy, profile)
        alpha = image.getchannel("A")
        for edge in (alpha.crop((0, 0, image.width, 1)), alpha.crop((0, image.height - 1, image.width,
                                                                     image.height)),
                     alpha.crop((0, 0, 1, image.height)), alpha.crop((image.width - 1, 0, image.width,
                                                                      image.height))):
            if max(edge.getdata()) != 0:
                raise RuntimeError(f"Paragon-Link-{dx}x{dy} touches its own edge; raise LINK_PAD")
        if max(alpha.getdata()) == 0:
            raise RuntimeError(f"Paragon-Link-{dx}x{dy} came out empty")

        name = f"Paragon-Link-{dx}x{dy}"
        image.save(os.path.join(outputRoot, name + ".png"), optimize=True)
        writeDxt3Blp(image, os.path.join(outputRoot, name + ".blp"))
        built += 1
    return built, shapes


def main():
    os.makedirs(outputRoot, exist_ok=True)
    for name, image in buildAssets().items():
        validateAsset(name, image)
        # The plain bar and the un-turned comet are only raw material now: the per-geometry textures are
        # built from the sources in assets/paragon/source, and nothing in the frame asks for these two by
        # name any more. Building them keeps the validation honest; shipping them would be dead weight.
        if name in SOURCE_ONLY:
            for stale in (name + ".png", name + ".blp"):
                path = os.path.join(outputRoot, stale)
                if os.path.exists(path):
                    os.remove(path)
            print(f"Checked {name}: {image.width}x{image.height}, not shipped")
            continue
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

    built, shapes = buildLinkTextures()
    if built:
        print(f"Built {built} per-geometry link textures")
        print(f"Built {buildSparkTextures(shapes)} per-geometry link sparks")


if __name__ == "__main__":
    main()
