"""Builds The Hollow Voice's two fight tracks (modules/mod-stat-growth/src/HollowVoice.cpp) for the client patch.

Archbishop Aldric's track (phase 1, 2:03) and Vel'thazar's (the rest, 5:30) are normalised to the loudness of
L'Infini's shipped tracks (-12 LUFS, true peak -1 dB: loud enough at the client's music volume, never clipping), with
ffmpeg's loudnorm in two passes so the dynamics stay (linear gain when the peak allows), and written as 44.1 kHz stereo
MP3 into modules/mod-stat-growth/client-assets/compiled/music, which patchFiles.js ships.

The fight plays them as one track (HollowVoice.mp3): the Archbishop's up to 2:02.0, in its fade, then Vel'thazar's.
Two tracks sent one after the other did not play: the first reaching its own end stopped the music the client had just
been sent, even a second one already playing (tried 2026-09-30). The two alone stay for .hollow music.

Usage: python localTools/hollowVoice/buildMusic.py <archbishop track> <vel'thazar track> [--ffmpeg <ffmpeg.exe>]
"""
import argparse
import json
import os
import subprocess

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUTPUT = os.path.join(REPO, 'modules', 'mod-stat-growth', 'client-assets', 'compiled', 'music')
TARGET_LUFS = -12.0
TRUE_PEAK = -1.0
RANGE = 11.0
SWITCH_SECONDS = 122.0      # HollowVoice.cpp AtSecondTrack


def default_ffmpeg():
    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except ImportError:
        return 'ffmpeg'


def measure(ffmpeg, source):
    filter_ = f'loudnorm=I={TARGET_LUFS}:TP={TRUE_PEAK}:LRA={RANGE}:print_format=json'
    result = subprocess.run([ffmpeg, '-hide_banner', '-nostats', '-i', source, '-af', filter_, '-f', 'null', '-'],
                            capture_output=True, text=True, check=True)
    text = result.stderr
    return json.loads(text[text.rindex('{'):text.rindex('}') + 1])


def normalise(ffmpeg, source, target):
    stats = measure(ffmpeg, source)
    filter_ = (f"loudnorm=I={TARGET_LUFS}:TP={TRUE_PEAK}:LRA={RANGE}:measured_I={stats['input_i']}:"
               f"measured_TP={stats['input_tp']}:measured_LRA={stats['input_lra']}:"
               f"measured_thresh={stats['input_thresh']}:offset={stats['target_offset']}:linear=true")
    subprocess.run([ffmpeg, '-hide_banner', '-nostats', '-y', '-i', source, '-af', filter_, '-ar', '44100', '-ac', '2',
                    '-codec:a', 'libmp3lame', '-b:a', '192k', target], capture_output=True, check=True)
    print(f"{os.path.basename(target)}: {stats['input_i']} LUFS -> {TARGET_LUFS}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('archbishop')
    parser.add_argument('velthazar')
    parser.add_argument('--ffmpeg', default=default_ffmpeg())
    args = parser.parse_args()
    os.makedirs(OUTPUT, exist_ok=True)
    normalise(args.ffmpeg, args.archbishop, os.path.join(OUTPUT, 'HollowVoiceAldric.mp3'))
    normalise(args.ffmpeg, args.velthazar, os.path.join(OUTPUT, 'HollowVoiceVelthazar.mp3'))
    join(args.ffmpeg)


def join(ffmpeg):
    aldric = os.path.join(OUTPUT, 'HollowVoiceAldric.mp3')
    velthazar = os.path.join(OUTPUT, 'HollowVoiceVelthazar.mp3')
    target = os.path.join(OUTPUT, 'HollowVoice.mp3')
    graph = f'[0]atrim=0:{SWITCH_SECONDS},asetpts=N/SR/TB[a];[1]asetpts=N/SR/TB[v];[a][v]concat=n=2:v=0:a=1'
    subprocess.run([ffmpeg, '-hide_banner', '-nostats', '-y', '-i', aldric, '-i', velthazar, '-filter_complex', graph,
                    '-ar', '44100', '-ac', '2', '-codec:a', 'libmp3lame', '-b:a', '192k', target],
                   capture_output=True, check=True)
    print(f'{os.path.basename(target)}: the two joined at {SWITCH_SECONDS} s')


if __name__ == '__main__':
    main()
