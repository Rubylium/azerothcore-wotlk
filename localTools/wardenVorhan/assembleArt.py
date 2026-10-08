"""Export exact PNG canvases from retained imagegen paintings. Requires Pillow and NumPy."""
import json
from pathlib import Path

from PIL import Image

from imageAssembly import blackBackground, fitCanvas, quietPlaque, registerGaze, registerWalls, runeLayer

here = Path(__file__).resolve().parent
sourceDir = here / "source"
artDir = here / "art"


def loadSource(name):
    return Image.open(sourceDir / f"{name}.png").convert("RGB")


def exportArt():
    artDir.mkdir(exist_ok=True)
    jobs = json.loads((here / "assetManifest.json").read_text(encoding="utf-8"))["assets"]
    completed = []
    for job in jobs:
        name = job["name"]
        if not (sourceDir / f"{name}.png").exists():
            continue
        if name.startswith("gaze_") or name.startswith("wall_band_"):
            continue
        image = loadSource(name)
        if name == "cell_runes":
            image = runeLayer(image)
        elif name == "rollcall_mark":
            image = quietPlaque(image)
        else:
            image = fitCanvas(image, tuple(job["size"]))
            if job["kind"] == "effect":
                image = blackBackground(image)
        image.save(artDir / f"{name}.png")
        completed.append(name)
    gazeNames = [f"gaze_{state}" for state in ("closed", "half", "open")]
    wallNames = [f"wall_band_{index}" for index in range(1, 5)]
    for names, builder in ((gazeNames, registerGaze), (wallNames, registerWalls)):
        if not all((sourceDir / f"{name}.png").exists() for name in names):
            continue
        result = builder([loadSource(name) for name in names])
        frames = result[0] if names == wallNames else result
        for name, image in zip(names, frames):
            image.save(artDir / f"{name}.png")
            completed.append(name)
    print(f"Exported {len(completed)}/{len(jobs)} exact-size opaque PNGs.")


if __name__ == "__main__":
    exportArt()
