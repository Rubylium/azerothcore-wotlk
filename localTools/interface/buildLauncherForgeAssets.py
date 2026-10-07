"""The launcher's "La Forge" art, from the painted package (clientPatcher/assets/launcher/forge/png) to what the WPF
launcher embeds (clientPatcher/launcher/Assets/Forge). .agents/plans/launcher-forge/launcher-forge.ASSETS.md.

- The scene (2560 x 1520 paintings) at 3/4: 1920 x 1140, the launcher's Viewbox canvas. Opaque walls as JPEG; the
  anvil and the foreground cropped to what they hold, their place written to forge.json.
- WPF has no additive blending: a light-only layer painted on black (the fire's light, the smoke, the ingot's glow,
  the sparks, the embers) becomes a transparent PNG whose alpha is its brightness and whose colour is that light at
  full strength - laid over the scene, it lights it as the additive layer would.
- The spark burst cut into its 16 frames, the embers into their 16 sprites; the UI pieces reduced.

Usage: python localTools/interface/buildLauncherForgeAssets.py
"""

import json
from pathlib import Path

import numpy as np
from PIL import Image

REPO = Path(__file__).resolve().parents[2]
SOURCE = REPO / "clientPatcher" / "assets" / "launcher" / "forge" / "png"
OUT = REPO / "clientPatcher" / "launcher" / "Assets" / "Forge"
SCALE = 0.75


def load(name):
    return Image.open(SOURCE / f"{name}.png")


def scaled(image, factor=SCALE):
    return image.resize((round(image.width * factor), round(image.height * factor)), Image.Resampling.LANCZOS)


def light_to_alpha(image):
    """A light painted on black, as a transparent layer: alpha its brightness, colour that light at full strength"""
    rgb = np.asarray(image.convert("RGB")).astype(np.float32)
    alpha = rgb.max(axis=2)
    safe = np.maximum(alpha, 1.0)[:, :, None]
    colour = np.clip(rgb * 255.0 / safe, 0, 255)
    rgba = np.dstack([colour, alpha]).astype(np.uint8)
    rgba[alpha < 3] = 0
    return Image.fromarray(rgba, "RGBA")


def cropped(image):
    """The part of a transparent layer that holds something, and where it sits"""
    box = image.getbbox()
    return image.crop(box), box


def save_png(image, name):
    image.save(OUT / f"{name}.png", optimize=True)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    layout = {"scene": [round(2560 * SCALE), round(1520 * SCALE)]}

    for name in ("backWallHot", "backWallCold"):
        scaled(load(name).convert("RGB")).save(OUT / f"{name}.jpg", quality=88, optimize=True)

    for name in ("anvilHot", "anvilCold", "foreground"):
        image, box = cropped(scaled(load(name).convert("RGBA")))
        save_png(image, name)
        layout[name] = [box[0], box[1], image.width, image.height]

    image, box = cropped(light_to_alpha(scaled(load("fireLight"))))
    save_png(image, "fireLight")
    layout["fireLight"] = [box[0], box[1], image.width, image.height]

    smoke = light_to_alpha(scaled(load("smoke")))
    save_png(smoke, "smoke")
    layout["smoke"] = [smoke.width, smoke.height]

    # The ingot: both heats at half (1200 x 400 painted, drawn about 340 wide), its glow as light
    for name in ("ingotHot", "ingotCold"):
        save_png(scaled(load(name).convert("RGBA"), 0.5), name)
    save_png(light_to_alpha(scaled(load("ingotGlow"), 0.5)), "ingotGlow")

    # The spark burst's 16 frames (512 cells) at half, the embers' 16 sprites (256 cells) at half
    burst = light_to_alpha(load("sparkBurst"))
    for index in range(16):
        column, row = index % 4, index // 4
        frame = burst.crop((column * 512, row * 512, column * 512 + 512, row * 512 + 512))
        save_png(scaled(frame, 0.5), f"spark{index:02d}")
    embers = light_to_alpha(load("embers"))
    for index in range(16):
        column, row = index % 4, index // 4
        sprite = embers.crop((column * 256, row * 256, column * 256 + 256, row * 256 + 256))
        save_png(scaled(sprite, 0.25), f"ember{index:02d}")

    save_png(scaled(load("hammer").convert("RGBA"), 0.4), "hammer")
    save_png(scaled(load("emblem").convert("RGBA"), 0.25), "emblem")
    save_png(load("ironPlate").convert("RGBA"), "ironPlate")
    save_png(scaled(load("headerBand").convert("RGBA"), 0.5), "headerBand")

    (OUT / "forge.json").write_text(json.dumps(layout, indent=2) + "\n", encoding="utf-8", newline="\n")
    total = sum(path.stat().st_size for path in OUT.iterdir())
    print(json.dumps(layout))
    print(f"{len(list(OUT.iterdir()))} files, {total / 1048576:.1f} MB")


if __name__ == "__main__":
    main()
