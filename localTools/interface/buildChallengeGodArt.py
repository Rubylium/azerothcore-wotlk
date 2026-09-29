"""Compiles the art of L'Infini's page on the challenge board (ChallengeBoard.lua, the L'Infini tab) into the BLP2
files the 3.3.5 client loads. Both pictures are paintings (localTools/interface/assets/challengeGod), shipped
uncompressed (the Paragon sky's writer): DXT would band their dark gradients.

- ChallengeGod-Portrait   the god, the page's left card: the painting's top (the card's proportions, GOD_PORTRAIT_*
                          in ChallengeBoard.lua) scaled into the top of a 512x1024 texture
- ChallengeGod-Backdrop   the Celestial Planetarium behind the whole page, mirrored so its calm side lies under the
                          text, the page's proportions cut from its middle, in a 1024x512 texture

Usage: python localTools/interface/buildChallengeGodArt.py
"""
import importlib.util
import os

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SOURCE = os.path.join(HERE, "assets", "challengeGod")
OUT = os.path.join(REPO, "clientPatcher", "interface", "Interface", "ChallengeBoard")

# The card and the page as ChallengeBoard.lua lays them out (width / height)
PORTRAIT_ASPECT = 272 / 346
PAGE_ASPECT = 792 / 370


def load_blp_writer():
    path = os.path.join(HERE, "buildParagonArt.py")
    spec = importlib.util.spec_from_file_location("buildParagonArt", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.writeRawBlp


def portrait():
    painting = Image.open(os.path.join(SOURCE, "portrait.png")).convert("RGBA")
    height = round(painting.width / PORTRAIT_ASPECT)
    crop = painting.crop((0, 0, painting.width, height))
    texture = Image.new("RGBA", (512, 1024), (0, 0, 0, 255))
    scaled = crop.resize((512, round(512 / PORTRAIT_ASPECT)), Image.Resampling.LANCZOS)
    texture.paste(scaled, (0, 0))
    print(f"portrait: {scaled.height} of 1024 rows used (ChallengeBoard.lua GOD_PORTRAIT_BOTTOM "
          f"{scaled.height / 1024:.4f})")
    return texture


def backdrop():
    painting = Image.open(os.path.join(SOURCE, "backdrop.png")).convert("RGBA")
    painting = painting.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    height = round(painting.width / PAGE_ASPECT)
    top = (painting.height - height) // 2
    crop = painting.crop((0, top, painting.width, top + height))
    return crop.resize((1024, 512), Image.Resampling.LANCZOS)


def main():
    write = load_blp_writer()
    os.makedirs(OUT, exist_ok=True)
    for name, image in (("ChallengeGod-Portrait", portrait()), ("ChallengeGod-Backdrop", backdrop())):
        image.save(os.path.join(OUT, name + ".png"))
        write(image, os.path.join(OUT, name + ".blp"))
        print("wrote", name)


if __name__ == "__main__":
    main()
