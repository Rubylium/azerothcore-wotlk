"""Shared registration for the two new Scarlet Cathedral legendary sets."""

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter

from gladiatorHudV2Assembly import cleanTransparent, mirrorLeft
from marqueInquisiteurAssembly import crest, registeredGlow, seamlessBar


def insetPainting(source):
    canvas = Image.new(source.mode, (512, 512), 0)
    canvas.paste(source.resize((464, 464), Image.Resampling.LANCZOS), (24, 24))
    return canvas


def slimBar(source, thickness):
    bar = seamlessBar(source).crop((0, 4, 256, 44))
    bar = bar.resize((256, thickness), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (256, 48), 0)
    canvas.paste(bar, (0, (48 - thickness) // 2))
    return cleanTransparent(mirrorLeft(canvas))


def registeredCorner(source, bar):
    painting = source.resize((256, 256), Image.Resampling.LANCZOS)
    alpha = np.asarray(painting.getchannel("A"))
    yValues = np.flatnonzero(alpha[:, 176] > 128)
    xValues = np.flatnonzero(alpha[176] > 128)
    offset = (round(48 - xValues.mean()), round(48 - yValues.mean()))
    registered = Image.new("RGBA", (256, 256), 0)
    registered.paste(painting, offset)
    compact = Image.new("RGBA", (256, 256), 0)
    compact.paste(registered.resize((218, 218), Image.Resampling.LANCZOS), (7, 7))
    registered = compact
    base = Image.new("RGBA", (256, 256), 0)
    horizontal = bar.copy()
    horizontal.paste(0, (0, 0, 48, 48))
    vertical = bar.transpose(Image.Transpose.ROTATE_90)
    vertical.paste(0, (0, 0, 48, 48))
    base.paste(horizontal, (0, 24))
    base.alpha_composite(vertical, (24, 0))
    mask = Image.new("L", (256, 256), 0)
    ImageDraw.Draw(mask).rectangle((0, 0, 108, 108), fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(5))
    registered.putalpha(ImageChops.multiply(registered.getchannel("A"), mask))
    base.alpha_composite(registered)
    return cleanTransparent(base)


def assembleFamily(sources, family):
    bar = slimBar(sources["tooltipBarHorizontal"], 28 if family == "sermentWhitemane" else 36)
    frame = mirrorLeft(insetPainting(sources["itemFrame"]))
    frame.putalpha(frame.getchannel("A").point(lambda value: 0 if value <= 2 else value))
    frame = cleanTransparent(frame)
    glow = registeredGlow(insetPainting(sources["itemFrameGlow"].convert("RGB")), frame)
    return {
        "itemFrame": frame,
        "itemFrameGlow": glow,
        "tooltipCorner": registeredCorner(sources["tooltipCorner"], bar),
        "tooltipTopCrest": crest(sources["tooltipTopCrest"], bar),
        "tooltipBottomCrest": crest(sources["tooltipBottomCrest"], bar, smaller=True),
        "tooltipBarHorizontal": bar,
        "tooltipBarVertical": bar.transpose(Image.Transpose.ROTATE_90),
        "itemIcon": sources["itemIcon"].convert("RGB").resize((256, 256), Image.Resampling.LANCZOS),
        "powerIcon": sources["powerIcon"].convert("RGB").resize((256, 256), Image.Resampling.LANCZOS),
    }


def validateFamily(assets):
    sizes = [(512, 512), (512, 512), (256, 256), (384, 192), (384, 192),
             (256, 48), (48, 256), (256, 256), (256, 256)]
    assert len(assets) == 9
    for (name, image), size in zip(assets.items(), sizes):
        assert image.size == size, name
    for name in ("itemFrame", "itemFrameGlow", "tooltipTopCrest", "tooltipBottomCrest", "tooltipBarHorizontal"):
        pixels = np.asarray(assets[name])
        assert np.array_equal(pixels, pixels[:, ::-1]), name
    horizontal = np.asarray(assets["tooltipBarHorizontal"])
    vertical = np.asarray(assets["tooltipBarVertical"])
    assert np.array_equal(horizontal[:, 0], horizontal[:, -1])
    assert np.array_equal(vertical[0], vertical[-1])
    cornerPixels = np.asarray(assets["tooltipCorner"])
    assert np.array_equal(cornerPixels[24:72, -1], horizontal[:, -1])
    assert np.array_equal(cornerPixels[-1, 24:72], vertical[-1])
    for name in ("tooltipTopCrest", "tooltipBottomCrest"):
        pixels = np.asarray(assets[name])
        assert np.array_equal(pixels[72:120, 0], horizontal[:, 0]), name
        assert np.array_equal(pixels[72:120, -1], horizontal[:, -1]), name
    assert not np.asarray(assets["itemFrame"])[180:350, 180:332, 3].any()
    glow = np.asarray(assets["itemFrameGlow"])
    assert not glow[0].any() and not glow[-1].any() and not glow[:, 0].any() and not glow[:, -1].any()
    for name, image in assets.items():
        if image.mode == "RGBA":
            pixels = np.asarray(image)
            assert not pixels[pixels[:, :, 3] == 0, :3].any(), name
