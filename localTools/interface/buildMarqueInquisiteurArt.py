"""Export the ten painted Inquisitor assets and a composed item/tooltip preview."""

import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFont

from marqueInquisiteurAssembly import assembleAssets


repoRoot = Path(__file__).resolve().parents[2]
assetRoot = repoRoot / "clientPatcher/assets/marqueInquisiteur"


def makePreview(assets):
    preview = Image.new("RGBA", (1000, 720), (25, 22, 19, 255))
    draw = ImageDraw.Draw(preview)
    font = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 22)
    smallFont = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 16)
    draw.text((28, 20), "Marque de l'Inquisiteur - asset preview", font=font, fill="#dfc894")
    for index, size in enumerate((256, 100, 50)):
        x, y = (30, 85) if index == 0 else (320, 90 + (index - 1) * 150)
        icon = assets["itemIcon"].resize((size, size), Image.Resampling.LANCZOS).convert("RGBA")
        if index == 0:
            glow = assets["itemFrameGlow"].resize((size, size), Image.Resampling.LANCZOS)
            icon = ImageChops.add(icon.convert("RGB"), glow).convert("RGBA")
        preview.alpha_composite(icon, (x, y))
        frame = assets["itemFrame"].resize((size, size), Image.Resampling.LANCZOS)
        preview.alpha_composite(frame, (x, y))
        draw.text((x, y + size + 8), f"{size} px", font=smallFont, fill="#c8bda9")
    preview.alpha_composite(assets["powerIcon"].convert("RGBA"), (30, 405))
    draw.text((30, 670), "Brand / power icon", font=smallFont, fill="#c8bda9")
    # Every border piece uses the same 1/4 scale, preserving the shared bar cross-section.
    tooltip = Image.new("RGBA", (480, 500), (10, 8, 7, 248))
    corner = assets["tooltipCorner"].resize((64, 64), Image.Resampling.LANCZOS)
    horizontal = assets["tooltipBarHorizontal"].resize((64, 12), Image.Resampling.LANCZOS)
    vertical = assets["tooltipBarVertical"].resize((12, 64), Image.Resampling.LANCZOS)
    for x in range(64, 416, 64):
        length = min(64, 416 - x)
        piece = horizontal.crop((0, 0, length, 12))
        tooltip.alpha_composite(piece, (x, 6))
        tooltip.alpha_composite(piece, (x, 482))
    for y in range(64, 436, 64):
        piece = vertical.crop((0, 0, 12, min(64, 436 - y)))
        tooltip.alpha_composite(piece, (6, y))
        tooltip.alpha_composite(piece, (462, y))
    tooltip.alpha_composite(corner, (0, 0))
    tooltip.alpha_composite(corner.transpose(Image.Transpose.FLIP_LEFT_RIGHT), (416, 0))
    tooltip.alpha_composite(corner.transpose(Image.Transpose.FLIP_TOP_BOTTOM), (0, 436))
    tooltip.alpha_composite(corner.transpose(Image.Transpose.ROTATE_180), (416, 436))
    tooltip.alpha_composite(assets["tooltipTitlePlate"].resize((420, 79)), (30, 45))
    text = ImageDraw.Draw(tooltip)
    for y, line, colour in (
        (65, "Marque de l'Inquisiteur", "#ff8000"),
        (112, "Dos", "#ffffff"), (152, "Cathédrale écarlate", "#c8bda9"),
        (230, "Vos coups apposent une marque sacrée.", "#ffffff"),
        (266, "Elle brûle la cible d'un feu purificateur.", "#ffffff"),
    ):
        text.text((42, y), line, font=font if y == 65 else smallFont, fill=colour)
    preview.alpha_composite(tooltip, (490, 130))
    preview.alpha_composite(assets["tooltipTopCrest"].resize((96, 48)), (682, 118))
    preview.alpha_composite(assets["tooltipBottomCrest"].resize((96, 48)), (682, 594))
    return preview.convert("RGB")


def makeContactSheet(assets):
    sheet = Image.new("RGB", (1100, 1080), "#201c18")
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 18)
    for index, (name, image) in enumerate(assets.items()):
        x, y = (index % 3) * 366, (index // 3) * 270
        draw.text((x + 12, y + 10), name, font=font, fill="#dfc894")
        fitted = image.copy()
        fitted.thumbnail((342, 220), Image.Resampling.LANCZOS)
        position = (x + (366 - fitted.width) // 2, y + 38 + (220 - fitted.height) // 2)
        if fitted.mode == "RGBA":
            sheet.paste(fitted, position, fitted)
        else:
            sheet.paste(fitted, position)
    return sheet


def validateAssets(assets):
    sizes = [(512, 512), (512, 512), (256, 256), (384, 192), (384, 192),
             (256, 48), (48, 256), (512, 96), (256, 256), (256, 256)]
    for (name, image), size in zip(assets.items(), sizes):
        assert image.size == size, name
    for name in ("itemFrame", "itemFrameGlow", "tooltipTopCrest", "tooltipBottomCrest",
                 "tooltipBarHorizontal", "tooltipTitlePlate"):
        pixels = np.asarray(assets[name])
        assert np.array_equal(pixels, pixels[:, ::-1]), name
    horizontal = np.asarray(assets["tooltipBarHorizontal"])
    vertical = np.asarray(assets["tooltipBarVertical"])
    assert np.array_equal(horizontal[:, 0], horizontal[:, -1])
    assert np.array_equal(vertical[0], vertical[-1])
    corner = np.asarray(assets["tooltipCorner"])
    assert np.array_equal(corner[24:72, -1], horizontal[:, -1])
    assert np.array_equal(corner[-1, 24:72], vertical[-1])
    frameAlpha = np.asarray(assets["itemFrame"])[:, :, 3]
    assert not frameAlpha[180:350, 180:332].any()
    glow = np.asarray(assets["itemFrameGlow"])
    assert not glow[0].any() and not glow[-1].any() and not glow[:, 0].any() and not glow[:, -1].any()
    for name in ("tooltipTopCrest", "tooltipBottomCrest"):
        pixels = np.asarray(assets[name])
        assert np.array_equal(pixels[72:120, 0], horizontal[:, 0]), name
        assert np.array_equal(pixels[72:120, -1], horizontal[:, -1]), name
    print("PASS: 10 exact sizes, symmetry, transparent opening, black glow edges and matching tile/corner/crest joins")


def main():
    sources = {path.stem: Image.open(path).convert("RGBA") for path in (assetRoot / "source").glob("*.png")}
    assets = assembleAssets(sources)
    validateAssets(assets)
    pngRoot = assetRoot / "png"
    pngRoot.mkdir(exist_ok=True)
    for name, image in assets.items():
        image.save(pngRoot / f"{name}.png")
    makePreview(assets).save(assetRoot / "preview.png")
    makeContactSheet(assets).save(assetRoot / "contactSheet.png")
    manifest = {
        "generator": "built-in imagegen", "alpha": "straight", "blend": {"itemFrameGlow": "ADD"},
        "tooltipBarThickness": 40, "cornerBarCenter": [48, 48], "crestBarCenterY": 96,
        "assets": {name: {"size": list(image.size), "mode": image.mode} for name, image in assets.items()},
    }
    (assetRoot / "assetManifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print("Exported 10 PNGs, metadata and composed preview")


if __name__ == "__main__":
    main()
