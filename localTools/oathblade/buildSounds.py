"""Compile Oathblade's source OGG effects as Wrath-compatible PCM WAV effects."""

from pathlib import Path


repoRoot = Path(__file__).resolve().parents[2]
sourceRoot = repoRoot / 'modules' / 'mod-oathblade' / 'client-assets' / 'sounds'
outputRoot = repoRoot / 'modules' / 'mod-oathblade' / 'client-assets' / 'compiled' / 'sounds'


def main():
    sources = sorted(sourceRoot.rglob('*.ogg'))
    if not sources:
        raise RuntimeError(f'No Oathblade source sounds found in {sourceRoot}')

    pending = []
    for source in sources:
        output = (outputRoot / source.relative_to(sourceRoot)).with_suffix('.wav')
        if not output.exists() or output.stat().st_mtime < source.stat().st_mtime:
            pending.append((source, output))

    if pending:
        try:
            import numpy as np
            import soundfile as sf
        except ImportError as error:
            raise RuntimeError('Converting changed sounds requires: python -m pip install soundfile') from error

        for source, output in pending:
            samples, sampleRate = sf.read(source, dtype='float32')
            samples = np.nan_to_num(samples)
            peak = float(np.max(np.abs(samples)))
            if peak > 0.97:
                samples *= 0.97 / peak
            output.parent.mkdir(parents=True, exist_ok=True)
            sf.write(output, samples, sampleRate, subtype='PCM_16')

    print(f'Oathblade sounds: {len(sources)} PCM WAV files, {len(pending)} compiled')


if __name__ == '__main__':
    main()
