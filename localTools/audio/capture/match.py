"""Finds the bank's sounds in a capture by matching their files against it (cross-correlation): each one heard, when,
and its gain over its first 0.3 s and over the rest - whatever else the speakers played (other programs, the game).
A sound cut short or muffled shows a tail gain far below its head's.
Usage: python match.py <capture.wav> <key prefix> [<key prefix> ...]   (e.g. Outlaw.PistolShot Outlaw.BetweenTheEyes)"""
import glob
import os
import sys
import wave

import numpy as np

RATE = 16000
SOUNDS = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', 'clientPatcher', 'addons',
                      'EvolutionsAudio', 'Sounds')


def load(path):
    with wave.open(path) as w:
        rate, channels = w.getframerate(), w.getnchannels()
        data = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(np.float32)
    data = data.reshape(-1, channels).mean(axis=1)
    positions = np.arange(0, len(data) - 1, rate / RATE)
    return np.interp(positions, np.arange(len(data)), data)


def correlate(signal, template):
    size = 1 << int(np.ceil(np.log2(len(signal) + len(template))))
    spectrum = np.fft.rfft(signal, size) * np.conj(np.fft.rfft(template, size))
    return np.fft.irfft(spectrum, size)[:len(signal)]


def gain(capture, template, at, start, stop):
    a, b = int(start * RATE), min(int(stop * RATE), len(template), len(capture) - at)
    if b <= a:
        return 0.0
    piece, reference = capture[at + a:at + b], template[a:b]
    return float(np.dot(piece, reference) / max(np.dot(reference, reference), 1.0))


capture = load(sys.argv[1])
found = []
for prefix in sys.argv[2:]:
    for path in sorted(glob.glob(os.path.join(SOUNDS, prefix + '_*.wav'))):
        template = load(path)
        score = correlate(capture, template) / max(np.dot(template, template), 1.0)
        threshold = 0.25 * score.max()
        index = 0
        while index < len(score):
            if score[index] >= threshold:
                window = score[index:index + RATE]
                peak = index + int(np.argmax(window))
                found.append((peak / RATE, os.path.basename(path), gain(capture, template, peak, 0, 0.3),
                              gain(capture, template, peak, 0.3, 3.0), len(template) / RATE))
                index = peak + RATE
            else:
                index += 1
# One event a second: the file that matches it best (its gain the highest)
best = []
for item in sorted(found):
    if best and item[0] - best[-1][0] < 1.0:
        if item[2] > best[-1][2]:
            best[-1] = item
    else:
        best.append(item)
for at, name, head, tail, length in best:
    head_db = 20 * np.log10(max(head, 1e-6))
    tail_db = 20 * np.log10(max(tail, 1e-6))
    print('%6.2fs %-28s head %6.1f dB  tail %6.1f dB  (tail - head %5.1f dB, file %.1fs)' % (
        at, name, head_db, tail_db, tail_db - head_db, length))
