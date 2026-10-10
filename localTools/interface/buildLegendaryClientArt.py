"""The legendaries' client art: their icons, item frames and tooltip frames, from the painted PNGs to what the 3.3.5
client loads (modules/mod-legendary; .agents/plans/legendary-items/).

Per legendary (clientPatcher/assets/<folder>/png, made by its own build<Name>Art.py):
- the item icon and the power icon, 64x64 TGA into modules/mod-stat-growth/client-assets/compiled (shipped as
  Interface\\Icons, and found there by localTools/patchSinisterStrike.ps1 for the power's spell);
- Interface\\ItemFrames\\Legendary-<Name>-Frame.blp and -Glow.blp: the item frame and its glow, 128x128;
- Interface\\ItemFrames\\Legendary-<Name>-Tooltip.blp: the tooltip frame's pieces at half their painted size, one
  512x256 atlas (FrameXML LegendaryFrames.lua, TOOLTIP_PIECES):
      corner        x 0-128,   y 0-128
      top crest     x 128-320, y 0-96
      bottom crest  x 320-512, y 0-96
      title plate   x 128-384, y 96-144
      bar, across   x 128-256, y 144-168
      bar, down     x 0-24,    y 128-256
Shipped uncompressed (the Paragon writer): DXT would band the gold.

Usage: python localTools/interface/buildLegendaryClientArt.py
"""

import os
import struct
from pathlib import Path

from PIL import Image

from buildParagonArt import writeRawBlp

REPO = Path(__file__).resolve().parents[2]
ICONS = REPO / "modules" / "mod-stat-growth" / "client-assets" / "compiled"
FRAMES = REPO / "clientPatcher" / "interface" / "Interface" / "ItemFrames"

LEGENDARIES = [
    {"name": "MarqueInquisiteur", "folder": "marqueInquisiteur",
     "itemIcon": "INV_Legendary_MarqueInquisiteur", "powerIcon": "Legendary_MarqueInquisiteur"},
    {"name": "SermentWhitemane", "folder": "sermentWhitemane",
     "itemIcon": "INV_Legendary_SermentWhitemane", "powerIcon": "Legendary_SermentWhitemane"},
    {"name": "ConsecrationMograine", "folder": "consecrationMograine",
     "itemIcon": "INV_Legendary_ConsecrationMograine", "powerIcon": "Legendary_ConsecrationMograine"},
]

TOOLTIP_PIECES = {
    "tooltipCorner": (0, 0),
    "tooltipTopCrest": (128, 0),
    "tooltipBottomCrest": (320, 0),
    "tooltipTitlePlate": (128, 96),
    "tooltipBarHorizontal": (128, 144),
    "tooltipBarVertical": (0, 128),
}


def write_icon_tga(image, path, size=64):
    """Uncompressed 32-bit BGRA, top-down: the shape of the other compiled icons (buildFrontierArt.py)."""
    image = image.convert("RGBA").resize((size, size), Image.Resampling.LANCZOS)
    header = bytearray(18)
    header[2] = 2
    struct.pack_into("<HH", header, 12, size, size)
    header[16] = 32
    header[17] = 0x28
    pixels = bytearray()
    for r, g, b, a in image.getdata():
        pixels += bytes((b, g, r, a))
    path.write_bytes(bytes(header) + bytes(pixels))


def half(image):
    return image.resize((image.width // 2, image.height // 2), Image.Resampling.LANCZOS)


def build(legendary):
    png = REPO / "clientPatcher" / "assets" / legendary["folder"] / "png"

    def load(name):
        return Image.open(png / f"{name}.png").convert("RGBA")

    write_icon_tga(load("itemIcon"), ICONS / f"{legendary['itemIcon']}.tga")
    write_icon_tga(load("powerIcon"), ICONS / f"{legendary['powerIcon']}.tga")

    frame = load("itemFrame").resize((128, 128), Image.Resampling.LANCZOS)
    writeRawBlp(frame, str(FRAMES / f"Legendary-{legendary['name']}-Frame.blp"))
    glow = Image.open(png / "itemFrameGlow.png").convert("RGB").resize((128, 128), Image.Resampling.LANCZOS)
    glow.putalpha(255)
    writeRawBlp(glow, str(FRAMES / f"Legendary-{legendary['name']}-Glow.blp"))

    atlas = Image.new("RGBA", (512, 256), 0)
    for name, position in TOOLTIP_PIECES.items():
        # The title plate was dropped after the first legendary (stretched behind the title, out of place)
        if (png / f"{name}.png").exists():
            atlas.paste(half(load(name)), position)
    atlas.save(png.parent / "tooltipAtlas.png")
    writeRawBlp(atlas, str(FRAMES / f"Legendary-{legendary['name']}-Tooltip.blp"))
    print(f"{legendary['name']}: icons, item frame and glow, tooltip atlas")


# The other dungeons' legendaries (mod-legendary Definitions 4-24): one painted icon each and nothing else - no frame,
# no tooltip frame (those stay for a few chosen items). Its item and its power's spell share it. Painted at 256 x 256
# into clientPatcher/assets/legendaryIcons/png/<icon>.png (.agents/plans/legendary-items/dungeon-legendaries.ASSETS.md);
# one not painted yet is skipped, and the game shows its look's own icon meanwhile (localTools/patchSinisterStrike.ps1).
ICON_ONLY = REPO / "clientPatcher" / "assets" / "legendaryIcons" / "png"
ICON_ONLY_NAMES = [
    "INV_Legendary_EpaulieresCapacitus", "INV_Legendary_AbaquePathaleon", "INV_Legendary_BrassardsSepethrea",
    "INV_Legendary_CeintureIngvar", "INV_Legendary_CuirasseKeleseth", "INV_Legendary_CollierAnnhylde",
    "INV_Legendary_PoignesKargath", "INV_Legendary_BandelettesNethekurse", "INV_Legendary_ChevalierePorung",
    "INV_Legendary_PlastronVanCleef", "INV_Legendary_PoudreGilnid", "INV_Legendary_MouflesCookie",
    "INV_Legendary_BottesDred", "INV_Legendary_RobeNovos", "INV_Legendary_PendentifTharonja",
    "INV_Legendary_JambieresDevoreur", "INV_Legendary_HeaumeBronjahm", "INV_Legendary_AnneauAmeRefletee",
    "INV_Legendary_EtincelleIonar", "INV_Legendary_PoingsLoken", "INV_Legendary_ChevaliereBjarngrim",
    "INV_Unique_EchoDuNeant", "INV_Legendary_EtoileCaptive", "INV_Unique_SablierPerpetuite",
    "INV_Legendary_TropheeTraqueur",
]


def build_icons():
    painted = 0
    for name in ICON_ONLY_NAMES:
        source = ICON_ONLY / f"{name}.png"
        if not source.exists():
            continue
        image = Image.open(source)
        if image.width != image.height:
            raise SystemExit(f"{source.name} is {image.width} x {image.height}: an icon is square")
        write_icon_tga(image, ICONS / f"{name}.tga")
        painted += 1
    print(f"Dungeon legendaries: {painted} of {len(ICON_ONLY_NAMES)} icons painted")


def main():
    FRAMES.mkdir(parents=True, exist_ok=True)
    for legendary in LEGENDARIES:
        build(legendary)
    build_icons()


if __name__ == "__main__":
    main()
