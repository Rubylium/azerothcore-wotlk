"""Deterministic registration and canvas assembly for painted Vorhan sources."""
import numpy as np
from PIL import Image, ImageFilter, ImageOps


def fitCanvas(image, size):
    return ImageOps.fit(image.convert("RGB"), size, method=Image.Resampling.LANCZOS)


def blackBackground(image):
    pixels = np.array(image.convert("RGB"))
    pixels[pixels.max(axis=2) <= 5] = 0
    return Image.fromarray(pixels)


def registerGaze(images):
    frames = [np.array(blackBackground(fitCanvas(image, (512, 512)))) for image in images]
    y, x = np.mgrid[:512, :512]
    eyeMask = ((x - 256) / 142) ** 2 + ((y - 200) / 133) ** 2 <= 1
    # The painted plate and chains stay fixed; the iris, shutters and red light animate.
    master = frames[0].astype(float)
    result = []
    for frame in frames:
        current = frame.astype(float)
        redLight = np.maximum(current[..., 0] - master[..., 0], 0)
        redMask = (current[..., 0] > current[..., 1] * 1.65) & (current[..., 0] > 45)
        combined = master.copy()
        combined[..., 0] += redLight * redMask
        combined[eyeMask] = current[eyeMask]
        result.append(blackBackground(Image.fromarray(np.clip(combined, 0, 255).astype("uint8"))))
    return result


def runeLayer(image):
    pixels = np.array(image.convert("RGB"))
    visible = pixels.max(axis=2) > 8
    y, x = np.where(visible)
    cropped = image.crop((x.min(), y.min(), x.max() + 1, y.max() + 1))
    ring = ImageOps.contain(cropped.convert("RGB"), (716, 716), Image.Resampling.LANCZOS)
    canvas = Image.new("RGB", (1024, 1024))
    canvas.paste(ring, ((1024 - ring.width) // 2, (1024 - ring.height) // 2))
    return blackBackground(canvas)


def quietPlaque(image):
    image = fitCanvas(image, (512, 512))
    y, x = np.mgrid[:512, :512]
    radius = np.sqrt((x - 255.5) ** 2 + (y - 255.5) ** 2)
    blend = np.clip((195 - radius) / 25, 0, 1)[..., None]
    original = np.array(image).astype(float)
    quiet = np.array(image.filter(ImageFilter.GaussianBlur(9))).astype(float) * 0.68
    return blackBackground(Image.fromarray((original * (1 - blend) + quiet * blend).astype("uint8")))


def tileSeam(pixels, seamWidth=24):
    pixels = pixels.astype(float)
    for offset in range(seamWidth):
        weight = (1 - offset / seamWidth) ** 2
        average = (pixels[:, offset] + pixels[:, -1 - offset]) / 2
        pixels[:, offset] = pixels[:, offset] * (1 - weight) + average * weight
        pixels[:, -1 - offset] = pixels[:, -1 - offset] * (1 - weight) + average * weight
    return np.clip(pixels, 0, 255).round().astype("uint8")


def registerWalls(images):
    frames = []
    for image in images:
        # Uniform scale after cropping the generated strip to its authored 4:1 canvas.
        height = round(image.width / 4)
        bottom = round(image.height * 0.86)
        crop = image.crop((0, bottom - height, image.width, bottom))
        frames.append(np.array(fitCanvas(crop, (1024, 256))))
    master = frames[0]
    red, green, blue = [master[..., channel].astype(float) for channel in range(3)]
    ironMask = (red > green * 0.56) & (blue > green * 0.40) & (master.max(axis=2) > 8)
    result = []
    for frame in frames:
        frame[ironMask] = master[ironMask]
        result.append(blackBackground(Image.fromarray(tileSeam(frame))))
    return result, ironMask
