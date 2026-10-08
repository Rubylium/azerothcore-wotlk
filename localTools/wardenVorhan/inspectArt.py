"""Validate deliverable canvases and build review sheets at native UI scales."""
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageOps

here = Path(__file__).resolve().parent
artDir = here / "art"
previewDir = here / "preview"


def boardPreview():
    if not all((artDir / f"{name}.png").exists() for name in ("backdrop", "portrait")):
        return
    backdrop = Image.open(artDir / "backdrop.png").transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    scene = ImageOps.fit(backdrop, (836, 565), method=Image.Resampling.LANCZOS).convert("RGBA")
    portrait = Image.open(artDir / "portrait.png").crop((0, 0, 1024, 1303))
    portrait = portrait.resize((444, 565), Image.Resampling.LANCZOS).convert("RGBA")
    pixels = np.array(portrait)
    y, x = np.mgrid[:565, :444]
    horizontal = np.minimum(np.clip(x / (444 * 0.05), 0, 1), np.clip((444 - x) / (444 * 0.22), 0, 1))
    vertical = np.minimum(np.clip(y / (565 * 0.04), 0, 1), np.clip((565 - y) / (565 * 0.30), 0, 1))
    for values in (horizontal, vertical):
        values[:] = values * values * (3 - 2 * values)
    pixels[..., 3] = (255 * horizontal * vertical).round().astype("uint8")
    scene.alpha_composite(Image.fromarray(pixels), (0, 0))
    scene.convert("RGB").save(previewDir / "boardComposite.png")


def contactSheet(jobs, filename, columns=6):
    cellWidth, cellHeight = 200, 230
    sheet = Image.new("RGB", (columns * cellWidth, math.ceil(len(jobs) / columns) * cellHeight), (18, 19, 22))
    draw = ImageDraw.Draw(sheet)
    for index, job in enumerate(jobs):
        x, y = index % columns * cellWidth, index // columns * cellHeight
        path = artDir / f'{job["name"]}.png'
        if not path.exists():
            continue
        image = Image.open(path)
        thumb = ImageOps.contain(image, (176, 176), Image.Resampling.LANCZOS)
        sheet.paste(thumb, (x + (cellWidth - thumb.width) // 2, y + 6))
        if job["kind"] in ("spell", "item"):
            sheet.paste(image.resize((32, 32), Image.Resampling.LANCZOS), (x + 154, y + 172))
        label = job["name"].replace("INV_Vorhan_", "").replace("ICON_", "")
        draw.text((x + 8, y + 209), label, fill=(218, 220, 222))
    sheet.save(previewDir / filename)


def inspectArt(requireComplete=True):
    previewDir.mkdir(exist_ok=True)
    jobs = json.loads((here / "assetManifest.json").read_text(encoding="utf-8"))["assets"]
    missing = []
    for job in jobs:
        path = artDir / f'{job["name"]}.png'
        if not path.exists():
            missing.append(job["name"])
            continue
        image = Image.open(path)
        assert image.size == tuple(job["size"]), (path.name, image.size)
        assert image.mode == "RGB" and image.format == "PNG", path.name
        if job["kind"] == "wall":
            pixels = np.array(image)
            assert np.array_equal(pixels[:, 0], pixels[:, -1]), path.name
    for kind in ("spell", "item", "effect", "wall", "board"):
        contactSheet([job for job in jobs if job["kind"] == kind], f"{kind}Sheet.png")
    boardPreview()
    wallPaths = [artDir / f"wall_band_{index}.png" for index in range(1, 5)]
    if all(path.exists() for path in wallPaths):
        frames = []
        for path in wallPaths:
            image = Image.open(path)
            tiled = Image.new("RGB", (image.width * 2, image.height))
            tiled.paste(image, (0, 0))
            tiled.paste(image, (image.width, 0))
            frames.append(tiled.resize((1024, 128), Image.Resampling.LANCZOS))
        frames[0].save(previewDir / "wallAnimation.gif", save_all=True, append_images=frames[1:], duration=160, loop=0)
    report = {"expected": len(jobs), "exported": len(jobs) - len(missing), "missing": missing,
              "checks": ["PNG", "exact dimensions", "RGB opaque", "identical left/right wall seam"]}
    (previewDir / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))
    if requireComplete:
        assert not missing, "Artwork set incomplete"


if __name__ == "__main__":
    import sys
    inspectArt(requireComplete="--partial" not in sys.argv)
