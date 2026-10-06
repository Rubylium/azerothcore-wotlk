"""Export the compact three-helmet Gladiateur HUD v2 without changing the v1 assets or runtime HUD.

Usage: python localTools/interface/buildGladiatorHudV2Art.py
Requires Pillow and NumPy. Paintings are retained in clientPatcher/assets/gladiatorHud/v2/source.
"""

import json
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

from buildParagonArt import writeRawBlp
from gladiatorHudV2Assembly import (
    cleanTransparent, helmetStates, jewelStates, mirrorLeft, registerEnergy, socketGlow,
)


repoRoot = Path(__file__).resolve().parents[2]
assetRoot = repoRoot / "clientPatcher/assets/gladiatorHud/v2"
sourceRoot = assetRoot / "source"
pngRoot = assetRoot / "png"
blpPath = repoRoot / "clientPatcher/interface/Interface/ClassHud/gladiatorHudV2Atlas.blp"

pieceBoxes = {
    "frame": [0, 512, 0, 224], "bladeEnergy": [512, 1024, 0, 224],
    "helmetEmpty": [0, 152, 228, 336], "helmetOneThird": [156, 308, 228, 336],
    "helmetTwoThirds": [312, 464, 228, 336], "helmetFull": [468, 620, 228, 336],
    "helmetGold": [624, 776, 228, 336],
    "glowCrimson": [0, 212, 340, 448], "glowGold": [216, 428, 340, 448],
    "jewelDark": [432, 496, 340, 404], "jewelGold": [500, 564, 340, 404],
    "jewelCrack": [568, 632, 340, 404],
}


def loadSource(name):
    return Image.open(sourceRoot / (name + ".png")).convert("RGBA")


def place(image, texture, centre, size):
    texture = texture.resize(size, Image.Resampling.LANCZOS)
    image.alpha_composite(texture, (round(centre[0] - size[0] / 2), round(centre[1] - size[1] / 2)))


def addFx(image, texture, centre, size, intensity=1):
    layer = Image.new("RGB", image.size, 0)
    texture = texture.resize(size, Image.Resampling.LANCZOS).point(lambda value: round(value * intensity))
    layer.paste(texture, (round(centre[0] - size[0] / 2), round(centre[1] - size[1] / 2)))
    return ImageChops.add(image.convert("RGB"), layer).convert("RGBA")


def composeState(assets, helmetNames, jewel="jewelDark", glow=None, energy=False, broken=False, scale=1):
    result = Image.new("RGBA", (162 * scale, 70 * scale), (31, 27, 22, 255))

    def scaledPlace(image, name, centre, size):
        place(image, assets[name], tuple(value * scale for value in centre),
              tuple(value * scale for value in size))

    def scaledFx(image, name, centre, size, intensity):
        return addFx(image, assets[name], tuple(value * scale for value in centre),
                     tuple(value * scale for value in size), intensity)

    if glow:
        result = scaledFx(result, glow, (81, 33), (146, 60), 0.35)
    scaledPlace(result, "frame", (81, 43), (154, 63))
    if energy:
        result = scaledFx(result, "bladeEnergy", (81, 43), (154, 63), 0.8)
    scaledPlace(result, jewel, (81, 52), (10, 10))
    if broken:
        scaledPlace(result, "jewelCrack", (81, 52), (10, 10))
    # Render right first, left last: the left socket owns overlaps, exactly as on ClassHudReaper.
    for index in (2, 1, 0):
        scaledPlace(result, helmetNames[index], (46 + index * 34, 33), (56, 40))
    return result.convert("RGB")


def makePreviews(assets):
    cases = [
        ("Idle", ["helmetEmpty"] * 3, {}),
        ("1/9 bleed", ["helmetOneThird", "helmetEmpty", "helmetEmpty"], {}),
        ("5/9 bleed + Opening", ["helmetFull", "helmetTwoThirds", "helmetEmpty"], {"jewel": "jewelGold"}),
        ("Ripe", ["helmetFull"] * 3, {"jewel": "jewelGold", "glow": "glowCrimson"}),
        ("Coup de grace", ["helmetGold"] * 3, {"jewel": "jewelGold", "glow": "glowGold", "energy": True}),
        ("Broken guard", ["helmetGold"] * 3,
         {"jewel": "jewelGold", "glow": "glowGold", "energy": True, "broken": True}),
    ]
    sheet = Image.new("RGB", (810, 810), (31, 27, 22))
    draw = ImageDraw.Draw(sheet)
    for index, (label, helmets, options) in enumerate(cases):
        preview = composeState(assets, helmets, **options)
        preview.save(assetRoot / ("preview" + str(index) + ".png"))
        column, row = index % 2, index // 2
        x, y = column * 405, row * 270
        draw.text((x + 12, y + 10), label + " - 162 x 70 below", fill=(232, 194, 90))
        enlarged = composeState(assets, helmets, **options, scale=3)
        sheet.paste(enlarged.resize((388, 168), Image.Resampling.LANCZOS), (x + 8, y + 30))
        sheet.paste(preview, (x + 122, y + 199))
    sheet.save(assetRoot / "statePreview.png")
    comparison = Image.new("RGB", (648, 560), (31, 27, 22))
    comparison.paste(composeState(assets, ["helmetEmpty"] * 3, scale=4), (0, 0))
    empowered = composeState(assets, ["helmetGold"] * 3, jewel="jewelGold", glow="glowGold", energy=True, scale=4)
    comparison.paste(empowered, (0, 280))
    comparison.save(assetRoot / "idleVsEmpowered.png")


def main():
    pngRoot.mkdir(parents=True, exist_ok=True)
    assets = {}
    assets["frame"] = cleanTransparent(mirrorLeft(loadSource("frame").resize((512, 224),
                                                                             Image.Resampling.LANCZOS)))
    helmets = ["helmetEmpty", "helmetOneThird", "helmetTwoThirds", "helmetFull", "helmetGold"]
    states, mask, fillBounds = helmetStates([loadSource(name) for name in helmets])
    assets.update(zip(helmets, states))
    mask.save(assetRoot / "helmetFillMask.png")
    jewels = ["jewelDark", "jewelGold", "jewelCrack"]
    assets.update(zip(jewels, jewelStates([loadSource(name) for name in jewels])))
    assets["bladeEnergy"] = registerEnergy(loadSource("bladeEnergy"), assets["frame"])
    for name in ("glowCrimson", "glowGold"):
        assets[name] = socketGlow(loadSource(name))
    atlas = Image.new("RGBA", (1024, 512), 0)
    for name, image in assets.items():
        image.save(pngRoot / (name + ".png"))
        left, right, top, bottom = pieceBoxes[name]
        if image.size != (right - left, bottom - top):
            raise ValueError(name + " does not fit its atlas cell")
        atlas.paste(image.convert("RGBA"), (left, top))
    atlas.save(assetRoot / "gladiatorHudV2Atlas.png")
    writeRawBlp(atlas, str(blpPath))
    manifest = {
        "version": 2, "atlas": "Interface\\ClassHud\\gladiatorHudV2Atlas", "atlasSize": [1024, 512],
        "pieceBoxes": pieceBoxes, "boxOrder": ["left", "right", "top", "bottom"],
        "additivePieces": ["glowCrimson", "glowGold", "bladeEnergy"],
        "hudSize": [162, 70], "frameSize": [154, 63], "frameOffset": [0, -8],
        "helmetSize": [56, 40], "helmetOffsets": [[-35, 2], [-1, 2], [33, 2]],
        "helmetOverlapOrder": ["left", "middle", "right"], "helmetFillBounds": fillBounds,
        "jewelSize": [10, 10], "jewelOffset": [0, -17], "glowSize": [146, 60], "glowOffset": [0, 2],
        "shockwave": "Interface\\ClassHud\\gladiatorShockwave", "integrationStatus": "assetsOnly",
    }
    (assetRoot / "assetManifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    makePreviews(assets)
    print("Exported 12 v2 PNGs, 1024x512 PNG/BLP2 atlas, exact-size state previews, and placement manifest")


if __name__ == "__main__":
    main()
