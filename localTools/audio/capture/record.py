# Records what the speakers play while a run of the capture rig plays (run.ps1); needs pyaudiowpatch.
"""Records what the default speakers play (WASAPI loopback) for N seconds into a WAV, with its start time.
Usage: python record.py <seconds> <out.wav>"""
import sys
import time
import wave

import pyaudiowpatch as pa

seconds, out = float(sys.argv[1]), sys.argv[2]
p = pa.PyAudio()
wasapi = p.get_host_api_info_by_type(pa.paWASAPI)
speakers = p.get_device_info_by_index(wasapi['defaultOutputDevice'])
loopback = next(d for d in p.get_loopback_device_info_generator() if speakers['name'] in d['name'])
rate, channels = int(loopback['defaultSampleRate']), loopback['maxInputChannels']
stream = p.open(format=pa.paInt16, channels=channels, rate=rate, input=True, frames_per_buffer=1024,
                input_device_index=loopback['index'])
with wave.open(out, 'wb') as w:
    w.setnchannels(channels)
    w.setsampwidth(2)
    w.setframerate(rate)
    start = time.time()
    open(out + '.start', 'w').write(repr(start))
    while time.time() - start < seconds:
        w.writeframes(stream.read(1024, exception_on_overflow=False))
stream.close()
p.terminate()
print('recorded', out, 'from', time.strftime('%H:%M:%S', time.localtime(start)))
