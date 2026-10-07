"""Export La Forge's scene/UI assets and preview its ready, updating and closed states."""

import json
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFont

from launcherForgeAssembly import assembleAssets
from launcherForgeValidation import additiveNames, validateAssets


repoRoot = Path(__file__).resolve().parents[2]
assetRoot = repoRoot / "clientPatcher/assets/launcher/forge"


def addLight(canvas, light):
    return ImageChops.add(canvas.convert("RGB"), light.convert("RGB")).convert("RGBA")


def scenePreview(assets, cold):
    name = "Cold" if cold else "Hot"
    scene = assets[f"backWall{name}"].convert("RGBA")
    scene.alpha_composite(assets[f"anvil{name}"])
    if not cold:
        scene = addLight(scene, assets["fireLight"])
    scene.alpha_composite(assets["foreground"])
    return scene.resize((1280, 760), Image.Resampling.LANCZOS).convert("RGB")


def launcherPreview(assets, state):
    canvas = scenePreview(assets, state == "closed").convert("RGBA")
    canvas.alpha_composite(assets["headerBand"].resize((1280, 48), Image.Resampling.LANCZOS), (0, 0))
    logo = assets["emblem"].resize((84, 84), Image.Resampling.LANCZOS)
    canvas.alpha_composite(logo, (62, 100))
    plate = assets["ironPlate"].resize((1220, 178), Image.Resampling.LANCZOS)
    canvas.alpha_composite(plate, (30, 565))
    ingot = assets["ingotCold"].copy()
    if state == "ready":
        ingot = assets["ingotHot"].copy()
        glow = assets["ingotGlow"].resize((480, 192), Image.Resampling.LANCZOS)
        light = Image.new("RGB", (1280, 760), 0)
        light.paste(glow, (12, 558))
        canvas = addLight(canvas, light)
    elif state == "updating":
        # Crop geometry and texture together; the painted rim and silhouette stay registered.
        full = assets["ingotHot"].crop((0, 0, 780, 400))
        ingot.paste(full, (0, 0))
    canvas.alpha_composite(ingot.resize((360, 120), Image.Resampling.LANCZOS), (70, 594))
    draw = ImageDraw.Draw(canvas)
    titleFont = ImageFont.truetype("C:/Windows/Fonts/arialbd.ttf", 58)
    font = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 22)
    smallFont = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 17)
    buttonFont = ImageFont.truetype("C:/Windows/Fonts/arialbd.ttf", 24)
    draw.text((26, 12), "EVOLUTIONS", font=smallFont, fill="#a39d94")
    draw.text((1180, 10), "-    x", font=smallFont, fill="#a39d94")
    draw.text((163, 111), "EVOLUTIONS", font=titleFont, fill="#e9e2d7")
    draw.text((168, 178), "LA FORGE", font=smallFont, fill="#cda16b")
    labels = {"ready": ("Le royaume est ouvert", "JOUER"),
              "updating": ("Mise à jour en cours", "65 %"),
              "closed": ("Le royaume est fermé", "FERMÉ")}
    heading, button = labels[state]
    draw.text((70, 281), heading, font=font, fill="#ffb347" if state != "closed" else "#a7acb4")
    draw.text((70, 324), "Votre aventure continue en Azeroth.", font=font, fill="#b4aea5")
    draw.text((70, 371), "Actualités  ·  Communauté  ·  Paramètres", font=smallFont, fill="#8d867e")
    draw.text((251, 653), button, font=buttonFont, anchor="mm",
              fill="#23160d" if state == "ready" else "#f1e7d7")
    draw.text((510, 610), heading, font=font, fill="#d2cbc1")
    description = "Client à jour" if state == "ready" else ("Téléchargement des fichiers" if state == "updating"
                                                            else "En attente de l'ouverture du royaume")
    draw.text((510, 651), description, font=smallFont, fill="#8d867e")
    return canvas.convert("RGB")


def contactSheet(assets):
    canvas = Image.new("RGB", (1600, 1280), "#191715")
    draw = ImageDraw.Draw(canvas)
    font = ImageFont.truetype("C:/Windows/Fonts/arial.ttf", 19)
    for index, (name, painting) in enumerate(assets.items()):
        x, y = index % 4 * 400, index // 4 * 320
        draw.text((x + 12, y + 9), name, font=font, fill="#cda16b")
        fitted = painting.copy()
        fitted.thumbnail((376, 270), Image.Resampling.LANCZOS)
        position = (x + (400 - fitted.width) // 2, y + 40 + (270 - fitted.height) // 2)
        canvas.paste(fitted, position, fitted if fitted.mode == "RGBA" else None)
    return canvas


def writeJson(path, data):
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8", newline="\n")


def main():
    sources = {path.stem: Image.open(path).convert("RGBA") for path in (assetRoot / "source").glob("*.png")}
    assets, transforms, sparks, embers = assembleAssets(sources)
    validateAssets(assets, sparks, embers)
    pngRoot = assetRoot / "png"
    pngRoot.mkdir(exist_ok=True)
    for name, painting in assets.items():
        painting.save(pngRoot / f"{name}.png")
    frameRoot = assetRoot / "sparkFrames"
    frameRoot.mkdir(exist_ok=True)
    for index, frame in enumerate(sparks):
        frame.save(frameRoot / f"spark{index:02}.png")
    frames = [frame.resize((256, 256), Image.Resampling.LANCZOS) for frame in sparks]
    frames.append(Image.new("RGB", (256, 256), 0))
    frames[0].save(assetRoot / "sparkPreview.gif", save_all=True, append_images=frames[1:],
                   duration=[50] * 16 + [650], loop=0, disposal=2)
    contactSheet(assets).save(assetRoot / "contactSheet.png")
    comparison = Image.new("RGB", (1280, 2280), "#131416")
    for index, state in enumerate(("ready", "updating", "closed")):
        preview = launcherPreview(assets, state)
        preview.save(assetRoot / f"preview{state.title()}.png")
        comparison.paste(preview, (0, index * 760))
    comparison.save(assetRoot / "statePreview.png")
    for cold in (False, True):
        scenePreview(assets, cold).save(assetRoot / ("sceneCold.png" if cold else "sceneHot.png"))
    cells = lambda size: [[index % 4 * size, index // 4 * size, size, size] for index in range(16)]
    writeJson(assetRoot / "assetManifest.json", {
        "generator": "built-in imagegen", "launcherSize": [1280, 760], "sceneSize": [2560, 1520],
        "alpha": "straight", "additiveAssets": list(additiveNames), "transforms": transforms,
        "sceneLayerOrder": ["backWall", "anvil", "fireLight", "smoke", "foreground"],
        "calmZones": {"leftFraction": 0.45, "bottomFraction": 0.26},
        "ingotGlowOffset": [-200, -120], "sparkCellOrigin": [256, 256], "sparkFrameDurationMs": 50,
        "sparkCells": cells(512), "emberCells": cells(256),
        "assets": {name: {"size": list(image.size), "mode": image.mode} for name, image in assets.items()},
    })
    print("Exported 16 exact-size PNGs, 16 spark frames, GIF and three 1280x760 state previews")


if __name__ == "__main__":
    main()
