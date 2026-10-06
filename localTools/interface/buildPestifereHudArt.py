"""Package the Pestifere tank HUD paintings into PNGs, a BLP2 atlas, and native-size previews.

Usage: python localTools/interface/buildPestifereHudArt.py
Requires Pillow and NumPy. No server build or client patch is performed.
"""

import json

import numpy as np
from pathlib import Path

from PIL import Image, ImageDraw

from buildGladiatorHudV2Art import addFx, place
from buildParagonArt import writeRawBlp
from pestifereHudAssembly import buildAssets, partialFlask


repoRoot = Path(__file__).resolve().parents[2]
assetRoot = repoRoot / "clientPatcher/assets/pestifereHud"
pngRoot = assetRoot / "png"
blpPath = repoRoot / "clientPatcher/interface/Interface/ClassHud/pestifereHudAtlas.blp"
pieceBoxes = {
    "frame": [0, 512, 0, 224], "bileDrips": [512, 1024, 0, 224],
    "flaskEmpty": [0, 132, 228, 348], "flaskBone": [136, 268, 228, 348],
    "flaskFlesh": [272, 404, 228, 348], "flaskBile": [408, 540, 228, 348],
    "boiling": [544, 676, 228, 348], "splash": [680, 808, 228, 356],
    "glowToxic": [0, 212, 360, 468], "glowSepulcre": [216, 428, 360, 468],
    "boilFlat": [432, 496, 360, 424], "boilSmall": [500, 564, 360, 424],
    "boilSwollen": [568, 632, 360, 424], "boilRipe": [636, 700, 360, 424],
}


def loadSource(name):
    return Image.open(assetRoot / ("source/" + name + ".png")).convert("RGBA")


def composeState(assets, mask, bounds, levels, boil="boilFlat", avatar=False, fievre=False,
                 sepulcre=False, splash=False, scale=1):
    result = Image.new("RGBA", (162 * scale, 70 * scale), (31, 27, 22, 255))

    def draw(image, texture, centre, size):
        place(image, texture, tuple(value * scale for value in centre), tuple(value * scale for value in size))

    def fx(image, name, centre, size, intensity):
        return addFx(image, assets[name], tuple(value * scale for value in centre),
                     tuple(value * scale for value in size), intensity)

    if sepulcre:
        result = fx(result, "glowSepulcre", (81, 33), (156, 68), 0.9)
    if avatar:
        result = fx(result, "glowToxic", (81, 33), (146, 60), 0.4)
    draw(result, assets["frame"], (81, 43), (154, 63))
    if fievre:
        result = fx(result, "bileDrips", (81, 43), (154, 63), 0.7)
    draw(result, assets[boil], (81, 52), (12, 12))
    flaskNames = ["flaskBone", "flaskFlesh", "flaskBile"]
    for index in (2, 1, 0):
        flask = partialFlask(assets["flaskEmpty"], assets[flaskNames[index]], mask, bounds, levels[index])
        draw(result, flask, (41 + index * 38, 33), (40, 37))
        if avatar and levels[index] > 0:
            result = fx(result, "boiling", (41 + index * 38, 33), (40, 37), 0.55)
    if splash:
        result = fx(result, "splash", (81, 52), (36, 36), 0.85)
    return result.convert("RGB")


def makePreviews(assets, mask, bounds):
    cases = [
        ("Idle", [0, 0, 0], {}),
        ("Three plagues / swollen", [1, 1, 1], {"boil": "boilSwollen"}),
        ("Liquid levels 100 / 65 / 35%", [1, 0.65, 0.35], {"boil": "boilSmall"}),
        ("Avatar + Fievre / ripe", [1, 1, 1], {"boil": "boilRipe", "avatar": True, "fievre": True}),
        ("Sepulcre", [1, 0.75, 0.6], {"boil": "boilSwollen", "sepulcre": True}),
        ("Detonation", [1, 1, 1], {"splash": True}),
    ]
    sheet = Image.new("RGB", (810, 810), (31, 27, 22))
    text = ImageDraw.Draw(sheet)
    for index, (label, levels, options) in enumerate(cases):
        native = composeState(assets, mask, bounds, levels, **options)
        native.save(assetRoot / ("preview" + str(index) + ".png"))
        enlarged = composeState(assets, mask, bounds, levels, **options, scale=3)
        x, y = index % 2 * 405, index // 2 * 270
        text.text((x + 12, y + 10), label + " - 162 x 70 below", fill=(232, 194, 90))
        sheet.paste(enlarged.resize((388, 168), Image.Resampling.LANCZOS), (x + 8, y + 30))
        sheet.paste(native, (x + 122, y + 199))
    sheet.save(assetRoot / "statePreview.png")
    boils = Image.new("RGB", (512, 160), (31, 27, 22))
    text = ImageDraw.Draw(boils)
    for index, name in enumerate(("boilFlat", "boilSmall", "boilSwollen", "boilRipe")):
        image = assets[name].resize((112, 112), Image.Resampling.LANCZOS)
        boils.paste(image, (index * 128 + 8, 24), image)
        text.text((index * 128 + 12, 8), name, fill=(232, 194, 90))
    boils.save(assetRoot / "boilPreview.png")


ADDITIVE = ("boiling", "glowToxic", "glowSepulcre", "bileDrips", "splash")


def atlasPiece(name, image):
    """A light-only piece is transparent where it is black: opaque black next to the frame (and across the atlas's
    wrap, the drips' right edge beside the frame's left one) bled into it in the client's smaller mip levels - a dark
    line along the left cleaver in game. ADD blends by the source alpha, so lit pixels stay opaque."""
    image = image.convert("RGBA")
    if name not in ADDITIVE:
        return image
    pixels = np.asarray(image).copy()
    pixels[:, :, 3] = np.where(pixels[:, :, :3].max(axis=2) > 0, 255, 0)
    return Image.fromarray(pixels)


def main():
    pngRoot.mkdir(parents=True, exist_ok=True)
    assets, mask, bounds, face = buildAssets(loadSource)
    mask.save(assetRoot / "flaskFillMask.png")
    face.save(assetRoot / "boilFaceMask.png")
    atlas = Image.new("RGBA", (1024, 512), 0)
    for name, image in assets.items():
        left, right, top, bottom = pieceBoxes[name]
        if image.size != (right - left, bottom - top):
            raise ValueError(name + " does not fit atlas cell")
        image.save(pngRoot / (name + ".png"))
        atlas.paste(atlasPiece(name, image), (left, top))
    atlas.save(assetRoot / "pestifereHudAtlas.png")
    writeRawBlp(atlas, str(blpPath))
    manifest = {
        "atlas": "Interface\\ClassHud\\pestifereHudAtlas", "atlasSize": [1024, 512],
        "pieceBoxes": pieceBoxes, "boxOrder": ["left", "right", "top", "bottom"],
        "additivePieces": ["boiling", "glowToxic", "glowSepulcre", "bileDrips", "splash"],
        "hudSize": [162, 70], "frameSize": [154, 63], "frameOffset": [0, -8],
        "flaskSize": [40, 37], "flaskOffsets": [[-40, 2], [-2, 2], [36, 2]],
        "plagueOrder": ["carapace", "chair", "peste"], "flaskOverlapOrder": ["left", "middle", "right"],
        "flaskFillBounds": bounds, "liquidClipDirection": "topDown",
        "boilSize": [12, 12], "boilOffset": [0, -17],
        "boilStackStates": {"0": "boilFlat", "1-2": "boilSmall", "3-5": "boilSwollen", "6": "boilRipe"},
        "glowSize": [146, 60], "glowOffset": [0, 2], "integrationStatus": "assetsOnly",
    }
    (assetRoot / "assetManifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    makePreviews(assets, mask, bounds)
    print("Exported 14 PNG assets, 1024x512 PNG/BLP2 atlas, masks, six native-size states and boil evolution")


if __name__ == "__main__":
    main()
