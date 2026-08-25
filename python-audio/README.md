# High Performance Python for Audio Processing

A course that teaches **high performance Python** through **audio
processing**: every chapter processes real signal data, *measures* how long
it takes, and then makes it faster. All audio is synthesized (no assets
needed), and every run writes `.wav` files into `out/` that you can listen
to.

The performance ladder:

```
pure Python  →  vectorized NumPy  →  right dtypes, no copies
             →  better ALGORITHMS (FFT)  →  streaming under a deadline
             →  profiling; native code only as the last resort
```

## Chapters

1. **Digital audio in pure Python** — samples, Nyquist, dBFS, WAV by hand
2. **The cost of pure Python** — measuring the interpreter tax (1M samples)
3. **Vectorization with NumPy** — the ~50–100× free speedup
4. **dtypes & memory** — float64/float32/int16, bandwidth, the promotion trap
5. **Views, copies, in-place** — allocation is the silent killer
6. **A synthesis toolbox** — oscillators, ADSR envelopes, chords, an arpeggio
7. **Filters** — one algorithm, three speeds: complexity *and* constants (plus the sequential IIR exception)
8. **The FFT** — O(n²) → O(n log n): a measured ~10,000× at n=1024, then note detection
9. **Spectral denoising** — STFT with zero-copy framing, spectral gating, overlap-add: +15 dB SNR in ~9 ms
10. **An effects chain** — echo, distortion, tremolo: ~2000× faster than real time
11. **Streaming & real time** — block processing with state, the 11.6 ms deadline, worst-block analysis
12. **Profiling & the playbook** — cProfile the hotspot, fix it, and the 5-step optimization order
13. **FM synthesis** — Chowning: two sines make a spectrum; sidebands via FFT; a bell and a brass note

Every function is documented in [DOCUMENTATION.md](DOCUMENTATION.md), with
the concepts, the numbers to expect, and pointers to the production
ecosystem (scipy.signal, librosa, sounddevice, numba).

## Running

Requires Python 3.10+ and NumPy (the only dependency).

```bash
make deps   # python3 -m pip install -r requirements.txt
make run    # run all 13 chapters (~2 seconds)
make solve  # chapters 1–3 solution: A# minor Lorenz attractor
make clean  # remove generated audio and caches
```

Timing output varies by machine — **the ratios are the lesson**, not the
absolute numbers.

## Listen to your work

Each run writes to `out/`:

| File | Made by | What you hear |
|---|---|---|
| `tone.wav` | Ch 1 | a 440 Hz sine, built sample-by-sample |
| `asharp_minor_py.wav` | `make solve` | 0.75 s A# minor Lorenz chord, packed stereo by hand |
| `asharp_minor.wav` | `make solve` | 4 s of the same chord, vectorized saws — *listen to this* |
| `chord.wav` | Ch 6 | an A-major triad (three mixed sines) |
| `arpeggio.wav` | Ch 6 | four ADSR-enveloped notes |
| `noisy.wav` / `denoised.wav` | Ch 9 | the chord drowned in noise, then spectrally gated |
| `effects.wav` | Ch 10 | the arpeggio through echo → distortion → tremolo |
| `fm_bell.wav` | Ch 13 | inharmonic FM bell (ratio 1.4, decaying index) |
| `fm_brass.wav` | Ch 13 | harmonic FM brass: brightness follows the ADSR |

## Layout

| File | Purpose |
|---|---|
| `main.py` | All 13 chapters, one runnable file |
| `solutions/strange_attractor_saw.py` | Chapters 1–3 solution: Lorenz-tuned A# minor saws |
| `DOCUMENTATION.md` | Written explanation of every chapter and function |
| `Makefile` | `run`, `solve`, `deps`, `clean` |
| `requirements.txt` | numpy |
