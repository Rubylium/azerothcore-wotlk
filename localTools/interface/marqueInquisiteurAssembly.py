"""Registration and seamless joins for the Inquisitor's painted UI pieces."""

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter

from gladiatorHudV2Assembly import cleanTransparent, mirrorLeft


def visibleCrop(image):
    bounds = image.getchannel("A").point(lambda value: 255 if value > 128 else 0).getbbox()
    return image.crop(bounds)


def seamlessBar(source):
    source = visibleCrop(source)
    source = source.resize((256, 40), Image.Resampling.LANCZOS)
    source = mirrorLeft(source)
    pixels = np.asarray(source).copy()
    # One shared endpoint cross-section gives both exact colour and derivative continuity.
    profile = pixels[:, 0].copy()
    for x in range(16):
        weight = max(0, (x - 3) / 13)
        pixels[:, x] = np.rint(profile * (1 - weight) + pixels[:, x] * weight)
        pixels[:, 255 - x] = pixels[:, x]
    bar = Image.new("RGBA", (256, 48), 0)
    bar.paste(Image.fromarray(pixels), (0, 4))
    return cleanTransparent(bar)


def repeatBar(bar, width):
    result = Image.new("RGBA", (width, 48), 0)
    for x in range(0, width, 256):
        result.paste(bar, (x, 0))
    return result


def crest(source, bar, smaller=False):
    painting = mirrorLeft(source.resize((384, 192), Image.Resampling.LANCZOS))
    if smaller:
        motif = painting.crop((120, 28, 264, 168)).resize((108, 105), Image.Resampling.LANCZOS)
        painting = Image.new("RGBA", (384, 192), 0)
        painting.paste(motif, (138, 44))
    base = Image.new("RGBA", (384, 192), 0)
    base.paste(repeatBar(bar, 384), (0, 72))
    mask = Image.new("L", (384, 192), 0)
    ImageDraw.Draw(mask).rectangle((108, 0, 275, 191), fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(10))
    painting.putalpha(ImageChops.multiply(painting.getchannel("A"), mask))
    base.alpha_composite(painting)
    return cleanTransparent(mirrorLeft(base))


def corner(source, bar):
    painting = source.resize((256, 256), Image.Resampling.LANCZOS)
    base = Image.new("RGBA", (256, 256), 0)
    base.paste(bar, (0, 24))
    base.alpha_composite(bar.transpose(Image.Transpose.ROTATE_90), (24, 0))
    y, x = np.indices((256, 256))
    mask = np.clip((150 - np.maximum(x, y)) / 28, 0, 1)
    painting.putalpha(Image.fromarray(np.rint(np.asarray(painting.getchannel("A")) * mask).astype(np.uint8)))
    base.alpha_composite(painting)
    return cleanTransparent(base)


def titlePlate(source):
    painting = mirrorLeft(visibleCrop(source).resize((512, 96), Image.Resampling.LANCZOS))
    pixels = np.asarray(painting).copy()
    y, x = np.indices((96, 512), dtype=float)
    fade = np.clip(np.minimum(x, 511 - x) / 64, 0, 1) * np.clip(np.minimum(y, 95 - y) / 18, 0, 1)
    pixels[:, :, 3] = np.rint(pixels[:, :, 3] * fade * 0.22)
    return cleanTransparent(Image.fromarray(pixels))


def registeredGlow(source, frame):
    glow = mirrorLeft(source.convert("RGB").resize((512, 512), Image.Resampling.LANCZOS))
    alpha = frame.getchannel("A")
    # Keep exterior light around the exact frame silhouette; never spill into the icon's central opening.
    outer = alpha.filter(ImageFilter.MaxFilter(25)).filter(ImageFilter.GaussianBlur(6))
    support = ImageChops.subtract(outer, alpha.filter(ImageFilter.MinFilter(3)))
    pixels = np.asarray(glow).astype(float)
    y, x = np.indices((512, 512), dtype=float)
    fade = np.clip(np.minimum.reduce((x, 511 - x, y, 511 - y)) / 8, 0, 1)
    pixels *= (np.asarray(support) / 255 * fade * 0.65)[:, :, None]
    pixels[pixels.max(axis=2) < 5] = 0
    return Image.fromarray(np.rint(pixels).astype(np.uint8))


def assembleAssets(sources):
    bar = seamlessBar(sources["tooltipBarHorizontal"])
    frame = cleanTransparent(mirrorLeft(sources["itemFrame"].resize((512, 512), Image.Resampling.LANCZOS)))
    frame.putalpha(frame.getchannel("A").point(lambda value: 0 if value <= 2 else value))
    frame = cleanTransparent(frame)
    return {
        "itemFrame": frame,
        "itemFrameGlow": registeredGlow(sources["itemFrameGlow"], frame),
        "tooltipCorner": corner(sources["tooltipCorner"], bar),
        "tooltipTopCrest": crest(sources["tooltipTopCrest"], bar),
        "tooltipBottomCrest": crest(sources["tooltipBottomCrest"], bar, smaller=True),
        "tooltipBarHorizontal": bar,
        "tooltipBarVertical": bar.transpose(Image.Transpose.ROTATE_90),
        "tooltipTitlePlate": titlePlate(sources["tooltipTitlePlate"]),
        "itemIcon": sources["itemIcon"].convert("RGB").resize((256, 256), Image.Resampling.LANCZOS),
        "powerIcon": sources["powerIcon"].convert("RGB").resize((256, 256), Image.Resampling.LANCZOS),
    }
