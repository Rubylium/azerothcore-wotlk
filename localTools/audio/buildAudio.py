"""Builds the sound bank of our own sound engine (the client extension DLL's EvolutionsAudio, awesome_wotlk):
clientPatcher/addons/EvolutionsAudio, which Build-FriendPatch.ps1 ships as Interface\\AddOns\\EvolutionsAudio.

    python buildAudio.py

Every modules/*/client-assets/audio/*.json is read:
    { "sounds": { "<key>": { "kind": "ui" | "world" | "loop", "files": [ "<wav, relative to the repository>", ... ],
                              "volume": 1.0, "minDistance": 5, "maxDistance": 40, "loudness": <dBFS RMS> } } }
- kind: ui is heard as an interface sound (no position, no room); world from where the server says (a point, or an
  object it follows), with the place's echo and muffled behind walls; loop the same, repeating until stopped or its
  object gone.
- minDistance / maxDistance (yards, world and loop): full volume within the first, fading to nothing at the second.
- loudness: each file brought to that RMS level, its peaks held under -1 dBFS by a limiter. By default the kind's:
  LOUDNESS below, set against the game's own sounds (its interface cues at -12 to -25 dBFS RMS, LevelUp -12), so a
  new sound starts as loud as the rest - sounds from another game come mixed for its engine, often far quieter.
- volume: what the engine plays it at on top (1 by default); tune it live in game - edit sounds.txt in the client's
  copy, /eva reload - then carry the value back here.

A sound plays one of its files at random. Written: Sounds/<key>_<n>.wav, sounds.txt (the engine's bank, one sound a
line: key, kind, volume, min and max distance, files) and the addon's .toc. Each file's level before and after is
printed; the folder is rebuilt whole.
"""
import glob
import json
import os
import shutil
import sys

import numpy
import soundfile
from scipy.ndimage import maximum_filter1d, minimum_filter1d, uniform_filter1d

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
OUTPUT = os.path.join(REPO, 'clientPatcher', 'addons', 'EvolutionsAudio')
KINDS = ('ui', 'world', 'loop')
LOUDNESS = {'ui': -12.0, 'world': -12.0, 'loop': -16.0}
DISTANCES = {'ui': (0.0, 0.0), 'world': (12.0, 60.0), 'loop': (3.0, 25.0)}
CEILING = 10 ** (-1.0 / 20)
LOOKAHEAD = 0.003
RELEASE = 0.060
TOC = """## Interface: 30300
## Title: Evolutions Audio
## Notes: The sounds of the Evolutions client's own sound engine (AwesomeWotlkLib), read by it: nothing to load.
## Author: Evolutions
"""


def db(value):
    return 20 * numpy.log10(max(value, 1e-9))


def rms(samples):
    return float(numpy.sqrt(numpy.mean(samples ** 2)))


def normalize(samples, rate, level):
    """raised as a whole to that RMS level, the peaks held under the ceiling by a look-ahead limiter (the gain each
    sample needs, taken over the next 3 ms and released over 60 ms) - a few passes, the limiter taking some back"""
    for _ in range(3):
        samples = samples * 10 ** ((level - db(rms(samples))) / 20)
        envelope = maximum_filter1d(numpy.max(numpy.abs(samples), axis=1), size=2 * max(1, int(rate * LOOKAHEAD)) + 1)
        gain = numpy.minimum(1.0, CEILING / numpy.maximum(envelope, 1e-9))
        release = max(1, int(rate * RELEASE))
        gain = uniform_filter1d(minimum_filter1d(gain, size=release), size=release)
        samples = samples * gain[:, None]
    return numpy.clip(samples, -CEILING, CEILING)


def main():
    sounds = {}
    for path in sorted(glob.glob(os.path.join(REPO, 'modules', '*', 'client-assets', 'audio', '*.json'))):
        with open(path, encoding='utf-8') as source:
            for key, spec in json.load(source)['sounds'].items():
                if key in sounds:
                    raise SystemExit(f'{key} is in two manifests ({path})')
                if spec.get('kind', 'ui') not in KINDS:
                    raise SystemExit(f'{key}: kind must be one of {KINDS}')
                if any(c in key for c in '\t;\\/ '):
                    raise SystemExit(f'{key}: a key has no tab, ;, slash nor space')
                sounds[key] = spec

    shutil.rmtree(OUTPUT, ignore_errors=True)
    os.makedirs(os.path.join(OUTPUT, 'Sounds'))
    lines = ['# key\tkind\tvolume\tmin distance\tmax distance\tfiles (Sounds\\)'
             ' - written by localTools/audio/buildAudio.py']
    for key, spec in sorted(sounds.items()):
        kind = spec.get('kind', 'ui')
        level = float(spec.get('loudness', LOUDNESS[kind]))
        minimum, maximum = DISTANCES[kind]
        minimum = float(spec.get('minDistance', minimum))
        maximum = float(spec.get('maxDistance', maximum))
        names = []
        for index, source in enumerate(spec['files']):
            samples, rate = soundfile.read(os.path.join(REPO, source), dtype='float32', always_2d=True)
            before = db(rms(samples))
            samples = normalize(samples, rate, level)
            name = f'{key}_{index + 1}.wav'
            soundfile.write(os.path.join(OUTPUT, 'Sounds', name), samples if samples.shape[1] > 1 else samples[:, 0],
                            rate, subtype='PCM_16')
            names.append(name)
            print(f'  {name:42} {before:6.1f} -> {db(rms(samples)):6.1f} dBFS RMS')
        lines.append('\t'.join([key, kind, f'{float(spec.get("volume", 1.0)):g}', f'{minimum:g}', f'{maximum:g}',
                                ';'.join(names)]))
    with open(os.path.join(OUTPUT, 'sounds.txt'), 'w', encoding='utf-8', newline='\n') as manifest:
        manifest.write('\n'.join(lines) + '\n')
    with open(os.path.join(OUTPUT, 'EvolutionsAudio.toc'), 'w', encoding='utf-8', newline='\n') as toc:
        toc.write(TOC)
    print(f'{len(sounds)} sounds written to {os.path.relpath(OUTPUT, REPO)}')


if __name__ == '__main__':
    if len(sys.argv) != 1:
        raise SystemExit(__doc__)
    main()
