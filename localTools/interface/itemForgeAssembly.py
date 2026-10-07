"""Register generated Item Forge paintings; final PNGs are consumed at their native sizes.

A painting is only ever scaled uniformly (one factor for both axes) and cropped: never stretched, squashed, sliced
into bands or mirrored (.agents/plans/item-forge/item-forge.ASSETS.md)."""

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

from gladiatorHudV2Assembly import cleanTransparent, solidBounds


def fitSolid(image, size, padding=0):
    bounds = solidBounds(image)
    art = image.crop(bounds)
    scale = min((size[0] - 2 * padding) / art.width, (size[1] - 2 * padding) / art.height)
    art = art.resize((round(art.width * scale), round(art.height * scale)), Image.Resampling.LANCZOS)
    result = Image.new("RGBA", size)
    result.paste(art, ((size[0] - art.width) // 2, (size[1] - art.height) // 2))
    return cleanTransparent(result)


def stateFamily(images, size, padding=0):
    states = [fitSolid(image, size, padding) for image in images]
    alpha = states[0].getchannel("A")
    for state in states:
        state.putalpha(alpha)
    return [cleanTransparent(state) for state in states]


def heatedRow(cold, selected):
    alpha = cold.getchannel("A")
    edge = np.asarray(alpha, dtype=float) - np.asarray(alpha.filter(ImageFilter.MinFilter(7)), dtype=float)
    edge = np.clip(edge / 255, 0, 1)[:, :, None] * 0.75
    pixels = np.asarray(selected, dtype=float).copy()
    pixels[:, :, :3] = pixels[:, :, :3] * (1 - edge) + np.array([255, 138, 42]) * edge
    return cleanTransparent(Image.fromarray(np.clip(pixels, 0, 255).astype(np.uint8)))


def edgeFade(image, margin=8, threshold=5):
    pixels = np.asarray(image.convert("RGB"), dtype=float).copy()
    y, x = np.indices(pixels.shape[:2])
    distance = np.minimum.reduce([x, y, image.width - 1 - x, image.height - 1 - y])
    fade = np.clip(distance / margin, 0, 1)
    pixels *= (fade * fade * (3 - 2 * fade))[:, :, None]
    pixels[pixels.max(axis=2) < threshold] = 0
    return Image.fromarray(np.clip(np.rint(pixels), 0, 255).astype(np.uint8))


def coverUniform(image, size):
    """Scale uniformly until the canvas is covered, then crop the overflow evenly: the painting keeps its shape"""
    scale = max(size[0] / image.width, size[1] / image.height)
    scaled = image.resize((round(image.width * scale), round(image.height * scale)), Image.Resampling.LANCZOS)
    left, top = (scaled.width - size[0]) // 2, (scaled.height - size[1]) // 2
    return scaled.crop((left, top, left + size[0], top + size[1]))


def registerStage(image):
    return coverUniform(image.convert("RGB"), (868, 480))


def stageFamily(cold, hot):
    # Both states are the same painting (the lit one an edit of the cold): the same uniform scale, nothing pasted
    return [edgeFade(registerStage(image), 26, 0) for image in (cold, hot)]


def fireLight(image):
    image = coverUniform(image.convert("RGB"), (868, 480))
    # Register the generated diffuse light peak to the furnace mouth, not the overall canvas centre.
    shifted = Image.new("RGB", image.size)
    shifted.paste(image, (0, -38))
    y, x = np.indices((480, 868))
    mask = np.exp(-(((x - 434) / 210) ** 4 + ((y - 208) / 160) ** 4))
    # A piece overlays this region; suppress light inside its reserved dark square.
    icon = Image.new("L", image.size)
    ImageDraw.Draw(icon).rectangle((376, 182, 491, 297), fill=255)
    icon = np.asarray(icon.filter(ImageFilter.GaussianBlur(18)), dtype=float) / 255
    mask *= 1 - 0.88 * icon
    pixels = np.asarray(shifted, dtype=float) * mask[:, :, None] * 0.55
    return edgeFade(Image.fromarray(pixels.astype(np.uint8)), 24)


def gaugeFill(image):
    # The generated strip occupies the top of its native canvas; isolate that painted strip at its own aspect.
    image = image.convert("RGB")
    stripHeight = round(image.width * 20 / 464)
    strip = image.crop((0, 0, image.width, stripHeight)).resize((464, 20), Image.Resampling.LANCZOS)
    pixels = np.asarray(strip, dtype=float)
    depth = np.linspace(0, 1, 20)[:, None, None]
    pixels *= 1 - depth * np.array([0.2, 0.5, 0.65])
    return Image.fromarray(pixels.astype(np.uint8))


def radialLight(image, size, radius=0.43):
    image = image.convert("RGB").resize(size, Image.Resampling.LANCZOS)
    y, x = np.indices((size[1], size[0]), dtype=float)
    distance = np.hypot(x - (size[0] - 1) / 2, y - (size[1] - 1) / 2)
    outer = min(size) * radius
    fade = np.clip((outer - distance) / (min(size) * 0.1), 0, 1)
    pixels = np.asarray(image, dtype=float) * fade[:, :, None]
    return edgeFade(Image.fromarray(pixels.astype(np.uint8)), 4)


def heatLight(image):
    image = image.convert("RGB").resize((116, 116), Image.Resampling.LANCZOS)
    mask = Image.new("L", (116, 116))
    ImageDraw.Draw(mask).rounded_rectangle((12, 12, 103, 103), radius=10, fill=255)
    mask = np.asarray(mask.filter(ImageFilter.GaussianBlur(4)), dtype=float) / 255
    pixels = np.asarray(image, dtype=float)
    # Keep the generated molten veins; boost the white-hot centre, retaining rounded-square falloff.
    y, x = np.indices((116, 116))
    centre = np.exp(-((x - 57.5) ** 2 + (y - 57.5) ** 2) / 650)
    pixels += centre[:, :, None] * np.array([110, 80, 42])
    pixels = np.maximum(pixels, centre[:, :, None] * np.array([255, 224, 160]))
    return edgeFade(Image.fromarray(np.clip(pixels * mask[:, :, None], 0, 255).astype(np.uint8)), 6)


def steamSheet(image):
    image = image.convert("RGB")
    result = Image.new("RGB", (1024, 512))
    for index in range(8):
        col, row = index % 4, index // 4
        box = (round(col * image.width / 4), round(row * image.height / 2),
               round((col + 1) * image.width / 4), round((row + 1) * image.height / 2))
        frame = image.crop(box).resize((256, 256), Image.Resampling.LANCZOS)
        frame = edgeFade(frame, 16)
        if index == 7:
            frame = Image.fromarray((np.asarray(frame, dtype=float) * 0.35).astype(np.uint8))
        result.paste(frame, (col * 256, row * 256))
    return result


def banner(image):
    # Fitted whole, at its own proportions; the code places the icon and the text on the painting as it is
    return fitSolid(image, (960, 220))
