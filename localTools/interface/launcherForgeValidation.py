"""Production checks for forge PNG canvases, registration, calm regions and tile cells."""

import numpy as np


assetSizes = {
    "backWallHot": (2560, 1520), "backWallCold": (2560, 1520),
    "anvilHot": (2560, 1520), "anvilCold": (2560, 1520), "foreground": (2560, 1520),
    "fireLight": (2560, 1520), "smoke": (2560, 760),
    "ingotHot": (1200, 400), "ingotCold": (1200, 400), "ingotGlow": (1600, 640),
    "sparkBurst": (2048, 2048), "embers": (1024, 1024), "hammer": (1200, 900),
    "emblem": (1024, 1024), "ironPlate": (1024, 512), "headerBand": (2560, 192),
}
additiveNames = ("fireLight", "smoke", "ingotGlow", "sparkBurst", "embers")


def validateAssets(assets, sparkCells, emberCells):
    assert set(assets) == set(assetSizes)
    for name, size in assetSizes.items():
        image = assets[name]
        assert image.size == size, name
        pixels = np.asarray(image)
        if image.mode == "RGBA":
            assert not pixels[pixels[:, :, 3] == 0, :3].any(), name
    for hotName, coldName in (("anvilHot", "anvilCold"), ("ingotHot", "ingotCold")):
        hot, cold = np.asarray(assets[hotName]), np.asarray(assets[coldName])
        assert np.array_equal(hot[:, :, 3], cold[:, :, 3]), coldName
    for name in ("backWallCold", "anvilCold", "ingotCold"):
        pixels = np.asarray(assets[name])[:, :, :3].astype(int)
        assert (pixels[:, :, 2] >= pixels[:, :, 1]).all(), name
        assert (pixels[:, :, 1] >= pixels[:, :, 0]).all(), name
    for name in ("emblem", "ironPlate", "ingotHot", "ingotCold", "ingotGlow"):
        pixels = np.asarray(assets[name])
        assert np.array_equal(pixels, pixels[:, ::-1]), name
    for name in ("smoke", "headerBand"):
        pixels = np.asarray(assets[name])
        assert np.array_equal(pixels[:, 0], pixels[:, -1]), name
    for name in additiveNames:
        pixels = np.asarray(assets[name])
        assert assets[name].mode == "RGB", name
        assert not pixels[0].any() and not pixels[-1].any(), name
        assert not pixels[:, 0].any() and not pixels[:, -1].any(), name
    for name in ("anvilHot", "anvilCold"):
        alpha = np.asarray(assets[name])[:, :, 3]
        assert not alpha[:, :1152].any() and not alpha[1124:].any(), name
    light = np.asarray(assets["fireLight"])
    assert not light[:, :1152].any() and not light[1124:].any()
    for name in ("backWallHot", "backWallCold"):
        pixels = np.asarray(assets[name]).astype(float)
        luminance = pixels.mean(axis=2)
        assert luminance[:, :1152].mean() < 28, name
        assert luminance[1124:].mean() < 28, name
    for cells, atlasName, cellSize in ((sparkCells, "sparkBurst", 512), (emberCells, "embers", 256)):
        assert len(cells) == 16
        atlas = np.asarray(assets[atlasName])
        for index, cell in enumerate(cells):
            pixels = np.asarray(cell)
            assert pixels.max() > 12, (atlasName, index)
            assert not pixels[0].any() and not pixels[-1].any(), (atlasName, index)
            assert not pixels[:, 0].any() and not pixels[:, -1].any(), (atlasName, index)
            x, y = index % 4 * cellSize, index // 4 * cellSize
            assert np.array_equal(atlas[y:y + cellSize, x:x + cellSize], pixels)
    brightness = [np.asarray(cell).astype(float).sum() for cell in sparkCells]
    assert max(brightness[:8]) > max(brightness[12:]) * 8
    for cell in sparkCells[:7]:
        core = np.asarray(cell).astype(float).mean(axis=2)[160:352, 160:352]
        coreY, coreX = np.nonzero(core > core.max() * 0.95)
        assert abs(coreX.mean() + 160 - 256) <= 1
        assert abs(coreY.mean() + 160 - 256) <= 1
    print("PASS: 16 exact dimensions; shared cold alpha/blue-grey palette; symmetry; calm zones")
    print("PASS: black additive edges; smoke/header seams; 16 isolated cells per particle sheet; burst fade")
