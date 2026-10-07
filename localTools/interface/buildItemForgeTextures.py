"""The Forge's painted art (clientPatcher/assets/itemForge/png, .agents/plans/item-forge/item-forge.ASSETS.md) as the
BLP textures the window loads (Interface/ItemForge). Every piece is placed on its power-of-two canvas at its own size:
nothing is resized, except the three sheets reused from the launcher, scaled down uniformly (one factor for both
axes). The places written here are the ones ItemForge.lua reads its texture coordinates from (ART there).

Usage: python localTools/interface/buildItemForgeTextures.py
"""

from pathlib import Path

from PIL import Image

from buildParagonArt import writeDxt3Blp, writeRawBlp

REPO = Path(__file__).resolve().parents[2]
ART = REPO / "clientPatcher" / "assets" / "itemForge"
OUT = REPO / "clientPatcher" / "interface" / "Interface" / "ItemForge"

# The small pieces' atlas (1024 x 512): name -> top left corner; each keeps its delivered size
ATLAS = {
    "medallion": (4, 4),
    "rankCold": (132, 4), "rankLit": (184, 4), "rankGold": (236, 4),
    "rankFlare": (288, 4),
    "pieceHeat": (388, 4),
    "rowPlate": (4, 132), "rowSelected": (4, 220),
    "gaugeFill": (4, 308),
    "goldenBurst": (580, 4),
}


# Where the hammer's handle ends, as a fraction of the painting: the point it swings around
HAMMER_PIVOT = (0.88, 0.80)


def png(name):
    return Image.open(ART / "png" / f"{name}.png").convert("RGBA")


def canvas(size, *pieces):
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    for piece, place in pieces:
        image.paste(piece, place)
    return image


def uniform(image, factor):
    return image.resize((round(image.width * factor), round(image.height * factor)), Image.Resampling.LANCZOS)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    # The anvil stage, both heats, and the fire's light (868 x 480 on 1024 x 512)
    writeDxt3Blp(canvas((1024, 512), (png("stageCold"), (0, 0))), OUT / "ForgeStageCold.blp")
    writeDxt3Blp(canvas((1024, 512), (png("stageLit"), (0, 0))), OUT / "ForgeStageLit.blp")
    writeRawBlp(canvas((1024, 512), (png("fireLight"), (0, 0))), OUT / "ForgeFireLight.blp")
    # The quench's steam: 8 cells of 256, already its own power-of-two sheet
    writeRawBlp(png("steam"), OUT / "ForgeSteam.blp")
    # The masterpiece banner (960 x 220 on 1024 x 256)
    writeRawBlp(canvas((1024, 256), (png("masterpieceBanner"), (0, 0))), OUT / "ForgeBanner.blp")

    small = canvas((1024, 512), *((png(name), place) for name, place in ATLAS.items()))
    writeRawBlp(small, OUT / "ForgeAtlas.blp")

    reuse = ART / "reference" / "launcherReuse"
    # The launcher's sheets at a half and a quarter: 4 x 4 cells of 256 (sparks) and 64 (embers)
    writeDxt3Blp(uniform(Image.open(reuse / "sparkBurst.png").convert("RGBA"), 0.5), OUT / "ForgeSparks.blp")
    writeRawBlp(uniform(Image.open(reuse / "embers.png").convert("RGBA"), 0.25), OUT / "ForgeEmbers.blp")
    # The hammer (1200 x 900) at 0.19, with the end of its handle (HAMMER_PIVOT of it) at the centre of a 512
    # square: the code swings it by turning its texture coordinates around that centre, and every part of it stays
    # within the square's inscribed circle, so no angle clips it
    hammer = uniform(Image.open(reuse / "hammer.png").convert("RGBA"), 0.19)
    pivot = (round(hammer.width * HAMMER_PIVOT[0]), round(hammer.height * HAMMER_PIVOT[1]))
    writeRawBlp(canvas((512, 512), (hammer, (256 - pivot[0], 256 - pivot[1]))), OUT / "ForgeHammer.blp")
    print("hammer", hammer.size, "pivot", pivot)

    for path in sorted(OUT.glob("*.blp")):
        print(f"{path.name}: {path.stat().st_size // 1024} KB")


if __name__ == "__main__":
    main()
