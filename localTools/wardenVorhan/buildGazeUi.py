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


def main():
    os.makedirs(OUTPUT, exist_ok=True)
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
