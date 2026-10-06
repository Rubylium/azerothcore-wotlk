"""Export Whitemane and Mograine PNG packages, with previews at 130% item-frame scale."""

import json
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFont

from buildMarqueInquisiteurArt import makeContactSheet
from monasteryLegendaryAssembly import assembleFamily, validateFamily


repoRoot = Path(__file__).resolve().parents[2]
assetRoot = repoRoot / "clientPatcher/assets"
familyTitles = {
    "sermentWhitemane": "Serment de Whitemane",
    "consecrationMograine": "Consécration de Mograine",
}


def itemPreview(assets, iconSize, glowEnabled):
    frameSize = round(iconSize * 1.3)
    canvas = Image.new("RGBA", (frameSize, frameSize), 0)
    icon = assets["itemIcon"].resize((iconSize, iconSize), Image.Resampling.LANCZOS).convert("RGBA")
    offset = (frameSize - iconSize) // 2
    canvas.alpha_composite(icon, (offset, offset))
    if glowEnabled:
        glow = assets["itemFrameGlow"].resize((frameSize, frameSize), Image.Resampling.LANCZOS)
        canvas = ImageChops.add(canvas.convert("RGB"), glow).convert("RGBA")
    canvas.alpha_composite(assets["itemFrame"].resize((frameSize, frameSize), Image.Resampling.LANCZOS))
    return canvas


def tooltipPreview(assets, title):
    panel = Image.new("RGBA", (480, 380), (10, 8, 7, 248))
    scale = 0.25
    corner = assets["tooltipCorner"].resize((64, 64), Image.Resampling.LANCZOS)
    horizontal = assets["tooltipBarHorizontal"].resize((64, 12), Image.Resampling.LANCZOS)
    vertical = assets["tooltipBarVertical"].resize((12, 64), Image.Resampling.LANCZOS)
    for x in range(64, 416, 64):
        piece = horizontal.crop((0, 0, min(64, 416 - x), 12))
        panel.alpha_composite(piece, (x, 6))
        panel.alpha_composite(piece, (x, 362))
    for y in range(64, 316, 64):
        piece = vertical.crop((0, 0, 12, min(64, 316 - y)))
        panel.alpha_composite(piece, (6, y))
        panel.alpha_composite(piece, (462, y))
    panel.alpha_composite(corner, (0, 0))
    panel.alpha_composite(corner.transpose(Image.Transpose.FLIP_LEFT_RIGHT), (416, 0))
    panel.alpha_composite(corner.transpose(Image.Transpose.FLIP_TOP_BOTTOM), (0, 316))
    panel.alpha_composite(corner.transpose(Image.Transpose.ROTATE_180), (416, 316))
    font = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 22)
    smallFont = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 17)
    draw = ImageDraw.Draw(panel)
    draw.text((38, 58), title, font=font, fill="#ff8000")
    draw.text((38, 102), "Cathédrale écarlate", font=smallFont, fill="#ffffff")
    draw.text((38, 160), "Objet légendaire", font=smallFont, fill="#dfc894")
    draw.text((38, 226), "Aperçu du cadre et des ornements", font=smallFont, fill="#ffffff")
    # Crest canvases overhang the panel; their bar centres line up with its y=12 and y=368.
    canvas = Image.new("RGBA", (480, 404), 0)
    canvas.alpha_composite(panel, (0, 12))
    for name, y in (("tooltipTopCrest", 0), ("tooltipBottomCrest", 356)):
        crest = assets[name].resize((round(384 * scale), round(192 * scale)), Image.Resampling.LANCZOS)
        canvas.alpha_composite(crest, (192, y))
    return canvas


def familyPreview(assets, title):
    canvas = Image.new("RGBA", (1000, 720), (25, 22, 19, 255))
    draw = ImageDraw.Draw(canvas)
    font = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 22)
    smallFont = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 16)
    draw.text((25, 20), title, font=font, fill="#dfc894")
    for iconSize, position, glowing in ((200, (25, 85), True), (100, (310, 85), False),
                                      (50, (310, 260), False), (50, (395, 260), True)):
        item = itemPreview(assets, iconSize, glowing)
        canvas.alpha_composite(item, position)
        caption = f"Icon {iconSize}px / frame {item.width}px" if iconSize != 50 else ("Glow" if glowing else "Idle")
        draw.text((position[0], position[1] + item.height + 8), caption, font=smallFont, fill="#c8bda9")
    canvas.alpha_composite(assets["powerIcon"].convert("RGBA"), (25, 420))
    draw.text((25, 685), "Power icon", font=smallFont, fill="#c8bda9")
    canvas.alpha_composite(tooltipPreview(assets, title), (490, 130))
    return canvas.convert("RGB")


def writeJson(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")


def main():
    familyImages = []
    for family, title in familyTitles.items():
        folder = assetRoot / family
        sources = {path.stem: Image.open(path).convert("RGBA") for path in (folder / "source").glob("*.png")}
        assets = assembleFamily(sources, family)
        validateFamily(assets)
        pngRoot = folder / "png"
        pngRoot.mkdir(exist_ok=True)
        for name, image in assets.items():
            image.save(pngRoot / f"{name}.png")
        preview = familyPreview(assets, title)
        preview.save(folder / "preview.png")
        familyImages.append(preview)
        makeContactSheet(assets).crop((0, 0, 1100, 810)).save(folder / "contactSheet.png")
        writeJson(folder / "assetManifest.json", {
            "generator": "built-in imagegen", "alpha": "straight", "blend": {"itemFrameGlow": "ADD"},
            "itemFrameScale": 1.3, "sourceInset": 24,
            "tooltipBarThickness": 28 if family == "sermentWhitemane" else 36,
            "cornerBarCenter": [48, 48], "crestBarCenterY": 96,
            "assets": {name: {"size": list(image.size), "mode": image.mode} for name, image in assets.items()},
        })
        print(f"PASS {family}: 9 exact sizes, symmetry, transparent opening, black glow edges and all tile joins")
    comparison = Image.new("RGB", (1000, 1440), "#191613")
    for index, image in enumerate(familyImages):
        comparison.paste(image, (0, index * 720))
    comparison.save(assetRoot / "monasteryLegendaries/preview.png")
    print("Exported 18 PNG assets, two contact sheets, native-scale previews and metadata")


if __name__ == "__main__":
    main()
