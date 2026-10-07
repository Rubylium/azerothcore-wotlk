"""Assemble the revised 15-asset Item Forge delivery; no atlas/runtime integration."""

import json
from pathlib import Path

from PIL import Image, ImageDraw

from itemForgeAssembly import (
    banner, fireLight, fitSolid, gaugeFill, heatedRow, heatLight, radialLight, stageFamily, stateFamily, steamSheet,
)
from itemForgeValidation import validateAssets


repoRoot = Path(__file__).resolve().parents[2]
assetRoot = repoRoot / "clientPatcher/assets/itemForge"
assetSpecs = {
    "stageCold": ((868, 480), "RGB"), "stageLit": ((868, 480), "RGB"),
    "fireLight": ((868, 480), "RGB"), "medallion": ((120, 120), "RGBA"),
    "rowPlate": ((536, 84), "RGBA"), "rowSelected": ((536, 84), "RGBA"),
    "gaugeFill": ((464, 20), "RGB"), "rankCold": ((48, 48), "RGBA"),
    "rankLit": ((48, 48), "RGBA"), "rankGold": ((48, 48), "RGBA"),
    "rankFlare": ((96, 96), "RGB"), "pieceHeat": ((116, 116), "RGB"),
    "steam": ((1024, 512), "RGB"), "masterpieceBanner": ((960, 220), "RGBA"),
    "goldenBurst": ((440, 440), "RGB"),
}


def source(name):
    return Image.open(assetRoot / "source" / f"{name}.png")


def contactSheet(assets):
    sheet = Image.new("RGB", (1120, 1220), (18, 16, 15))
    draw = ImageDraw.Draw(sheet)
    draw.text((20, 12), "ITEM FORGE - 15 assets - stock shell and buttons retained", fill=(232, 180, 90))
    for index, name in enumerate(("stageCold", "stageLit")):
        art = assets[name].resize((434, 240), Image.Resampling.LANCZOS)
        sheet.paste(art, (20 + index * 550, 60))
        draw.text((20 + index * 550, 38), name + " - in-game size", fill="white")
    positions = {
        "fireLight": (20, 335, 434, 240), "steam": (570, 335, 512, 256),
        "masterpieceBanner": (20, 640, 960, 220), "rowPlate": (20, 910, 536, 84),
        "rowSelected": (570, 910, 536, 84), "gaugeFill": (20, 1035, 464, 20),
        "medallion": (20, 1090, 120, 120), "rankCold": (180, 1130, 48, 48),
        "rankLit": (260, 1130, 48, 48), "rankGold": (340, 1130, 48, 48),
        "rankFlare": (430, 1100, 96, 96), "pieceHeat": (570, 1090, 116, 116),
        "goldenBurst": (850, 1040, 170, 170),
    }
    for name, (x, y, width, height) in positions.items():
        draw.text((x, y - 20), name, fill="white")
        art = assets[name].resize((width, height), Image.Resampling.LANCZOS)
        sheet.paste(art, (x, y), art.getchannel("A") if art.mode == "RGBA" else None)
    sheet.save(assetRoot / "preview/contactSheet.png")


def main():
    for folder in ("png", "preview"):
        (assetRoot / folder).mkdir(parents=True, exist_ok=True)
    assets = {}
    assets["stageCold"], assets["stageLit"] = stageFamily(source("stageCold"), source("stageLit"))
    assets["fireLight"] = fireLight(source("fireLight"))
    assets["medallion"] = fitSolid(source("medallion"), (120, 120))
    assets["rowPlate"], assets["rowSelected"] = stateFamily(
        [source("rowPlate"), source("rowSelected")], (536, 84))
    assets["rowSelected"] = heatedRow(assets["rowPlate"], assets["rowSelected"])
    assets["gaugeFill"] = gaugeFill(source("gaugeFill"))
    ranks = stateFamily([source(name) for name in ("rankCold", "rankLit", "rankGold")], (48, 48), 1)
    assets.update(zip(("rankCold", "rankLit", "rankGold"), ranks))
    assets["rankFlare"] = radialLight(source("rankFlare"), (96, 96))
    assets["pieceHeat"] = heatLight(source("pieceHeat"))
    assets["steam"] = steamSheet(source("steam"))
    assets["masterpieceBanner"] = banner(source("masterpieceBanner"))
    assets["goldenBurst"] = radialLight(source("goldenBurst"), (440, 440))
    for name, image in assets.items():
        image.save(assetRoot / "png" / f"{name}.png")
    contactSheet(assets)
    report = validateAssets(assetRoot, assetSpecs)
    (assetRoot / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Delivered {report['assetCount']} PNGs; validation errors: {report['errors']}")
    if report["errors"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
