"""Exact canvases, shared heat-state silhouettes and seamless forge texture exports."""

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter

from gladiatorHudV2Assembly import cleanTransparent, mirrorLeft, solidBounds


sceneSize = (2560, 1520)


def fadeEdges(image, margin=12):
    pixels = np.asarray(image.convert("RGB")).astype(float)
    y, x = np.indices((image.height, image.width), dtype=float)
    fade = np.clip(np.minimum.reduce((x, image.width - 1 - x, y, image.height - 1 - y)) / margin, 0, 1)
    pixels *= fade[:, :, None]
    pixels[pixels.max(axis=2) < 4] = 0
    return Image.fromarray(np.rint(pixels).astype(np.uint8))


def coldRelight(hot, cold):
    """Transfer the edit's broad cold lighting onto the hot master's unshifted geometry/detail."""
    cold = cold.resize(hot.size, Image.Resampling.LANCZOS).convert("RGB")
    hotGrey = hot.convert("RGB").convert("L")
    detail = (np.asarray(hotGrey).astype(float) + 12) / (
        np.asarray(hotGrey.filter(ImageFilter.GaussianBlur(9))).astype(float) + 12)
    coldLight = np.asarray(cold.convert("L").filter(ImageFilter.GaussianBlur(9))).astype(float)
    light = np.clip(coldLight * np.clip(detail, 0.5, 1.8), 0, 180)
    pixels = np.stack((light * 0.79, light * 0.90, light), axis=2)
    result = Image.fromarray(np.rint(pixels).astype(np.uint8))
    if hot.mode == "RGBA":
        result = result.convert("RGBA")
        result.putalpha(hot.getchannel("A"))
        result = cleanTransparent(result)
    return result


def fitRegistered(hot, cold, size, target):
    bounds = solidBounds(hot)
    cropped = hot.crop(bounds)
    scale = min((target[2] - target[0]) / cropped.width, (target[3] - target[1]) / cropped.height)
    fittedSize = (round(cropped.width * scale), round(cropped.height * scale))
    position = (round((target[0] + target[2] - fittedSize[0]) / 2),
                round((target[1] + target[3] - fittedSize[1]) / 2))
    hotFit = cropped.resize(fittedSize, Image.Resampling.LANCZOS)
    hotFit.putalpha(hotFit.getchannel("A").point(lambda value: 0 if value <= 2 else value))
    coldFit = coldRelight(cropped, cold.crop(bounds)).resize(fittedSize, Image.Resampling.LANCZOS)
    coldFit.putalpha(hotFit.getchannel("A"))
    result = []
    for painting in (hotFit, coldFit):
        canvas = Image.new("RGBA", size, 0)
        canvas.paste(painting, position)
        result.append(cleanTransparent(canvas))
    return result, {"sourceBounds": list(bounds), "size": list(fittedSize), "position": list(position)}


def fitSolid(source, size, padding):
    source = source.crop(solidBounds(source))
    source.thumbnail((size[0] - padding * 2, size[1] - padding * 2), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", size, 0)
    canvas.paste(source, ((size[0] - source.width) // 2, (size[1] - source.height) // 2))
    canvas.putalpha(canvas.getchannel("A").point(lambda value: 0 if value <= 2 else value))
    return cleanTransparent(canvas)


def calmWall(source):
    pixels = np.asarray(source.convert("RGB").resize(sceneSize, Image.Resampling.LANCZOS)).astype(float)
    y, x = np.indices((1520, 2560), dtype=float)
    left = 0.60 + 0.40 * np.clip((x - 1000) / 220, 0, 1)
    lower = 1 - 0.60 * np.clip((y - 1090) / 100, 0, 1)
    return Image.fromarray(np.rint(pixels * (left * lower)[:, :, None]).astype(np.uint8))


def tileHorizontal(source, size, opaque=False):
    if source.mode == "RGBA":
        bounds = solidBounds(source)
        source = source.crop((0, bounds[1], source.width, bounds[3]))
    image = source.resize(size, Image.Resampling.LANCZOS)
    pixels = np.asarray(image).astype(float)
    seamWidth = min(96, size[0] // 8)
    average = (pixels[:, 0] + pixels[:, -1]) / 2
    for x in range(seamWidth):
        factor = max(0, (x - 3) / (seamWidth - 4))
        pixels[:, x] = average * (1 - factor) + pixels[:, x] * factor
        pixels[:, -1 - x] = average * (1 - factor) + pixels[:, -1 - x] * factor
    result = Image.fromarray(np.rint(pixels).astype(np.uint8))
    return result if opaque else cleanTransparent(result)


def particleSheet(source, size, cellSize, registerBurst=False):
    painting = source.convert("RGB").resize((size, size), Image.Resampling.LANCZOS)
    sheet = Image.new("RGB", (size, size), 0)
    cells = []
    lastOffset = (0, 0)
    for index in range(16):
        x, y = (index % 4) * cellSize, (index // 4) * cellSize
        cell = painting.crop((x, y, x + cellSize, y + cellSize))
        if registerBurst:
            if index < 7:
                pixels = np.asarray(cell).astype(float).mean(axis=2)
                core = pixels[160:352, 160:352]
                coreY, coreX = np.nonzero(core > core.max() * 0.95)
                lastOffset = (round(256 - coreX.mean() - 160), round(256 - coreY.mean() - 160))
            registered = Image.new("RGB", (cellSize, cellSize), 0)
            registered.paste(cell, lastOffset)
            cell = registered
        cell = fadeEdges(cell, 8)
        sheet.paste(cell, (x, y))
        cells.append(cell)
    return sheet, cells


def makeFireLight(source, anvil):
    light = source.convert("RGB").resize(sceneSize, Image.Resampling.LANCZOS)
    # Destroy the generator's accidental floor texture; only soft illumination survives.
    light = light.filter(ImageFilter.GaussianBlur(36))
    pixels = np.asarray(light).astype(float)
    y, x = np.indices((1520, 2560), dtype=float)
    mask = np.clip((x - 1152) / 180, 0, 1) * np.clip((1124 - y) / 130, 0, 1)
    pixels *= (mask * 0.30)[:, :, None]
    blade = np.asarray(anvil)[:, :, :3].astype(float)
    hot = (blade[:, :, 0] > blade[:, :, 1] * 1.25) & (blade[:, :, 0] > 150)
    bladeLight = np.zeros_like(blade, dtype=np.uint8)
    bladeLight[hot] = blade[hot].astype(np.uint8)
    bladeLight = Image.fromarray(bladeLight).filter(ImageFilter.GaussianBlur(16))
    pixels += np.asarray(bladeLight) * 0.35
    pixels *= mask[:, :, None]
    return fadeEdges(Image.fromarray(np.clip(pixels, 0, 255).astype(np.uint8)))


def makeIngotGlow(source, ingot):
    glow = source.convert("RGB").resize((1600, 640), Image.Resampling.LANCZOS)
    glow = mirrorLeft(glow.filter(ImageFilter.GaussianBlur(10)))
    alpha = Image.new("L", (1600, 640), 0)
    alpha.paste(ingot.getchannel("A"), (200, 120))
    outer = alpha.filter(ImageFilter.MaxFilter(99)).filter(ImageFilter.GaussianBlur(20))
    ring = ImageChops.subtract(outer, alpha.filter(ImageFilter.MinFilter(3)))
    colour = np.asarray(glow).astype(float)
    # The outer mask supplies the exact button registration; broad source colour supplies the painted heat.
    warmth = np.maximum(colour, np.array([120, 45, 5]))
    pixels = warmth * (np.asarray(ring) / 255)[:, :, None]
    return fadeEdges(Image.fromarray(np.clip(pixels, 0, 255).astype(np.uint8)), 24)


def assembleAssets(sources):
    wallHot = calmWall(sources["backWallHot"])
    wallCold = coldRelight(wallHot, calmWall(sources["backWallCold"]))
    anvils, anvilTransform = fitRegistered(sources["anvilHot"], sources["anvilCold"], sceneSize,
                                          (1485, 655, 2458, 1110))
    ingots, ingotTransform = fitRegistered(sources["ingotHot"], sources["ingotCold"], (1200, 400),
                                          (60, 60, 1140, 340))
    ingots = [cleanTransparent(mirrorLeft(image)) for image in ingots]
    foreground = cleanTransparent(sources["foreground"].resize(sceneSize, Image.Resampling.LANCZOS))
    foregroundPixels = np.asarray(foreground).copy()
    foregroundPixels[:, :, :3] = np.minimum(foregroundPixels[:, :, :3], 60)
    foreground = cleanTransparent(Image.fromarray(foregroundPixels))
    smoke = sources["smoke"].convert("RGB").resize((2560, 760), Image.Resampling.LANCZOS)
    smoke = tileHorizontal(fadeEdges(smoke, 20), (2560, 760), opaque=True)
    sparks, sparkCells = particleSheet(sources["sparkBurst"], 2048, 512, registerBurst=True)
    embers, emberCells = particleSheet(sources["embers"], 1024, 256)
    header = tileHorizontal(sources["headerBand"], (2560, 176))
    headerCanvas = Image.new("RGBA", (2560, 192), 0)
    headerCanvas.paste(header, (0, 8))
    assets = {
        "backWallHot": wallHot, "backWallCold": wallCold,
        "anvilHot": anvils[0], "anvilCold": anvils[1], "foreground": foreground,
        "fireLight": makeFireLight(sources["fireLight"], anvils[0]), "smoke": smoke,
        "ingotHot": ingots[0], "ingotCold": ingots[1],
        "ingotGlow": makeIngotGlow(sources["ingotGlow"], ingots[0]),
        "sparkBurst": sparks, "embers": embers,
        "hammer": fitSolid(sources["hammer"], (1200, 900), 48),
        "emblem": cleanTransparent(mirrorLeft(fitSolid(sources["emblem"], (1024, 1024), 40))),
        "ironPlate": cleanTransparent(mirrorLeft(fitSolid(sources["ironPlate"], (1024, 512), 24))),
        "headerBand": cleanTransparent(headerCanvas),
    }
    # Lanczos can overshoot by a single channel value at sharp boundaries; keep cold hues strictly ordered.
    for name in ("anvilCold", "ingotCold"):
        pixels = np.asarray(assets[name]).copy()
        pixels[:, :, 1] = np.minimum(pixels[:, :, 1], pixels[:, :, 2])
        pixels[:, :, 0] = np.minimum(pixels[:, :, 0], pixels[:, :, 1])
        assets[name] = cleanTransparent(Image.fromarray(pixels))
    return assets, {"anvil": anvilTransform, "ingot": ingotTransform}, sparkCells, emberCells
