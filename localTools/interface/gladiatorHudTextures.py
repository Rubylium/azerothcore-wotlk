"""Registration and atlas packaging for the painted Gladiateur HUD sources."""

import math

import numpy as np
from PIL import Image, ImageChops


def visibleBounds(image, threshold=128):
    return image.getchannel("A").point(lambda value: 255 if value >= threshold else 0).getbbox()


def registerImage(image, sourceCentre, sourceRadius, targetSize, targetRadius, background=(0, 0, 0, 0)):
    """Apply one shared affine registration, preserving straight alpha through Pillow's RGBA resampling."""
    scaleX = sourceRadius[0] / targetRadius[0]
    scaleY = sourceRadius[1] / targetRadius[1]
    transform = (
        scaleX, 0, sourceCentre[0] - targetSize[0] / 2 * scaleX,
        0, scaleY, sourceCentre[1] - targetSize[1] / 2 * scaleY,
    )
    return image.transform(targetSize, Image.Transform.AFFINE, transform, Image.Resampling.BICUBIC,
                           fillcolor=background)


def registerFamily(images, targetSize, diameter):
    left, top, right, bottom = visibleBounds(images[0])
    centre = ((left + right) / 2, (top + bottom) / 2)
    radius = max(right - left, bottom - top) / 2
    return [registerImage(image, centre, (radius, radius), targetSize, (diameter / 2, diameter / 2))
            for image in images]


def registerRing(image, targetSize, targetRadius, targetWidth):
    """Sample the painted blood into a precise annulus; never repaint its colour or texture."""
    source = np.asarray(image.convert("RGBA"))
    alpha = source[:, :, 3]
    rows, cols = np.nonzero(alpha > 128)
    centreX = (cols.min() + cols.max()) / 2
    centreY = (rows.min() + rows.max()) / 2
    radiusX = (cols.max() - cols.min()) / 2
    radiusY = (rows.max() - rows.min()) / 2
    y, x = np.indices((targetSize, targetSize), dtype=float)
    dx, dy = x + 0.5 - targetSize / 2, y + 0.5 - targetSize / 2
    distance = np.hypot(dx, dy)
    angle = np.arctan2(dy, dx)
    # The source's wet ring occupies its outermost ~3.6% radial band.
    sourceFraction = 0.982 + (distance - targetRadius) / targetWidth * 0.036
    sourceX = np.clip(np.rint(centreX + np.cos(angle) * radiusX * sourceFraction), 0, image.width - 1).astype(int)
    sourceY = np.clip(np.rint(centreY + np.sin(angle) * radiusY * sourceFraction), 0, image.height - 1).astype(int)
    result = source[sourceY, sourceX].copy()
    coverage = np.clip(targetWidth / 2 + 0.5 - np.abs(distance - targetRadius), 0, 1)
    result[:, :, 3] = np.rint(result[:, :, 3] * coverage).astype(np.uint8)
    result[result[:, :, 3] == 0, :3] = 0
    return Image.fromarray(result)


def bloodFrames(ring):
    y, x = np.indices((ring.height, ring.width), dtype=float)
    # In screen coordinates clockwise from six o'clock passes through nine o'clock first.
    angle = np.mod(np.arctan2(y + 0.5 - ring.height / 2, x + 0.5 - ring.width / 2) - math.pi / 2,
                   2 * math.pi)
    frames = []
    for index in range(16):
        mask = Image.fromarray(np.where(angle < index / 15 * 2 * math.pi, 255, 0).astype(np.uint8))
        frame = ring.copy()
        frame.putalpha(ImageChops.multiply(frame.getchannel("A"), mask))
        pixels = np.asarray(frame).copy()
        pixels[pixels[:, :, 3] == 0, :3] = 0
        frame = Image.fromarray(pixels)
        frames.append(frame)
    return frames


def packFrames(frames, columns):
    width, height = frames[0].size
    atlas = Image.new(frames[0].mode, (width * columns, height * math.ceil(len(frames) / columns)), 0)
    for index, frame in enumerate(frames):
        atlas.paste(frame, (index % columns * width, index // columns * height))
    return atlas


def cleanBlack(image):
    """Remove near-black generator noise so additive backgrounds are exactly zero."""
    pixels = np.asarray(image.convert("RGB")).copy()
    pixels[pixels.max(axis=2) < 8] = 0
    return Image.fromarray(pixels)
