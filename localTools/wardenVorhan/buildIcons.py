"""Gardien-chef Vorhan's icons: his spells' and debuffs' (localTools/wardenVorhan/Spells.ps1) and his sets' pieces
(mod-legendary Definitions 101-135), from the paintings in localTools/wardenVorhan/art to the 64x64 TGAs the client
loads as Interface\\Icons (modules/mod-stat-growth/client-assets/compiled; localTools/patchSinisterStrike.ps1 gives a
spell its painted icon when one is there, a stock one until then).

The inmate numbers' icons (ICON_Matricule1-8) are one painted blank tag (ICON_Matricule.png) with each number drawn on
it here: painted numerals are unreliable in image generation.

Each painting is square (256 x 256, the brief: .agents/plans/warden-vorhan/warden-vorhan.ASSETS.md); one not painted
yet is skipped.

Usage: python localTools/wardenVorhan/buildIcons.py
"""

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(REPO / "localTools" / "interface"))
from buildLegendaryClientArt import write_icon_tga  # noqa: E402

ART = HERE / "art"
ICONS = REPO / "modules" / "mod-stat-growth" / "client-assets" / "compiled"

SPELL_ICONS = [
    "ICON_MarqueGeolier", "ICON_Isolement", "ICON_SceauIsolement", "ICON_Menottes", "ICON_CouvreFeu", "ICON_Evasion",
    "ICON_CelluleSurpeuplee", "ICON_HorsCellule", "ICON_Regard", "ICON_Appel", "ICON_MauvaisMatricule",
    "ICON_Barriere", "ICON_ViolationCouvreFeu", "ICON_Sentence", "ICON_MiseIsolement", "ICON_Perpetuite",
    "ICON_PeineCapitale",
]
SET_ICONS = [f"INV_Vorhan_{armour}_{slot}" for armour in ("Plate", "Mail", "Leather", "Cloth")
             for slot in ("Head", "Shoulders", "Chest", "Hands", "Legs", "Wrists", "Waist", "Feet")] + \
            ["INV_Vorhan_Neck", "INV_Vorhan_Ring", "INV_Vorhan_Cloak"]
NUMBER_FONT = Path("C:/Windows/Fonts/georgiab.ttf")


def painting(name):
    source = ART / f"{name}.png"
    if not source.exists():
        return None
    image = Image.open(source).convert("RGBA")
    if image.width != image.height:
        raise SystemExit(f"{source.name} is {image.width} x {image.height}: an icon is square")
    return image


# A number struck on the tag: ember-red with a dark edge and a glow, as big as the tag's face allows, read at 32 px
def with_number(tag, number):
    image = tag.copy()
    size = image.width
    font = ImageFont.truetype(str(NUMBER_FONT), int(size * 0.5))
    text = str(number)
    layer = Image.new("RGBA", image.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer)
    box = draw.textbbox((0, 0), text, font=font)
    position = ((size - (box[2] - box[0])) / 2 - box[0], (size - (box[3] - box[1])) / 2 - box[1])
    glow = Image.new("RGBA", image.size, (0, 0, 0, 0))
    ImageDraw.Draw(glow).text(position, text, font=font, fill=(255, 80, 20, 255))
    image.alpha_composite(glow.filter(ImageFilter.GaussianBlur(size * 0.03)))
    draw.text(position, text, font=font, fill=(255, 196, 120, 255), stroke_width=max(2, size // 64),
              stroke_fill=(30, 8, 0, 255))
    image.alpha_composite(layer)
    return image


def main():
    painted = 0
    for name in SPELL_ICONS + SET_ICONS:
        image = painting(name)
        if image is None:
            continue
        write_icon_tga(image, ICONS / f"{name}.tga")
        painted += 1
    tag = painting("ICON_Matricule")
    if tag is not None:
        for number in range(1, 9):
            write_icon_tga(with_number(tag, number), ICONS / f"ICON_Matricule{number}.tga")
        painted += 8
    print(f"Vorhan: {painted} of {len(SPELL_ICONS) + len(SET_ICONS) + 8} icons built")


if __name__ == "__main__":
    main()
