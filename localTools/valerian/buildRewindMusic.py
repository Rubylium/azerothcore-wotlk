"""Valérian's phase 3 music (Music.ValerianRewind, modules/mod-stat-growth/client-assets/audio/valerian.json): he refuses
to die and takes the fight back to his Écho - the music cut, the rewind (the user's own sound, a big clock's clockwork
and bells) as time stands still and runs back, then phase 2's own track again from just before its drop. The rewind is
the track's own start, as loud as the music (a sound effect of its own was far too quiet: the user, 2026-10-13).

    python localTools/valerian/buildRewindMusic.py [--preview out.mp3]

- The track: the rewind from its start, levelled to the music's loudness on its drop (REWIND_LEVEL_FROM), then phase 2
  from the beat RESUME_AT (just before 1:06, the Écho's build to the 1:11 drop) to its end, coming in REWIND_TO_MUSIC
  after the start, faded in over FADE_IN. Phase 3's clock: t3 = t - REWIND_TO_MUSIC (ZealotBoss.cpp RefusMusicMs).
- The rewind sound: the user's (Valerian_Rewind.wav beside the music, from their rewind.mp3). A wound back tape (the
  track run backwards, faster and faster) came first, then a lone church bell: loud and unpleasant, then replaced.
- --preview: an audition of the change as heard in the fight - phase 2's last seconds, the cut, the rewind, phase 3's
  music fading in REWIND_TO_MUSIC after the cut, under the rewind's last seconds (it was
  first after them: a gap the user heard).
"""
import argparse
import os

import numpy
import soundfile
from scipy.signal import resample_poly

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
SOURCES = os.path.join(REPO, 'modules', 'mod-stat-growth', 'client-assets', 'audio', 'sources', 'valerian')
P2 = os.path.join(SOURCES, 'valerian_p2.ogg')
P3 = os.path.join(SOURCES, 'valerian_p3.ogg')
REWIND = os.path.join(SOURCES, 'Valerian_Rewind.wav')

# Phase 2's grid (valerian.json: 92.3 BPM from 1.39 s)
BEAT = 60.0 / 92.3
FIRST_BEAT = 1.39
RESUME_AT = FIRST_BEAT + 99 * BEAT  # the beat just before 1:06
FADE_IN = 1.5                       # blended under the rewind's last clockwork, full as it falls away (8 s)
CUT_AT = 275.0                      # where the audition cuts phase 2 (Lumière totale's hit)
REWIND_TO_MUSIC = 6.5               # from the cut to phase 3's music: under the rewind's end, not after it
REWIND_LEVEL_FROM = (5.25, 15.25)   # the music's drop (phase 3's clock): the rewind's loudest 8 s made as loud


def write_blocks(path, samples, rate, **options):
    """Written in blocks: libsndfile's Vorbis encoder crashed on a whole track at once (as buildAudio.py found)"""
    with soundfile.SoundFile(path, 'w', rate, samples.shape[1], **options) as output:
        for start in range(0, len(samples), 65536):
            output.write(samples[start:start + 65536])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--preview', help='also write the change as heard in the fight there (an mp3)')
    args = parser.parse_args()
    track, rate = soundfile.read(P2, dtype='float64', always_2d=True)
    music = track[int(round(RESUME_AT * rate)):].copy()
    fade = int(FADE_IN * rate)
    music[:fade] *= numpy.linspace(0.0, 1.0, fade)[:, None]
    rewind, rewind_rate = soundfile.read(REWIND, dtype='float64', always_2d=True)
    if rewind_rate != rate:
        rewind = resample_poly(rewind, rate, rewind_rate, axis=0)
    if rewind.shape[1] != music.shape[1]:
        rewind = numpy.repeat(rewind[:, :1], music.shape[1], axis=1)

    def rms(samples):
        return float(numpy.sqrt(numpy.mean(samples ** 2)))
    drop = music[int(REWIND_LEVEL_FROM[0] * rate):int(REWIND_LEVEL_FROM[1] * rate)]
    gain = rms(drop) / rms(rewind[:int(8.0 * rate)])
    head = int(REWIND_TO_MUSIC * rate)
    out = numpy.zeros((head + len(music), music.shape[1]))
    out[head:] += music
    # As loud as the music, its peaks kept under the ceiling (a limiter would squash it)
    start = out[:len(rewind)]
    peak = numpy.abs(start + rewind * gain).max()
    if peak > 0.98:
        gain *= 0.98 / peak
    out[:len(rewind)] += rewind * gain
    write_blocks(P3, out.astype(numpy.float32), rate, format='OGG', subtype='VORBIS')
    print(f'{os.path.relpath(P3, REPO)}: {len(out) / rate:.2f} s (the rewind x{gain:.2f}, phase 2 from '
          f'{RESUME_AT:.2f} s at {REWIND_TO_MUSIC:.1f} s)')
    if args.preview:
        before = track[int(round((CUT_AT - 12.0) * rate)):int(round(CUT_AT * rate))]
        audition = numpy.concatenate([before, out[:head + int(35.0 * rate)]])
        audition *= min(1.0, 0.98 / numpy.abs(audition).max())
        write_blocks(args.preview, audition.astype(numpy.float32), rate, format='MP3')
        print(f'{args.preview}: {len(audition) / rate:.1f} s')


if __name__ == '__main__':
    main()
