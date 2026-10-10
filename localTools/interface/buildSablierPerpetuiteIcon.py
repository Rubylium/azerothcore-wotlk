"""Sablier de Perpétuité's icon (mod-legendary 27, Gardien-chef Vorhan's Unique): his own painted Perpétuité icon
(localTools/wardenVorhan/source/ICON_Perpetuite.png, the chained fel-iron hourglass) in the Unique's colours - its fel
green turned the Unique's crimson and red-black (as INV_Unique_EchoDuNeant), the hourglass itself left as painted.

Writes clientPatcher/assets/legendaryIcons/source/ and png/INV_Unique_SablierPerpetuite.png (256 x 256, opaque);
then python localTools/interface/buildLegendaryClientArt.py compiles it.

Usage: python localTools/interface/buildSablierPerpetuiteIcon.py
"""

from pathlib import Path

import numpy as np
from PIL import Image

REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "localTools" / "wardenVorhan" / "source" / "ICON_Perpetuite.png"
ICONS = REPO / "clientPatcher" / "assets" / "legendaryIcons"
NAME = "INV_Unique_SablierPerpetuite"

# The fel green: hues between these (degrees), the more saturated the more of it is turned
GREEN_FROM, GREEN_TO = 50.0, 190.0
# What it becomes: the Unique's crimson, darker than the green was (the red sand stands out), the smoke near black
CRIMSON_HUE = 358.0
GLOW_VALUE = 0.62
SMOKE_VALUE = 0.32


def rgb_to_hsv(rgb):
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    high = rgb.max(axis=-1)
    low = rgb.min(axis=-1)
    chroma = high - low
    hue = np.zeros_like(high)
    safe = np.where(chroma == 0, 1.0, chroma)
    hue = np.where(high == r, ((g - b) / safe) % 6.0, hue)
    hue = np.where(high == g, (b - r) / safe + 2.0, hue)
    hue = np.where(high == b, (r - g) / safe + 4.0, hue)
    hue = np.where(chroma == 0, 0.0, hue) * 60.0
    saturation = np.where(high == 0, 0.0, chroma / np.where(high == 0, 1.0, high))
    return hue, saturation, high


def hsv_to_rgb(hue, saturation, value):
    sector = (hue / 60.0) % 6.0
    chroma = value * saturation
    x = chroma * (1.0 - np.abs(sector % 2.0 - 1.0))
    zero = np.zeros_like(hue)
    index = np.floor(sector).astype(int)
    choices = [
        np.stack([chroma, x, zero], -1), np.stack([x, chroma, zero], -1), np.stack([zero, chroma, x], -1),
        np.stack([zero, x, chroma], -1), np.stack([x, zero, chroma], -1), np.stack([chroma, zero, x], -1),
    ]
    rgb = np.zeros(hue.shape + (3,))
    for sector_index, choice in enumerate(choices):
        rgb = np.where((index == sector_index)[..., None], choice, rgb)
    return rgb + (value - chroma)[..., None]


def main():
    image = Image.open(SOURCE).convert("RGB")
    rgb = np.asarray(image).astype(np.float64) / 255.0
    hue, saturation, value = rgb_to_hsv(rgb)
    # How green a pixel is: in the green's hues, weighted by its saturation (the hourglass's red and the iron's grey
    # stay as they are)
    in_green = np.clip(np.minimum(hue - GREEN_FROM, GREEN_TO - hue) / 15.0, 0.0, 1.0)
    weight = in_green * np.clip((saturation - 0.06) / 0.2, 0.0, 1.0)
    # Bright glow keeps most of its light, the smoke darkens towards red-black
    turned_value = value * (SMOKE_VALUE + (GLOW_VALUE - SMOKE_VALUE) * value)
    turned = hsv_to_rgb(np.full_like(hue, CRIMSON_HUE), np.clip(saturation * 1.05, 0.0, 1.0), turned_value)
    out = rgb * (1.0 - weight[..., None]) + turned * weight[..., None]
    result = Image.fromarray(np.clip(out * 255.0 + 0.5, 0, 255).astype(np.uint8), "RGB")
    result.save(ICONS / "source" / f"{NAME}.png")
    result.resize((256, 256), Image.LANCZOS).save(ICONS / "png" / f"{NAME}.png")
    print(f"{NAME}: written")


if __name__ == "__main__":
    main()
