"""Export 21 generated dungeon legendary icons and visual checks at 256, 64 and 32 pixels."""

import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

from buildLegendaryClientArt import ICON_ONLY_NAMES, ICONS
from legendaryIconValidation import validateIcons


repoRoot = Path(__file__).resolve().parents[2]
assetRoot = repoRoot / "clientPatcher/assets/legendaryIcons"
dungeonNames = ["The Mechanar", "Utgarde Keep", "The Shattered Halls", "The Deadmines",
                "Drak'Tharon Keep", "The Forge of Souls", "Halls of Lightning"]


def font(size):
    try:
        return ImageFont.truetype("segoeui.ttf", size)
    except OSError:
        return ImageFont.load_default()


def previews(images):
    sheet = Image.new("RGB", (1024, 2380), (18, 16, 22))
    small = Image.new("RGB", (1024, 860), (18, 16, 22))
    draw = ImageDraw.Draw(sheet)
    smallDraw = ImageDraw.Draw(small)
    draw.text((24, 12), "Dungeon legendaries - 256px PNGs", fill=(232, 180, 90), font=font(22))
    smallDraw.text((24, 12), "Readability - native 64px and 32px", fill=(232, 180, 90), font=font(22))
    for index, (name, image) in enumerate(images.items()):
        row, col = index // 3, index % 3
        x, y = 24 + col * 336, 88 + row * 326
        smallY = 84 + row * 110
        if col == 0:
            draw.text((24, y - 36), dungeonNames[row], fill="white", font=font(18))
            smallDraw.text((24, smallY - 28), dungeonNames[row], fill="white", font=font(16))
        sheet.paste(image, (x, y))
        label = name.removeprefix("INV_Legendary_")
        draw.text((x, y + 266), label, fill="white", font=font(15))
        small.paste(image.resize((64, 64), Image.Resampling.LANCZOS), (x, smallY))
        small.paste(image.resize((32, 32), Image.Resampling.LANCZOS), (x + 78, smallY + 16))
        smallDraw.text((x + 122, smallY + 20), label, fill="white", font=font(14))
    sheet.save(assetRoot / "contactSheet.png")
    small.save(assetRoot / "readabilityPreview.png")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verify-compiled", action="store_true")
    args = parser.parse_args()
    (assetRoot / "png").mkdir(parents=True, exist_ok=True)
    images = {}
    for name in ICON_ONLY_NAMES:
        with Image.open(assetRoot / "source" / f"{name}.png") as source:
            if source.width != source.height:
                raise SystemExit(f"{name}: generated master is not square")
            image = source.convert("RGB").resize((256, 256), Image.Resampling.LANCZOS)
        image.save(assetRoot / "png" / f"{name}.png")
        images[name] = image
    previews(images)
    report = validateIcons(assetRoot, ICON_ONLY_NAMES, ICONS if args.verify_compiled else None)
    (assetRoot / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Exported {report['iconCount']} icons; compiled verified: {report['compiledVerified']}")
    if report["errors"]:
        raise SystemExit("\n".join(report["errors"]))


if __name__ == "__main__":
    main()
