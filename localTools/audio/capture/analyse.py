"""Each sound event of a capture (run.ps1): its length above the floor, its attack's and body's level and how bright
they are (energy above 2 kHz: a muffled sound has almost none). Usage: python analyse.py <capture.wav>"""
import sys, wave, numpy as np
w = wave.open(sys.argv[1]); r = w.getframerate(); ch = w.getnchannels()
a = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).reshape(-1, ch).astype(np.float32).mean(axis=1)
win = int(r * 0.02)
env = np.array([np.abs(a[i:i + win]).mean() for i in range(0, len(a) - win, win)])
db = 20 * np.log10(env + 1)
floor = np.median(db)
onsets = []
last = -10
for i in range(1, len(db)):
    if db[i] > floor + 10 and db[i - 1] <= floor + 10 and i * 0.02 - last > 2.5:
        onsets.append(i * 0.02); last = i * 0.02
print('floor %.1f dB' % floor)
for t in onsets:
    s0 = int(t * r)
    def band(off, length):
        s = s0 + int(off * r); n = int(length * r); seg = a[s:s + n] * np.hanning(n)
        sp = np.abs(np.fft.rfft(seg)) ** 2; f = np.fft.rfftfreq(n, 1 / r)
        return 10 * np.log10(sp.sum() / n + 1), 100 * sp[f > 2000].sum() / sp.sum()
    i0 = int(t / 0.02); dur = 0
    while i0 + dur < len(db) and db[i0 + dur] > floor + 6: dur += 1
    l1, h1 = band(0, 0.05); l2, h2 = band(0.2, 0.5)
    print('t=%6.2f above floor %.2fs  attack %5.1f dB %4.1f%% HF  body %5.1f dB %4.1f%% HF  peak %.1f' % (
        t, dur * 0.02, l1, h1, l2, h2, db[i0:i0 + 50].max()))
