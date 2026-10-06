"""Package the Gladiateur HUD paintings as registered PNGs, BLP2 textures and flipbooks.

Usage: python localTools/interface/buildGladiatorHudArt.py
Requires Pillow and NumPy. Does not build or patch the client.
"""

import json
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

from buildParagonArt import writeDxt3Blp, writeRawBlp
from gladiatorHudEffects import emberFrames
from gladiatorHudTextures import (
    bloodFrames, cleanBlack, packFrames, registerFamily, registerImage, registerRing, visibleBounds,
)


repoRoot = Path(__file__).resolve().parents[2]
assetRoot = repoRoot / "clientPatcher/assets/gladiatorHud"
sourceRoot = assetRoot / "source"
pngRoot = assetRoot / "png"
outputRoot = repoRoot / "clientPatcher/interface/Interface/ClassHud"


def loadSource(name):
    return Image.open(sourceRoot / (name + ".png")).convert("RGBA")


def saveAsset(name, image, atlas=False):
    image.save(pngRoot / (name + ".png"))
    writer = writeDxt3Blp if atlas else writeRawBlp
    writer(image.convert("RGBA"), str(outputRoot / (name + ".blp")))


def registerFx(name, targetDiameter, sourceDiameter):
    image = cleanBlack(loadSource(name))
    return registerImage(image, (image.width / 2, image.height / 2),
                         (sourceDiameter / 2, sourceDiameter / 2), (512, 512),
                         (targetDiameter / 2, targetDiameter / 2), (0, 0, 0))


def composePreview(assets, bleed, boss, medallion, active=False):
    result = Image.new("RGBA", (512, 512), (24, 21, 18, 255))
    result.alpha_composite(assets["gladiatorFrame"])
    result.alpha_composite(bleed)
    result.alpha_composite(assets[boss], (192, 189))
    if active:
        result.alpha_composite(assets["gladiatorBossCrack"], (192, 189))
        rgb = ImageChops.add(result.convert("RGB"), assets["gladiatorRimGlow"])
        rgb = ImageChops.add(rgb, assets["gladiatorLaurelFlare"])
        result = rgb.convert("RGBA")
    coin = assets[medallion].resize((56, 56), Image.Resampling.LANCZOS)
    result.alpha_composite(coin, (345, 336))
    duel = assets["gladiatorDuelLit" if active else "gladiatorDuelDark"]
    result.alpha_composite(duel.resize((96, 48), Image.Resampling.LANCZOS), (208, 12))
    return result


def main():
    pngRoot.mkdir(parents=True, exist_ok=True)
    outputRoot.mkdir(parents=True, exist_ok=True)
    assets = {}
    assets["gladiatorFrame"] = registerImage(loadSource("gladiatorFrame"), (626.5, 602),
                                            (484.5, 484.5), (512, 512), (190, 190))
    for family, states in (("Boss", ("Dark", "Gold")),
                           ("Medallion", ("Dark", "Lit", "CoupDeGrace"))):
        names = ["gladiator" + family + state for state in states]
        images = registerFamily([loadSource(name) for name in names], (128, 128), 112)
        assets.update(zip(names, images))
    crack = loadSource("gladiatorBossCrack")
    left, top, right, bottom = visibleBounds(crack, 16)
    crack = registerImage(crack, ((left + right) / 2, (top + bottom) / 2),
                          ((right - left) / 2, (bottom - top) / 2), (128, 128), (43, 43))
    assets["gladiatorBossCrack"] = crack
    duelNames = ["gladiatorDuelDark", "gladiatorDuelLit"]
    # Shared fit preserves shape and registration between Duel states.
    duelImages = [loadSource(name) for name in duelNames]
    left, top, right, bottom = visibleBounds(duelImages[0])
    scale = min(112 / (right - left), 52 / (bottom - top))
    for name, image in zip(duelNames, duelImages):
        assets[name] = registerImage(image, ((left + right) / 2, (top + bottom) / 2),
                                     (1 / scale, 1 / scale), (128, 64), (1, 1))
    assets["gladiatorRimGlow"] = registerFx("gladiatorRimGlow", 380, 943)
    assets["gladiatorLaurelFlare"] = registerFx("gladiatorLaurelFlare", 420, 1090)
    assets["gladiatorShockwave"] = cleanBlack(loadSource("gladiatorShockwave")).resize(
        (256, 256), Image.Resampling.LANCZOS)
    for name, image in assets.items():
        saveAsset(name, image)
    ring = registerRing(loadSource("gladiatorBlood15"), 512, 162, 8)
    blood = bloodFrames(ring)
    bloodRoot = pngRoot / "bloodFrames"
    bloodRoot.mkdir(exist_ok=True)
    for index, image in enumerate(blood):
        image.save(bloodRoot / ("blood" + str(index).zfill(2) + ".png"))
    saveAsset("gladiatorBloodFill", packFrames(blood, 4), atlas=True)
    embers = emberFrames(loadSource("gladiatorEmberFlipbook"))
    saveAsset("gladiatorEmberFlipbook", packFrames(embers, 8), atlas=True)
    embers[0].save(pngRoot / "emberFirst.png")
    embers[-1].save(pngRoot / "emberLast.png")
    embers[0].save(assetRoot / "emberLoop.gif", save_all=True, append_images=embers[1:],
                   duration=50, loop=0, optimize=False)
    previews = [composePreview(assets, blood[0], "gladiatorBossDark", "gladiatorMedallionDark"),
                composePreview(assets, blood[8], "gladiatorBossGold", "gladiatorMedallionLit"),
                composePreview(assets, blood[15], "gladiatorBossGold", "gladiatorMedallionCoupDeGrace", True)]
    sheet = Image.new("RGB", (1024, 512), (24, 21, 18))
    labels = ("Idle", "Opening / Execute ready", "Broken guard / Coup de grace")
    draw = ImageDraw.Draw(sheet)
    for index, (preview, label) in enumerate(zip(previews, labels)):
        sheet.paste(preview.resize((320, 320), Image.Resampling.LANCZOS), (index * 340, 24))
        sheet.paste(preview.resize((130, 130), Image.Resampling.LANCZOS), (index * 340 + 95, 355))
        draw.text((index * 340 + 16, 8), label, fill=(232, 194, 90))
    sheet.save(assetRoot / "statePreview.png")
    manifest = {
        "canvas": [512, 512], "shieldCentre": [256, 256], "shieldOuterDiameter": 380,
        "bloodRadius": 162, "bloodWidth": 8, "bossCentre": [256, 253], "bossSize": [128, 128],
        "medallionCentre": [373, 364], "medallionDrawSize": [56, 56],
        "duelCentre": [256, 36], "duelDrawSize": [96, 48],
        "blood": {"texture": "gladiatorBloodFill", "columns": 4, "rows": 4, "frames": 16,
                  "cellSize": 512, "percentages": [round(index / 15 * 100, 6) for index in range(16)],
                  "direction": "clockwise", "start": "sixOClock", "blendMode": "BLEND"},
        "embers": {"texture": "gladiatorEmberFlipbook", "columns": 8, "rows": 8, "frames": 64,
                   "cellSize": 256, "fps": 20, "blendMode": "ADD"},
        "additiveTextures": ["gladiatorRimGlow", "gladiatorLaurelFlare", "gladiatorShockwave"],
        "textureRoot": "Interface\\ClassHud\\", "integrationStatus": "assetsOnly",
    }
    (assetRoot / "assetManifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("Exported 14 PNG/BLP assets, 16 blood frames, state preview, and looping ember GIF")


if __name__ == "__main__":
    main()
