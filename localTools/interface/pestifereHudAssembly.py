"""Register Pestifere paintings while preserving flask glass, socket rims and frame symmetry."""

from collections import deque

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter

from gladiatorHudV2Assembly import cleanAdditive, cleanTransparent, fitFamily, mirrorLeft, solidBounds, socketGlow


def alignSocketFamily(images, size, padding):
    """Use the empty socket's aspect ratio; align later paintings by their own solid bounds before transfer."""
    base = fitFamily(images[:1], size, padding)[0]
    target = solidBounds(base)
    fittedSize = (target[2] - target[0], target[3] - target[1])
    states = [base]
    for image in images[1:]:
        painting = image.crop(solidBounds(image)).resize(fittedSize, Image.Resampling.LANCZOS)
        canvas = Image.new("RGBA", size, 0)
        canvas.paste(painting, target[:2])
        states.append(canvas)
    return states


def connectedHollow(eligible, seed):
    mask = np.zeros(eligible.shape, dtype=bool)
    pending = deque([seed])
    while pending:
        x, y = pending.pop()
        if x < 0 or y < 0 or y >= eligible.shape[0] or x >= eligible.shape[1]:
            continue
        if mask[y, x] or not eligible[y, x]:
            continue
        mask[y, x] = True
        pending.extend(((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)))
    if not mask.any():
        raise ValueError("Flask hollow seed was not inside the original dark glass")
    return mask


def flaskStates(images):
    registered = alignSocketFamily(images, (132, 120), 3)
    base = np.asarray(registered[0])
    eligible = (base[:, :, :3].max(axis=2) < 50) & (base[:, :, 3] > 128)
    hollow = connectedHollow(eligible, (66, 77))
    states = []
    for image in registered:
        pixels = base.copy()
        pixels[hollow, :3] = np.asarray(image)[hollow, :3]
        states.append(cleanTransparent(Image.fromarray(pixels)))
    y, x = np.nonzero(hollow)
    bounds = [int(x.min()), int(x.max()) + 1, int(y.min()), int(y.max()) + 1]
    return states, Image.fromarray(hollow.astype(np.uint8) * 255), bounds


def boilStates(images):
    registered = alignSocketFamily(images, (64, 64), 5)
    face = Image.new("L", (64, 64), 0)
    ImageDraw.Draw(face).ellipse((11, 11, 52, 52), fill=255)
    face = face.filter(ImageFilter.GaussianBlur(0.5))
    states = [registered[0]]
    for painting in registered[1:]:
        state = Image.composite(painting, registered[0], face)
        state.putalpha(registered[0].getchannel("A"))
        states.append(state)
    return [cleanTransparent(state) for state in states], face


def registerBoiling(source, emptySource, empty):
    """Keep the flask's source-to-target transform for bubble registration, including neck overflow."""
    sourceBounds, targetBounds = solidBounds(emptySource), solidBounds(empty)
    targetSize = (targetBounds[2] - targetBounds[0], targetBounds[3] - targetBounds[1])
    fitted = cleanAdditive(source).crop(sourceBounds).resize(targetSize, Image.Resampling.LANCZOS)
    result = Image.new("RGB", empty.size, 0)
    result.paste(fitted, targetBounds[:2])
    return cleanAdditive(result)


def registerDrips(source, frame):
    drips = mirrorLeft(cleanAdditive(source).resize(frame.size, Image.Resampling.LANCZOS))
    support = frame.getchannel("A").filter(ImageFilter.MaxFilter(15))
    # Keep the generated hanging drops beneath the cutting edge, but exclude central knot and handles.
    pixels = np.asarray(support).copy()
    for amount in range(1, 25):
        pixels[amount:] = np.maximum(pixels[amount:], np.asarray(support)[:-amount])
    pixels[:, 188:324] = 0
    y, x = np.indices(pixels.shape, dtype=float)
    fade = np.clip(np.minimum.reduce((x, frame.width - 1 - x, y, frame.height - 1 - y)) / 3, 0, 1)
    pixels = np.rint(pixels * fade).astype(np.uint8)
    mask = Image.fromarray(pixels)
    return cleanAdditive(ImageChops.multiply(drips, Image.merge("RGB", (mask, mask, mask))))


def partialFlask(empty, full, mask, bounds, fraction):
    if fraction <= 0:
        return empty
    if fraction >= 1:
        return full
    top, bottom = bounds[2:]
    clipped = np.asarray(mask).copy()
    clipped[:round(bottom - (bottom - top) * fraction)] = 0
    return Image.composite(full, empty, Image.fromarray(clipped))


def buildAssets(loadSource):
    assets = {"frame": cleanTransparent(mirrorLeft(loadSource("frame").resize((512, 224),
                                                                            Image.Resampling.LANCZOS)))}
    flaskNames = ["flaskEmpty", "flaskBone", "flaskFlesh", "flaskBile"]
    flasks, mask, bounds = flaskStates([loadSource(name) for name in flaskNames])
    assets.update(zip(flaskNames, flasks))
    boilNames = ["boilFlat", "boilSmall", "boilSwollen", "boilRipe"]
    boils, face = boilStates([loadSource(name) for name in boilNames])
    assets.update(zip(boilNames, boils))
    assets["boiling"] = registerBoiling(loadSource("boiling"), loadSource("flaskEmpty"), assets["flaskEmpty"])
    assets["bileDrips"] = registerDrips(loadSource("bileDrips"), assets["frame"])
    for name in ("glowToxic", "glowSepulcre"):
        assets[name] = socketGlow(loadSource(name))
    assets["splash"] = cleanAdditive(loadSource("splash")).resize((128, 128), Image.Resampling.LANCZOS)
    return assets, mask, bounds, face
