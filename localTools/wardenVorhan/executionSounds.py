"""The numbered execution's sounds: a clock's tick and tock (cut from the game's grandfather clock) and the DOOM (the
troll gong with the clockwork giant's ground pound under it), written mono 44.1 kHz 16-bit."""
import os
import sys
import wave

import numpy as np

source, out = sys.argv[1], sys.argv[2]
RATE = 44100


def load(path):
    w = wave.open(path)
    n, sr, ch, sw = w.getnframes(), w.getframerate(), w.getnchannels(), w.getsampwidth()
    a = np.frombuffer(w.readframes(n), dtype=np.int16 if sw == 2 else np.uint8).astype(float)
    a = (a - 128.0) / 128.0 if sw == 1 else a / 32768.0
    if ch > 1:
        a = a.reshape(-1, ch).mean(1)
    if sr != RATE:
        x = np.arange(0, len(a) * RATE / sr) * sr / RATE
        a = np.interp(x, np.arange(len(a)), a)
    return a


def cut(a, start, end, fade_in=0.004, fade_out=0.06):
    piece = a[int(start * RATE):int(end * RATE)].copy()
    fi, fo = int(fade_in * RATE), int(fade_out * RATE)
    piece[:fi] *= np.linspace(0, 1, fi)
    piece[-fo:] *= np.linspace(1, 0, fo)
    return piece


def save(path, a):
    a = a / max(np.abs(a).max(), 1e-9) * 0.89
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((a * 32767).astype(np.int16).tobytes())


clock = load(os.path.join(source, 'Sound', 'Doodad', 'Grandfather_Clock_01.wav'))
os.makedirs(out, exist_ok=True)
save(os.path.join(out, 'VorhanExecutionTick.wav'), cut(clock, 0.17, 0.55))
save(os.path.join(out, 'VorhanExecutionTock.wav'), cut(clock, 0.79, 1.24))

gong = load(os.path.join(source, 'Sound', 'Doodad', 'G_GongTroll01.wav'))
pound = load(os.path.join(source, 'Sound', 'Creature', 'ClockworkGiant', 'ClockworkGiant_PoundGround.wav'))
gong = cut(gong, 0.0, 2.6, fade_out=0.8)
# The pound's first blow (it starts 0.2 s in), lined up on the gong's strike, half as loud
blow = cut(pound, 0.18, 0.95, fade_out=0.25)
doom = gong.copy()
doom[:len(blow)] += 0.5 * blow / max(np.abs(blow).max(), 1e-9) * np.abs(gong).max()
save(os.path.join(out, 'VorhanExecutionDoom.wav'), doom)
print('written', os.listdir(out))
