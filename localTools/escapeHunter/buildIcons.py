"""Le Traqueur d'évadés's icons: his spells' and debuffs' (localTools/escapeHunter/Spells.ps1), from the paintings in
localTools/escapeHunter/art to the 64x64 TGAs the client loads as Interface\\Icons
(modules/mod-stat-growth/client-assets/compiled; localTools/patchSinisterStrike.ps1 gives a spell its painted icon when
one is there, a stock one until then).

Each painting is square (256 x 256, the brief: .agents/plans/escape-hunter/escape-hunter.ASSETS.md); one not painted
yet is skipped. Run by the client build (clientPatcher/build/stages.json, traqueurIcons).

Usage: python localTools/escapeHunter/buildIcons.py
"""

import json
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(REPO / "localTools" / "interface"))
from buildLegendaryClientArt import write_icon_tga  # noqa: E402

ART = HERE / "art"
ICONS = REPO / "modules" / "mod-stat-growth" / "client-assets" / "compiled"
STAMP = HERE / "iconsBuilt.json"

SPELL_ICONS = [
    "ICON_Traqueur_Taillade", "ICON_Traqueur_Revers", "ICON_Traqueur_Laceration", "ICON_Traqueur_Moulinet",
    "ICON_Traqueur_Pistage", "ICON_Traqueur_Battue", "ICON_Traqueur_Collet", "ICON_Traqueur_Lanternes",
    "ICON_Traqueur_Hallali", "ICON_Traqueur_CureeMeute", "ICON_Traqueur_Proie", "ICON_Traqueur_PiegeProie",
    "ICON_Traqueur_Traque", "ICON_Traqueur_Charge", "ICON_Traqueur_Hurlement", "ICON_Traqueur_Cor",
    "ICON_Traqueur_Curee", "ICON_Traqueur_Debusque",
]


def painting(name):
    source = ART / f"{name}.png"
    if not source.exists():
        return None
    image = Image.open(source).convert("RGBA")
    if image.width != image.height:
        raise SystemExit(f"{source.name} is {image.width} x {image.height}: an icon is square")
    return image


def main():
    built = []
    for name in SPELL_ICONS:
        image = painting(name)
        if image is None:
            continue
        write_icon_tga(image, ICONS / f"{name}.tga")
        built.append(name)
    # What was built, the stage's output even before any icon is painted (a stock icon until then: never a placeholder)
    with open(STAMP, "w", encoding="utf-8", newline="\n") as stamp:
        stamp.write(json.dumps({"built": built}, indent=2) + "\n")
    print(f"Traqueur: {len(built)} of {len(SPELL_ICONS)} icons built")


if __name__ == "__main__":
    main()
