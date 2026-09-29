"""Compiles the art of L'Infini's page on the challenge board (ChallengeBoard.lua, the L'Infini tab) into the BLP2
files the 3.3.5 client loads. The page is one scene filling the window: the Celestial Planetarium, the god standing
on its left and melting into it. Both pictures are paintings (localTools/interface/assets/challengeGod), shipped
uncompressed (the Paragon sky's writer): DXT would band their dark gradients.

- ChallengeGod-Backdrop   the Planetarium, mirrored so its calm side lies under the text, cut to the window's inside
                          (GOD_SCENE_* in ChallengeBoard.lua) and stretched into a 1024x1024 texture (the client
                          stretches it back)
- ChallengeGod-Figure     the god: the painting's top at the proportions the page shows it (GOD_FIGURE_*), its edges
                          faded out in the alpha (a texture cannot be masked in the client), in the top of a 512x1024
                          texture

Usage: python localTools/interface/buildChallengeGodArt.py
"""
import importlib.util
import os

import numpy
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SOURCE = os.path.join(HERE, "assets", "challengeGod")
OUT = os.path.join(REPO, "clientPatcher", "interface", "Interface", "ChallengeBoard")

# ChallengeBoard.lua: the window's inside, and the god's part of it (width / height)
SCENE_ASPECT = 836 / 565
FIGURE_ASPECT = 444 / 565
# How far in from each edge the god fades out, in shares of its width / height
FADE_RIGHT, FADE_BOTTOM, FADE_LEFT, FADE_TOP = 0.42, 0.3, 0.06, 0.06
STALE = ("ChallengeGod-Portrait",)


def load_blp_writer():
    path = os.path.join(HERE, "buildParagonArt.py")
    spec = importlib.util.spec_from_file_location("buildParagonArt", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.writeRawBlp


def smoothstep(values):
    values = numpy.clip(values, 0.0, 1.0)
    return values * values * (3.0 - 2.0 * values)


def figure():
    painting = Image.open(os.path.join(SOURCE, "portrait.png")).convert("RGBA")
    height = round(painting.width / FIGURE_ASPECT)
    crop = painting.crop((0, 0, painting.width, height))
    scaled = crop.resize((512, round(512 / FIGURE_ASPECT)), Image.Resampling.LANCZOS)

    rgba = numpy.asarray(scaled, dtype=numpy.float32)
    rows, columns = rgba.shape[:2]
    x = (numpy.arange(columns, dtype=numpy.float32) + 0.5) / columns
    y = (numpy.arange(rows, dtype=numpy.float32) + 0.5) / rows
    across = smoothstep((1.0 - x) / FADE_RIGHT) * smoothstep(x / FADE_LEFT)
    down = smoothstep((1.0 - y) / FADE_BOTTOM) * smoothstep(y / FADE_TOP)
    rgba[..., 3] *= down[:, None] * across[None, :]

    texture = Image.new("RGBA", (512, 1024), (0, 0, 0, 0))
    texture.paste(Image.fromarray(rgba.round().astype(numpy.uint8), "RGBA"), (0, 0))
    print(f"figure: {rows} of 1024 rows used (ChallengeBoard.lua GOD_FIGURE_BOTTOM {rows / 1024:.4f})")
    return texture


def backdrop():
    painting = Image.open(os.path.join(SOURCE, "backdrop.png")).convert("RGBA")
    painting = painting.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    width = min(painting.width, round(painting.height * SCENE_ASPECT))
    left = (painting.width - width) // 2
    crop = painting.crop((left, 0, left + width, painting.height))
    return crop.resize((1024, 1024), Image.Resampling.LANCZOS)


def main():
    write = load_blp_writer()
    os.makedirs(OUT, exist_ok=True)
    for name in STALE:
        for extension in (".png", ".blp"):
            path = os.path.join(OUT, name + extension)
            if os.path.exists(path):
                os.remove(path)
    for name, image in (("ChallengeGod-Figure", figure()), ("ChallengeGod-Backdrop", backdrop())):
        image.save(os.path.join(OUT, name + ".png"))
        write(image, os.path.join(OUT, name + ".blp"))
        print("wrote", name)


if __name__ == "__main__":
    main()
