"""Builds the sound bank of our own sound engine (the client extension DLL's EvolutionsAudio, awesome_wotlk):
clientPatcher/addons/EvolutionsAudio, which Build-FriendPatch.ps1 ships as Interface\\AddOns\\EvolutionsAudio.

    python buildAudio.py             the bank, from the sounds kept in the repository
    python buildAudio.py --vendor    first brings every sound a manifest takes from elsewhere into the repository

Every modules/*/client-assets/audio/*.json is read:
    { "sounds": { "<key>": { "kind": "ui" | "world" | "loop" | "music", "files": [ "<file>", ... ],
                              "volume": 1.0, "minDistance": 5, "maxDistance": 40, "loudness": <dBFS RMS>,
                              "loopStart": <music: the frame its loop goes back to> } },
      "emitters": [ { "zones": [ "<zone name, each locale's>", ... ], "sound": "<key>", "points": [ [x, y, z], ... ],
                      "interval": [min, max], "speed": 0, "time": "any" | "day" | "night", "volume": 1.0,
                      "place": "any" | "indoors" | "outdoors" } ] }
- a file: a WAV/OGG kept in the repository, under the manifest's sources/<manifest name>/ (client-assets/audio/
  sources/...): the bank is built from the repository alone, never from a client. To bring a sound in, name it in the
  manifest as "client:<archive path>" (the game client's own, from its archives - voices in its language: CLIENT in
  clientFiles.js), "asc:<archive path>" (the Ascension client's, ASCENSION_ROOT) or any path on disk, then run
  --vendor: the file is copied there (as it is: an OGG stays one) and the manifest rewritten to name the copy.
- emitters: the zone's ambience, the engine playing it while the player is there (its name as GetRealZoneText gives
  it, in every locale played). A loop sound plays from the first point while within reach; another sound every min
  to max seconds from one point at random; with a speed (yards a second) the emitter flies round its points, its
  sounds following it. time: by day (6:00-21:00 server time), by night, or always. place: only with the player
  indoors (an inn's room: its glasses are not for the street), outdoors, or anywhere. Written to ambience.txt.
  Emitters follow the game's ambience volume and switch, the other sounds its sound effects'.
- kind: ui is heard as an interface sound (no position, no room); world from where the server says (a point, or an
  object it follows), with the place's echo and muffled behind walls; loop the same, repeating until stopped or its
  object gone. music: not positioned, one at a time (EvolutionsAudio_PlayMusic, the server's PlayMusic), looping
  without end: at its end it goes back to loopStart (a frame of the file, an int or one per file; 0, the default: the
  whole file loops) - an intro before it is heard once. A music file holds the intro and exactly one loop period, cut
  sample-exact so its end flows into the loop start (no fade: the engine fades when it stops).
- minDistance / maxDistance (yards, world and loop): full volume within the first, fading to nothing at the second.
- world and loop sounds are written mono (one point in the world); ui and music sounds keep their channels.
- loudness: each file brought to that RMS level, its peaks held under -1 dBFS by a limiter. By default the kind's:
  LOUDNESS below, set against the game's own sounds (its interface cues at -12 to -25 dBFS RMS, LevelUp -12), so a
  new sound starts as loud as the rest - sounds from another game come mixed for its engine, often far quieter.
  A music is not limited: one gain for the whole file, towards the game's own music (its zone and raid tracks sit at
  -11 to -19 dBFS RMS, most near -16), never past a -1 dBFS peak - its dynamics, and its seam, untouched.
- volume: what the engine plays it at on top (1 by default); tune it live in game - edit sounds.txt in the client's
  copy, /eva reload - then carry the value back here.

A sound plays one of its files at random. Written: Sounds/<key>_<n>.wav (a music's .flac: minutes long, decoded as it
plays, sample-exact), sounds.txt (the engine's bank, one sound a line: key, kind, volume, min and max distance, files,
a music's loop starts) and the addon's .toc. Each file's level before and after is
printed; the folder is rebuilt whole.
"""
import glob
import json
import os
import shutil
import subprocess
import sys
import tempfile

import numpy
import soundfile
from scipy.ndimage import maximum_filter1d, minimum_filter1d, uniform_filter1d

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
CLIENT_FILES = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'clientFiles.js')
ASCENSION_FILES = os.path.join(REPO, 'localTools', 'ascensionImport', 'ascensionArchives.js')
OUTPUT = os.path.join(REPO, 'clientPatcher', 'addons', 'EvolutionsAudio')
KINDS = ('ui', 'world', 'loop', 'music')
LOUDNESS = {'ui': -12.0, 'world': -12.0, 'loop': -16.0, 'music': -16.0}
DISTANCES = {'ui': (0.0, 0.0), 'world': (12.0, 60.0), 'loop': (3.0, 25.0), 'music': (0.0, 0.0)}
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


def level_music(samples, level):
    """one gain for the whole file towards that RMS level, never past the ceiling: no limiter, so the dynamics are
    kept and the loop's end still flows into its start"""
    gain = 10 ** ((level - db(rms(samples))) / 20)
    return samples * min(gain, CEILING / max(float(numpy.max(numpy.abs(samples))), 1e-9))


def write_blocks(path, samples, rate, **options):
    """written a block at a time: libsndfile's encoders overflow the stack on a write of minutes"""
    with soundfile.SoundFile(path, 'w', rate, samples.shape[1], **options) as output:
        for start in range(0, len(samples), 65536):
            output.write(samples[start:start + 65536])


def extract(script, paths, directory):
    """the archive files at their archive path under directory; those found nowhere stop the build"""
    if not paths:
        return
    os.makedirs(directory, exist_ok=True)
    listing = os.path.join(directory, 'list.json')
    with open(listing, 'w', encoding='utf-8') as output:
        json.dump(sorted(paths), output)
    command = ['node', script, 'files', listing, directory] if script == ASCENSION_FILES else \
        ['node', script, listing, directory]
    subprocess.run(command, check=True)
    with open(os.path.join(directory, 'missing.json'), encoding='utf-8') as missing:
        lost = json.load(missing)
    if lost:
        raise SystemExit(f'Not in the archives: {lost}')


def resolve_sources(sounds, work):
    """every file of every sound as a path on disk: archive files extracted first, once"""
    wanted = {'client:': set(), 'asc:': set()}
    for spec in sounds.values():
        for source in spec['files']:
            for prefix in wanted:
                if source.startswith(prefix):
                    wanted[prefix].add(source[len(prefix):].replace('/', '\\'))
    extract(CLIENT_FILES, wanted['client:'], os.path.join(work, 'client'))
    extract(ASCENSION_FILES, wanted['asc:'], os.path.join(work, 'asc'))

    def path(source):
        for prefix, folder in (('client:', 'client'), ('asc:', 'asc')):
            if source.startswith(prefix):
                return os.path.join(work, folder, *source[len(prefix):].replace('/', '\\').split('\\'))
        return os.path.join(REPO, source)
    return path


def manifests():
    return sorted(glob.glob(os.path.join(REPO, 'modules', '*', 'client-assets', 'audio', '*.json')))


def kept(manifest, source):
    """whether a manifest's file is one of its sources kept in the repository"""
    folder = os.path.join(os.path.dirname(manifest), 'sources')
    path = os.path.normpath(os.path.join(REPO, source))
    return ':' not in source and path.startswith(folder + os.sep) and os.path.isfile(path)


def vendor():
    """every file a manifest takes from elsewhere copied to its sources/<manifest name>/, the manifest rewritten"""
    work = tempfile.mkdtemp()
    for manifest in manifests():
        with open(manifest, encoding='utf-8') as source:
            document = json.load(source)
        sounds = document.get('sounds', {})
        outside = {key: spec for key, spec in sounds.items() if any(not kept(manifest, f) for f in spec['files'])}
        if not outside:
            continue
        source_path = resolve_sources(outside, os.path.join(work, os.path.basename(manifest)))
        folder = os.path.join(os.path.dirname(manifest), 'sources', os.path.splitext(os.path.basename(manifest))[0])
        os.makedirs(folder, exist_ok=True)
        copied = 0
        for spec in outside.values():
            files = []
            for source in spec['files']:
                if kept(manifest, source):
                    files.append(source)
                    continue
                origin = source_path(source)
                name = os.path.basename(origin.replace('\\', os.sep))
                target = os.path.join(folder, name)
                if os.path.exists(target) and open(target, 'rb').read() != open(origin, 'rb').read():
                    # Two files of one name from two folders: the second keeps its folder's name in front
                    target = os.path.join(folder, f'{os.path.basename(os.path.dirname(origin))}_{name}')
                shutil.copyfile(origin, target)
                files.append(os.path.relpath(target, REPO).replace(os.sep, '/'))
                copied += 1
            spec['files'] = files
        with open(manifest, 'w', encoding='utf-8', newline='\n') as output:
            output.write(json.dumps(document, indent=2, ensure_ascii=False) + '\n')
        print(f'{os.path.relpath(manifest, REPO)}: {copied} files brought into {os.path.relpath(folder, REPO)}')
    shutil.rmtree(work, ignore_errors=True)


def main():
    sounds = {}
    emitters = []
    for path in manifests():
        with open(path, encoding='utf-8') as source:
            for key, spec in json.load(source).get('sounds', {}).items():
                if key in sounds:
                    raise SystemExit(f'{key} is in two manifests ({path})')
                if spec.get('kind', 'ui') not in KINDS:
                    raise SystemExit(f'{key}: kind must be one of {KINDS}')
                if any(c in key for c in '\t;\\/ '):
                    raise SystemExit(f'{key}: a key has no tab, ;, slash nor space')
                lost = [f for f in spec['files'] if not kept(path, f)]
                if lost:
                    raise SystemExit(f'{key}: {lost} not kept in the repository - run buildAudio.py --vendor')
                sounds[key] = spec
            source.seek(0)
            for emitter in json.load(source).get('emitters', []):
                emitter['manifest'] = path
                emitters.append(emitter)

    for emitter in emitters:
        if emitter['sound'] not in sounds:
            raise SystemExit(f'An emitter plays {emitter["sound"]}, which no manifest has ({emitter["manifest"]})')
    shutil.rmtree(OUTPUT, ignore_errors=True)
    os.makedirs(os.path.join(OUTPUT, 'Sounds'))
    lines = ['# key\tkind\tvolume\tmin distance\tmax distance\tfiles (Sounds\\)\t[music: loop starts, frames]'
             ' - written by localTools/audio/buildAudio.py']
    for key, spec in sorted(sounds.items()):
        kind = spec.get('kind', 'ui')
        level = float(spec.get('loudness', LOUDNESS[kind]))
        minimum, maximum = DISTANCES[kind]
        minimum = float(spec.get('minDistance', minimum))
        maximum = float(spec.get('maxDistance', maximum))
        names = []
        starts = spec.get('loopStart', 0)
        starts = starts if isinstance(starts, list) else [starts] * len(spec['files'])
        for index, source in enumerate(spec['files']):
            samples, rate = soundfile.read(os.path.join(REPO, source), dtype='float32', always_2d=True)
            before = db(rms(samples))
            # A sound placed in the world is one point: mono (a stereo file does not sit at its place)
            if kind in ('world', 'loop') and samples.shape[1] > 1:
                samples = samples.mean(axis=1, keepdims=True)
            if kind == 'music':
                if not 0 <= int(starts[index]) < len(samples):
                    raise SystemExit(f'{key}: loopStart {starts[index]} is not a frame of {source}')
                samples = level_music(samples, level)
                name = f'{key}_{index + 1}.flac'
                write_blocks(os.path.join(OUTPUT, 'Sounds', name), samples, rate, format='FLAC', subtype='PCM_16')
            else:
                samples = normalize(samples, rate, level)
                name = f'{key}_{index + 1}.wav'
                soundfile.write(os.path.join(OUTPUT, 'Sounds', name),
                                samples if samples.shape[1] > 1 else samples[:, 0], rate, subtype='PCM_16')
            names.append(name)
            print(f'  {name:42} {before:6.1f} -> {db(rms(samples)):6.1f} dBFS RMS')
        fields = [key, kind, f'{float(spec.get("volume", 1.0)):g}', f'{minimum:g}', f'{maximum:g}', ';'.join(names)]
        if kind == 'music':
            fields.append(';'.join(str(int(start)) for start in starts))
        lines.append('\t'.join(fields))
    with open(os.path.join(OUTPUT, 'sounds.txt'), 'w', encoding='utf-8', newline='\n') as manifest:
        manifest.write('\n'.join(lines) + '\n')
    ambience = ['# zones\tkey\tmin interval\tmax interval\tspeed\ttime\tvolume\tpoints\tplace'
                ' - written by localTools/audio/buildAudio.py']
    for emitter in emitters:
        interval = emitter.get('interval', [0, 0])
        points = ';'.join(','.join(f'{float(c):g}' for c in point) for point in emitter['points'])
        ambience.append('\t'.join(['|'.join(emitter['zones']), emitter['sound'], f'{float(interval[0]):g}',
                                    f'{float(interval[1]):g}', f'{float(emitter.get("speed", 0)):g}',
                                    emitter.get('time', 'any'), f'{float(emitter.get("volume", 1.0)):g}', points,
                                    emitter.get('place', 'any')]))
    with open(os.path.join(OUTPUT, 'ambience.txt'), 'w', encoding='utf-8', newline='\n') as file:
        file.write('\n'.join(ambience) + '\n')
    with open(os.path.join(OUTPUT, 'EvolutionsAudio.toc'), 'w', encoding='utf-8', newline='\n') as toc:
        toc.write(TOC)
    print(f'{len(sounds)} sounds, {len(emitters)} emitters written to {os.path.relpath(OUTPUT, REPO)}')


if __name__ == '__main__':
    if sys.argv[1:] == ['--vendor']:
        vendor()
    elif sys.argv[1:]:
        raise SystemExit(__doc__)
    main()
