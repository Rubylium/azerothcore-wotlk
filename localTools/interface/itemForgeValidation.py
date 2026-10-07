"""Validate native delivery dimensions, modes, state alpha and additive flipbook isolation."""

import hashlib

import numpy as np
from PIL import Image


def validateAssets(root, specs):
    errors = []
    records = []
    images = {}
    for name, (size, mode) in specs.items():
        path = root / "png" / f"{name}.png"
        image = Image.open(path)
        images[name] = image
        if image.size != size or image.mode != mode:
            errors.append(f"{name}: expected {size} {mode}, got {image.size} {image.mode}")
        pixels = np.asarray(image)
        if mode == "RGBA" and np.any(pixels[pixels[:, :, 3] == 0, :3]):
            errors.append(f"{name}: nonzero colour in fully transparent pixels")
        records.append({"name": name, "size": list(image.size), "mode": image.mode,
                        "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    for family in (("rowPlate", "rowSelected"), ("rankCold", "rankLit", "rankGold")):
        alpha = np.asarray(images[family[0]])[:, :, 3]
        for name in family[1:]:
            if not np.array_equal(alpha, np.asarray(images[name])[:, :, 3]):
                errors.append(f"{name}: master alpha changed")
    for name in ("fireLight", "rankFlare", "pieceHeat", "goldenBurst"):
        pixels = np.asarray(images[name])
        if any(np.any(edge) for edge in (pixels[0], pixels[-1], pixels[:, 0], pixels[:, -1])):
            errors.append(f"{name}: additive canvas edge is not pure black")
    steam = np.asarray(images["steam"])
    energies = []
    for index in range(8):
        x, y = index % 4 * 256, index // 4 * 256
        cell = steam[y:y + 256, x:x + 256]
        if any(np.any(edge) for edge in (cell[0], cell[-1], cell[:, 0], cell[:, -1])):
            errors.append(f"steam frame {index + 1}: nonblack cell edge")
        energies.append(round(float(cell.mean()), 3))
    if energies[-1] >= max(energies) * 0.1:
        errors.append("steam: final frame must be nearly gone")
    # The banner's own dark face, as painted (the code writes its text there)
    textPixels = np.asarray(images["masterpieceBanner"].crop((205, 58, 905, 162)))
    if np.percentile(textPixels[:, :, :3].max(axis=2), 95) > 70:
        errors.append("banner: high-contrast artwork in its text face")
    if textPixels[:, :, 3].min() < 250:
        errors.append("banner: transparent pixels in its text face")
    reuse = {"hammer": (1200, 900), "sparkBurst": (2048, 2048), "embers": (1024, 1024)}
    for name, size in reuse.items():
        if Image.open(root / "reference/launcherReuse" / f"{name}.png").size != size:
            errors.append(f"reused {name}: dimensions changed")
    return {"assetCount": len(records), "assets": records, "steamFrameEnergy": energies,
            "sharedStateAlpha": True, "errors": errors}
