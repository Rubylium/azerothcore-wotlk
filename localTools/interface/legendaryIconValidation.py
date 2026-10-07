"""Validate the dungeon legendary icon delivery and optional compiled TGA files."""

import hashlib
from pathlib import Path

from PIL import Image


def validateIcons(assetRoot, names, compiledRoot=None):
    errors = []
    icons = []
    present = {path.stem for path in (assetRoot / "png").glob("*.png")}
    if present != set(names):
        errors.append(f"PNG names differ: missing={sorted(set(names) - present)}, extra={sorted(present - set(names))}")
    for name in names:
        path = assetRoot / "png" / f"{name}.png"
        if not path.exists():
            continue
        image = Image.open(path)
        if image.size != (256, 256) or image.mode != "RGB":
            errors.append(f"{name}: expected opaque 256x256 RGB, got {image.size} {image.mode}")
        record = {"name": name, "size": list(image.size), "mode": image.mode,
                  "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
        if compiledRoot is not None:
            compiledPath = Path(compiledRoot) / f"{name}.tga"
            if not compiledPath.exists():
                errors.append(f"{name}: compiled TGA missing")
            else:
                compiled = Image.open(compiledPath).convert("RGBA")
                if compiled.size != (64, 64) or compiled.getchannel("A").getextrema() != (255, 255):
                    errors.append(f"{name}: compiled TGA must be opaque 64x64")
                expected = image.convert("RGBA").resize((64, 64), Image.Resampling.LANCZOS)
                if compiled.tobytes() != expected.tobytes():
                    errors.append(f"{name}: compiled pixels differ from the PNG export")
                record["compiledSha256"] = hashlib.sha256(compiledPath.read_bytes()).hexdigest()
        icons.append(record)
    return {"iconCount": len(icons), "compiledVerified": compiledRoot is not None, "icons": icons, "errors": errors}
