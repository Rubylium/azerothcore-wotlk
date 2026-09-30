"""Builds The Hollow Voice's ability sounds (HollowVoice.cpp Sounds, SoundEntries 30120-30138) for the client patch.

The picks are retail paladin and void sounds (localTools/hollowVoice/sounds, extracted from the retail client). They came
from -9.6 to -24 LUFS, some clipping: in a raid of ten players' spells the quiet ones were lost. Each is brought to
TARGET_LUFS (a measured gain, then a limiter at TRUE_PEAK: a boss's sounds stand out over the players'), written as
Ogg Vorbis into modules/mod-stat-growth/client-assets/compiled/sounds/hollowvoice, which patchFiles.js ships.

    python localTools/hollowVoice/buildSounds.py
"""
import glob
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
OUTPUT = os.path.join(REPO, 'modules', 'mod-stat-growth', 'client-assets', 'compiled', 'sounds', 'hollowvoice')
TARGET_LUFS = -10.0
TRUE_PEAK = -1.0


def ffmpeg():
    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except ImportError:
        return 'ffmpeg'


def loudness(tool, path):
    import json
    result = subprocess.run([tool, '-hide_banner', '-nostats', '-i', path, '-af', 'loudnorm=print_format=json', '-f',
                             'null', '-'], capture_output=True, text=True, check=True)
    text = result.stderr
    return float(json.loads(text[text.rindex('{'):text.rindex('}') + 1])['input_i'])


def render(tool, source, target, gain):
    # The gain, then a limiter holding the peaks under TRUE_PEAK (a short hit keeps its punch, never clips)
    limit = 10 ** (TRUE_PEAK / 20.0)
    filters = f'volume={gain:.2f}dB,alimiter=limit={limit:.3f}:attack=1:release=40:level=false'
    subprocess.run([tool, '-hide_banner', '-nostats', '-y', '-i', source, '-af', filters, '-ar', '44100',
                    '-codec:a', 'libvorbis', '-q:a', '6', target], capture_output=True, check=True)


def main():
    os.makedirs(OUTPUT, exist_ok=True)
    tool = ffmpeg()
    for source in sorted(glob.glob(os.path.join(HERE, 'sounds', '*.ogg'))):
        target = os.path.join(OUTPUT, os.path.basename(source))
        # A measured gain, then again for what the limiter took back (loudnorm's single pass adapts over a short
        # sound and left some quieter than they came)
        gain = TARGET_LUFS - loudness(tool, source)
        render(tool, source, target, gain)
        gain += TARGET_LUFS - loudness(tool, target)
        render(tool, source, target, gain)
        print(f'{os.path.basename(target)}: {loudness(tool, target):.1f} LUFS')


if __name__ == '__main__':
    main()
