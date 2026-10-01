"""Builds The Hollow Voice's two fight tracks (modules/mod-stat-growth/src/HollowVoice.cpp) for the client patch.

Archbishop Aldric's track (phase 1, 2:03) and Vel'thazar's (the rest, 5:30) are normalised to the loudness of
L'Infini's shipped tracks (-12 LUFS, true peak -1 dB: loud enough at the client's music volume, never clipping), with
ffmpeg's loudnorm in two passes so the dynamics stay (linear gain when the peak allows), and written as 44.1 kHz stereo
MP3 into modules/mod-stat-growth/client-assets/compiled/music, which patchFiles.js ships.

The fight plays them as one track (HollowVoice.mp3): the Archbishop's up to 2:00.0, the end of its fade, GAP_SECONDS of
silence (the break: the fallen Archbishop's warning), then Vel'thazar's.
Two tracks sent one after the other did not play: the first reaching its own end stopped the music the client had just
been sent, even a second one already playing (tried 2026-09-30). The two alone stay for .hollow music.
HollowVoiceFromReveal.mp3 is the fight's track from 1:50, for a pull armed with .hollow reveal: a music sent over
another fades out a few seconds in, so a test starting late needs its own track, sent alone.

Usage: python localTools/hollowVoice/buildMusic.py <archbishop track> <vel'thazar track> [--ffmpeg <ffmpeg.exe>]
"""
import argparse
import json
import os
import subprocess

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUTPUT = os.path.join(REPO, 'modules', 'mod-stat-growth', 'client-assets', 'compiled', 'music')
TARGET_LUFS = -9.0          # was L'Infini's -12: under the fight's sounds (-7, impacts -5) the music was lost
TRUE_PEAK = -1.0
RANGE = 11.0
SWITCH_SECONDS = 120.0      # the Archbishop's track ends (its fade done)
GAP_SECONDS = 5.0           # HollowVoice.cpp TransitionMs: Vel'thazar's starts at AtSecondTrack = SWITCH + GAP
# Vel'thazar's track whole: its first 1.5 s are drum hits, heard as the demon tears out (cut once as a fade-in: the
# user wants them; HollowVoice.cpp SecondTrackTrimMs)
VELTHAZAR_TRIM_SECONDS = 0.0
REVEAL_TEST_SECONDS = 110.0 # HollowVoice.cpp RevealTestStartMs


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
    parser.add_argument('archbishop', nargs='?')
    parser.add_argument('velthazar', nargs='?')
    parser.add_argument('--ffmpeg', default=default_ffmpeg())
    parser.add_argument('--join-only', action='store_true', help='join the normalised tracks already in the output')
    parser.add_argument('--renormalise', action='store_true',
                        help='normalise the tracks already in the output again (to TARGET_LUFS), then join')
    args = parser.parse_args()
    os.makedirs(OUTPUT, exist_ok=True)
    if args.renormalise:
        for name in ('HollowVoiceAldric.mp3', 'HollowVoiceVelthazar.mp3'):
            shipped = os.path.join(OUTPUT, name)
            source = shipped + '.source.mp3'
            os.replace(shipped, source)
            normalise(args.ffmpeg, source, shipped)
            os.remove(source)
    if args.join_only or args.renormalise:
        join(args.ffmpeg)
        return
    normalise(args.ffmpeg, args.archbishop, os.path.join(OUTPUT, 'HollowVoiceAldric.mp3'))
    normalise(args.ffmpeg, args.velthazar, os.path.join(OUTPUT, 'HollowVoiceVelthazar.mp3'))
    join(args.ffmpeg)


def join(ffmpeg):
    aldric = os.path.join(OUTPUT, 'HollowVoiceAldric.mp3')
    velthazar = os.path.join(OUTPUT, 'HollowVoiceVelthazar.mp3')
    target = os.path.join(OUTPUT, 'HollowVoice.mp3')
    graph = (f'[0]atrim=0:{SWITCH_SECONDS},asetpts=N/SR/TB[a];[1]atrim=0:{GAP_SECONDS},asetpts=N/SR/TB[g];'
             f'[2]atrim=start={VELTHAZAR_TRIM_SECONDS},asetpts=N/SR/TB[v];[a][g][v]concat=n=3:v=0:a=1')
    silence = ['-f', 'lavfi', '-i', 'anullsrc=channel_layout=stereo:sample_rate=44100']
    subprocess.run([ffmpeg, '-hide_banner', '-nostats', '-y', '-i', aldric, *silence, '-i', velthazar,
                    '-filter_complex', graph,
                    '-ar', '44100', '-ac', '2', '-codec:a', 'libmp3lame', '-b:a', '192k', target],
                   capture_output=True, check=True)
    print(f'{os.path.basename(target)}: the two joined at {SWITCH_SECONDS} s, {GAP_SECONDS} s apart')
    late = os.path.join(OUTPUT, 'HollowVoiceFromReveal.mp3')
    subprocess.run([ffmpeg, '-hide_banner', '-nostats', '-y', '-i', target, '-af',
                    f'atrim={REVEAL_TEST_SECONDS},asetpts=N/SR/TB', '-ar', '44100', '-ac', '2', '-codec:a',
                    'libmp3lame', '-b:a', '192k', late], capture_output=True, check=True)
    print(f'{os.path.basename(late)}: from {REVEAL_TEST_SECONDS} s')


if __name__ == '__main__':
    main()
