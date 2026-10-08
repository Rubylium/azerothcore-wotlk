r"""The warden's eye for the interface (clientPatcher FrameXML WardenGaze.lua): the gaze's 64 frames
(indicatorArt.gaze_atlas, from art/eye_open.png and art/eye_glow.png) as Interface\WardenVorhan\WardenGaze.blp, and
their timeline written into WardenGaze.lua. The eye is drawn in the interface, over the world and its nameplates (a
model in the world was hidden behind the warden's nameplate). Run it again whenever the eye's paintings change.

Usage: python localTools/wardenVorhan/buildGazeUi.py
"""
import importlib.util
import os
import re

import indicatorArt

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
OUTPUT = os.path.join(REPO, 'clientPatcher', 'interface', 'Interface', 'WardenVorhan')
SCRIPT = os.path.join(REPO, 'clientPatcher', 'interface', 'Interface', 'FrameXML', 'WardenGaze.lua')


def blp_writer():
    path = os.path.join(REPO, 'localTools', 'interface', 'buildParagonArt.py')
    spec = importlib.util.spec_from_file_location('buildParagonArt', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.writeRawBlp


def sprites():
    """The eye's particles (WardenGaze.lua), drawn white-hot to ember on black-clear, RGBA: an ember (64), a wisp of
    smoke (128) and a soft flare (256)"""
    import numpy
    from PIL import Image

    def radial(size):
        xs = (numpy.arange(size) + 0.5) / size * 2.0 - 1.0
        x, y = numpy.meshgrid(xs, xs)
        return numpy.hypot(x, y)

    def image(colour, alpha):
        rgba = numpy.concatenate([colour, alpha[..., None]], axis=2)
        return Image.fromarray(numpy.clip(rgba * 255.0 + 0.5, 0, 255).astype(numpy.uint8), 'RGBA')

    radius = radial(64)
    core = numpy.clip(1.0 - radius / 0.45, 0.0, 1.0) ** 1.2
    halo = numpy.clip(1.0 - radius, 0.0, 1.0) ** 1.6
    colour = numpy.array([1.0, 0.35, 0.08]) * halo[..., None] + numpy.array([1.0, 0.85, 0.5]) * core[..., None]
    ember = image(numpy.clip(colour, 0.0, 1.0), numpy.clip(halo + core, 0.0, 1.0))

    radius = radial(128)
    noise = numpy.asarray(Image.fromarray((numpy.random.default_rng(5).random((8, 8)) * 255).astype(numpy.uint8))
                          .resize((128, 128), Image.BICUBIC), dtype=numpy.float64) / 255.0
    body = numpy.clip(1.0 - radius, 0.0, 1.0) ** 1.6 * (0.5 + 0.5 * noise)
    smoke = image(numpy.ones((128, 128, 3)) * numpy.array([0.35, 0.04, 0.03]), body * 0.8)

    radius = radial(256)
    glow = numpy.clip(1.0 - radius, 0.0, 1.0) ** 2.0
    flare = image(numpy.array([1.0, 0.3, 0.06]) * numpy.ones((256, 256, 3)), glow)
    return {'WardenGazeEmber': ember, 'WardenGazeSmoke': smoke, 'WardenGazeFlare': flare}


def main():
    os.makedirs(OUTPUT, exist_ok=True)
    for name, picture in sprites().items():
        picture.save(os.path.join(OUTPUT, name + '.png'))
        blp_writer()(picture, os.path.join(OUTPUT, name + '.blp'))
    atlas = indicatorArt.gaze_atlas()
    atlas.save(os.path.join(OUTPUT, 'WardenGaze.png'))
    blp_writer()(atlas, os.path.join(OUTPUT, 'WardenGaze.blp'))
    keys = indicatorArt.gaze_timeline()
    lines = ['local TIMELINE = {']
    for start in range(0, len(keys), 8):
        lines.append('    ' + ' '.join(f'{{ {time}, {frame} }},' for time, frame in keys[start:start + 8]))
    lines.append('}')
    with open(SCRIPT, encoding='utf-8') as source:
        script = source.read()
    script, count = re.subn(r'local TIMELINE = \{\n.*?\n\}', '\n'.join(lines), script, flags=re.S)
    if count != 1:
        raise SystemExit(f'{SCRIPT}: its TIMELINE table was not found')
    with open(SCRIPT, 'w', encoding='utf-8', newline='\n') as output:
        output.write(script)
    print(f'WardenGaze.blp {atlas.width} x {atlas.height}, {len(keys)} keys')


if __name__ == '__main__':
    main()
