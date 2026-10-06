"""Shared registration, socket protection and symmetry for Gladiateur HUD v2 paintings."""

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter


def solidBounds(image):
    return image.getchannel("A").point(lambda value: 255 if value > 128 else 0).getbbox()


def fitFamily(images, size, padding):
    bounds = solidBounds(images[0])
    result = []
    for image in images:
        fitted = image.crop(bounds).resize((size[0] - padding * 2, size[1] - padding * 2),
                                           Image.Resampling.LANCZOS)
        canvas = Image.new("RGBA", size, 0)
        canvas.paste(fitted, (padding, padding))
        result.append(canvas)
    return result


def mirrorLeft(image):
    result = image.copy()
    half = image.crop((0, 0, image.width // 2, image.height))
    result.paste(half.transpose(Image.Transpose.FLIP_LEFT_RIGHT), (image.width // 2, 0))
    return result


def cleanTransparent(image):
    pixels = np.asarray(image.convert("RGBA")).copy()
    pixels[pixels[:, :, 3] == 0, :3] = 0
    return Image.fromarray(pixels)


def cleanAdditive(image):
    pixels = np.asarray(image.convert("RGB")).copy()
    pixels[pixels.max(axis=2) < 8] = 0
    return Image.fromarray(pixels)


def helmetStates(images):
    registered = fitFamily(images, (152, 108), 3)
    empty = registered[0]
    base = np.asarray(empty)
    full = np.asarray(registered[3])
    # Transfer light only into the original charcoal hollow; the original bronze rim and alpha are immutable.
    hollow = ((base[:, :, :3].max(axis=2) < 48) & (base[:, :, 3] > 128)
              & (full[:, :, 0].astype(float) > full[:, :, 1] * 2.0)
              & (full[:, :, 0].astype(float) > full[:, :, 2] * 2.0))
    xValues = np.nonzero(hollow)[1]
    left, right = int(xValues.min()), int(xValues.max()) + 1
    fractions = (0, 1 / 3, 2 / 3, 1, 1)
    states = []
    for painting, fraction in zip(registered, fractions):
        mask = hollow.copy()
        mask[:, round(left + (right - left) * fraction):] = False
        result = base.copy()
        result[mask, :3] = np.asarray(painting)[mask, :3]
        states.append(cleanTransparent(Image.fromarray(result)))
    return states, Image.fromarray(hollow.astype(np.uint8) * 255), [left, right]


def jewelStates(images):
    dark, gold, crack = fitFamily(images, (64, 64), 5)
    face = Image.new("L", (64, 64), 0)
    ImageDraw.Draw(face).ellipse((11, 11, 52, 52), fill=255)
    face = face.filter(ImageFilter.GaussianBlur(0.5))
    lit = Image.composite(gold, dark, face)
    lit.putalpha(dark.getchannel("A"))
    # The crack's visible painting is fitted separately, centred inside the gem's face.
    bounds = solidBounds(crack)
    crack = crack.crop(bounds).resize((40, 40), Image.Resampling.LANCZOS)
    overlay = Image.new("RGBA", (64, 64), 0)
    overlay.paste(crack, (12, 12))
    overlay.putalpha(ImageChops.multiply(overlay.getchannel("A"), face))
    return [cleanTransparent(image) for image in (dark, lit, overlay)]


def bladeEdgeMask(frame):
    alpha = frame.getchannel("A")
    # A narrow band on and around the painted silhouette constrains generated energy to the true edge.
    outer = alpha.filter(ImageFilter.MaxFilter(9))
    inner = alpha.filter(ImageFilter.MinFilter(5))
    band = ImageChops.subtract(outer, inner).filter(ImageFilter.GaussianBlur(1))
    pixels = np.asarray(band).copy()
    pixels[:, 216:296] = 0
    pixels[162:, :] = 0
    return Image.fromarray(pixels)


def registerEnergy(source, frame):
    energy = cleanAdditive(source).resize(frame.size, Image.Resampling.LANCZOS)
    energy = mirrorLeft(energy)
    mask = bladeEdgeMask(frame)
    return cleanAdditive(ImageChops.multiply(energy, Image.merge("RGB", (mask, mask, mask))))


def socketGlow(source):
    # Blur reference-shaped aura into broad light so no foreign skull outline survives behind the helmets.
    glow = cleanAdditive(source).resize((212, 108), Image.Resampling.LANCZOS)
    glow = glow.filter(ImageFilter.GaussianBlur(5))
    y, x = np.indices((108, 212), dtype=float)
    fadeX = np.clip(np.minimum(x, 211 - x) / 12, 0, 1)
    fadeY = np.clip(np.minimum(y, 107 - y) / 10, 0, 1)
    pixels = np.rint(np.asarray(glow) * (fadeX * fadeY)[:, :, None]).astype(np.uint8)
    return cleanAdditive(Image.fromarray(pixels))
