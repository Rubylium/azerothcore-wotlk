"""Le Refus' screen (clientPatcher/interface/Interface/FrameXML/RefusOverlay.lua): the pictures laid over the game while
Valérian winds time back, as the user validated them on the preview page (2026-10-13: version A, the clock at 72%).

    python localTools/valerian/buildRefusOverlay.py

Writes clientPatcher/interface/Interface/Valerian/Refus*.png and .blp (the client's BLP2, its sides powers of two):
- RefusClock: the rewind's clock (art/sigil_refus.png, at its full 1024: drawn as tall as the screen, 512 was blown up),
  its whole disc opaque to a soft rim - keyed from its black background its dark stone came out half see-through.
- RefusHands: its hands (art/sigil_refus_hands.png), keyed, turned by the client about their middle.
- RefusVignette: the screen's edges darkening to violet (the preview's radial gradient, baked: the client fades
  plain textures only - a gradient's alpha set from Lua turned white).
- RefusFlash: the white-gold burst as the music comes back (added to the screen: its colour premultiplied on black).
"""
import importlib.util
import os

import numpy
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
ART = os.path.join(HERE, 'art')
OUT = os.path.join(REPO, 'clientPatcher', 'interface', 'Interface', 'Valerian')


def load_blp_writer():
    spec = importlib.util.spec_from_file_location('buildParagonArt', os.path.join(REPO, 'localTools', 'interface',
                                                                                'buildParagonArt.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.writeRawBlp


def radius(width, height):
    """Each pixel's distance from the middle: 1 at the farthest corner (an ellipse over the screen)"""
    y, x = numpy.mgrid[0:height, 0:width].astype(numpy.float64)
    u = (x + 0.5) / width * 2.0 - 1.0
    v = (y + 0.5) / height * 2.0 - 1.0
    return numpy.hypot(u, v) / numpy.sqrt(2.0), u, v


def stops(r, points):
    """A gradient over r: [(at, (r, g, b, a)), ...], linear between"""
    ats = [at for at, _ in points]
    channels = [numpy.interp(r, ats, [colour[c] for _, colour in points]) for c in range(4)]
    return numpy.dstack(channels)


def clock(size=1024):
    source = Image.open(os.path.join(ART, 'sigil_refus.png')).convert('RGB').resize((size, size), Image.LANCZOS)
    rgb = numpy.asarray(source, dtype=numpy.float64) / 255.0
    r, _, _ = radius(size, size)
    r = r * numpy.sqrt(2.0)      # 1 at the disc's rim
    alpha = numpy.clip((0.995 - r) / 0.02, 0.0, 1.0)
    return Image.fromarray((numpy.dstack([rgb, alpha]) * 255).astype(numpy.uint8), 'RGBA')


def hands(size=1024):
    source = Image.open(os.path.join(ART, 'sigil_refus_hands.png')).convert('RGB').resize((size, size), Image.LANCZOS)
    rgb = numpy.asarray(source, dtype=numpy.float64) / 255.0
    alpha = numpy.clip(rgb.max(axis=2) * 3.0, 0.0, 1.0)
    return Image.fromarray((numpy.dstack([rgb, alpha]) * 255).astype(numpy.uint8), 'RGBA')


def vignette(width=512, height=256):
    r, _, _ = radius(width, height)
    rgba = stops(r, [(0.0, (0, 0, 0, 0)), (0.38, (0, 0, 0, 0)), (0.70, (24 / 255, 10 / 255, 48 / 255, 0.55)),
                     (1.0, (10 / 255, 4 / 255, 22 / 255, 0.95))])
    # Its colour where it is clear: the dark violet's, so that its fading in never shows black edges
    rgba[..., :3] = numpy.where(rgba[..., 3:4] > 0.0, rgba[..., :3], numpy.array([24, 10, 48]) / 255.0)
    return Image.fromarray((rgba * 255).astype(numpy.uint8), 'RGBA')


def flash(size=256):
    r, _, _ = radius(size, size)
    rgba = stops(r, [(0.0, (1.0, 243 / 255, 196 / 255, 1.0)), (0.30, (232 / 255, 196 / 255, 106 / 255, 0.7)),
                     (0.70, (232 / 255, 196 / 255, 106 / 255, 0.0)), (1.0, (0, 0, 0, 0))])
    premultiplied = rgba[..., :3] * rgba[..., 3:4]
    return Image.fromarray((numpy.dstack([premultiplied, numpy.ones_like(r)]) * 255).astype(numpy.uint8), 'RGBA')


def main():
    write = load_blp_writer()
    os.makedirs(OUT, exist_ok=True)
    for name, image in (('RefusClock', clock()), ('RefusHands', hands()), ('RefusVignette', vignette()),
                        ('RefusFlash', flash())):
        image.save(os.path.join(OUT, name + '.png'))
        write(image, os.path.join(OUT, name + '.blp'))
        print(name, image.size)


if __name__ == '__main__':
    main()
