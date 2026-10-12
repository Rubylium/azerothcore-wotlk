"""Valérian's phase 3 music (Music.ValerianRewind, modules/mod-stat-growth/client-assets/audio/valerian.json): phase
2's own track wound back as a tape is, then played again from just before its drop - he refuses to die and takes the
fight back to his Écho.

    python localTools/valerian/buildRewindMusic.py [--preview out.mp3]

- The rewind (REWIND_BEATS of phase 2's tempo): the track from Lumière totale's hit (REWIND_FROM) run backwards,
  faster and faster (REWIND_SPEED, the pitch rising with it as a tape's), each slice low-passed under its own Nyquist so
  it does not alias, cut at its end.
- Then phase 2 from the beat RESUME_AT (just before 1:06, the Écho's build to the 1:11 drop) to its end. Phase 3's
  clock: its beats are phase 2's, shifted - track time t3 = REWIND_SECONDS + (t2 - RESUME_AT).

Written next to phase 2's source (valerian_p3.ogg); the client build ships it like the others.
"""
import argparse
import os

import numpy
import soundfile
from scipy.signal import butter, sosfilt

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
SOURCES = os.path.join(REPO, 'modules', 'mod-stat-growth', 'client-assets', 'audio', 'sources', 'valerian')
P2 = os.path.join(SOURCES, 'valerian_p2.ogg')
P3 = os.path.join(SOURCES, 'valerian_p3.ogg')

# Phase 2's grid (valerian.json: 92.3 BPM from 1.39 s)
BEAT = 60.0 / 92.3
FIRST_BEAT = 1.39
REWIND_BEATS = 5
REWIND_SECONDS = REWIND_BEATS * BEAT
REWIND_FROM = 275.0                 # Lumière totale's hit (4:35.0): wound back from there
REWIND_SPEED = (2.0, 14.0)          # times the track's speed, from the start of the rewind to its end
RESUME_AT = FIRST_BEAT + 99 * BEAT  # the beat just before 1:06
SLICES = 48
FADE_OUT = 0.12                     # the rewind cut, seconds
FADE_IN = 0.02


def rewind(track, rate):
    count = int(round(REWIND_SECONDS * rate))
    t = numpy.arange(count) / rate
    low, high = REWIND_SPEED
    # Speed rising exponentially; position: REWIND_FROM minus the distance wound
    growth = numpy.log(high / low) / REWIND_SECONDS
    wound = low * (numpy.exp(growth * t) - 1.0) / growth
    positions = (REWIND_FROM - wound) * rate
    speed = low * numpy.exp(growth * t)
    out = numpy.zeros((count, track.shape[1]), dtype=numpy.float64)
    bounds = numpy.linspace(0, count, SLICES + 1).astype(int)
    for start, end in zip(bounds[:-1], bounds[1:]):
        # The source this slice reads, low-passed under its own speed's Nyquist (with a margin), then sampled back
        middle = speed[(start + end) // 2]
        cutoff = min(0.45 * rate / middle, 0.45 * rate)
        first = max(int(positions[end - 1]) - 64, 0)
        last = min(int(positions[start]) + 64, len(track) - 1)
        piece = track[first:last + 1]
        sos = butter(6, cutoff, btype='low', fs=rate, output='sos')
        piece = sosfilt(sos, piece[::-1], axis=0)[::-1]
        where = positions[start:end] - first
        index = numpy.clip(where.astype(int), 0, len(piece) - 2)
        frac = (where - index)[:, None]
        out[start:end] = piece[index] * (1.0 - frac) + piece[index + 1] * frac
    fade = numpy.ones(count)
    tail = int(FADE_OUT * rate)
    fade[-tail:] = numpy.linspace(1.0, 0.0, tail)
    return out * fade[:, None]


def write_blocks(path, samples, rate, **options):
    """Written in blocks: libsndfile's Vorbis encoder crashed on a whole track at once (as buildAudio.py found)"""
    with soundfile.SoundFile(path, 'w', rate, samples.shape[1], **options) as output:
        for start in range(0, len(samples), 65536):
            output.write(samples[start:start + 65536])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--preview', help='also write the first seconds as an mp3 there (an audition)')
    args = parser.parse_args()
    track, rate = soundfile.read(P2, dtype='float64', always_2d=True)
    resumed = track[int(round(RESUME_AT * rate)):].copy()
    fade = int(FADE_IN * rate)
    resumed[:fade] *= numpy.linspace(0.0, 1.0, fade)[:, None]
    music = numpy.concatenate([rewind(track, rate), resumed])
    peak = numpy.abs(music).max()
    if peak > 0.99:
        music *= 0.99 / peak
    write_blocks(P3, music.astype(numpy.float32), rate, format='OGG', subtype='VORBIS')
    print(f'{os.path.relpath(P3, REPO)}: {len(music) / rate:.2f} s (rewind {REWIND_SECONDS:.2f} s, then phase 2 from '
          f'{RESUME_AT:.2f} s)')
    if args.preview:
        # The end of phase 2 into the rewind and the music back, as heard in the fight
        before = track[int(round((REWIND_FROM - 12.0) * rate)):int(round(REWIND_FROM * rate))]
        audition = numpy.concatenate([before, music[:int(40.0 * rate)]])
        write_blocks(args.preview, audition.astype(numpy.float32), rate, format='MP3')
        print(f'{args.preview}: {len(audition) / rate:.1f} s')


if __name__ == '__main__':
    main()
