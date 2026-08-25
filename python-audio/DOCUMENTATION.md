# High Performance Python for Audio Processing — Documentation

This course teaches **high performance Python** using **audio processing**
as the domain. Every chapter processes real signal data, *measures* it, and
then makes it faster. Audio is the ideal teaching domain for performance
work: signals are large arrays, effects are array math, real-time playback
imposes a hard deadline, and the results are audible — every run writes
`.wav` files into `out/` that any player can open.

The performance ladder the course climbs:

```
pure Python  →  vectorized NumPy  →  right dtypes, no copies
             →  better ALGORITHMS (FFT)  →  streaming under a deadline
             →  profiling; native code only as the last step
```

```bash
make deps   # pip install numpy (the only dependency)
make run    # run all 13 chapters (~2 s), writes out/*.wav
make solve  # chapters 1–3 solution: A# minor Lorenz attractor
make clean  # remove generated audio
```

Timing numbers below are from one representative run; they vary by machine.
**The ratios are the lesson, not the absolute numbers.**

---

## Table of Contents

1. [Digital audio in pure Python](#chapter-1--digital-audio-in-pure-python)
2. [The cost of pure Python](#chapter-2--the-cost-of-pure-python)
3. [Vectorization with NumPy](#chapter-3--vectorization-with-numpy)
   - [Foundations project: A# minor Lorenz attractor](#foundations-project--a-minor-lorenz-attractor)
4. [dtypes & memory](#chapter-4--dtypes--memory)
5. [Views, copies, in-place](#chapter-5--views-copies-in-place)
6. [A synthesis toolbox](#chapter-6--a-synthesis-toolbox)
7. [Filters: algorithms vs constants](#chapter-7--filters-algorithms-vs-constants)
8. [The FFT: the algorithmic win](#chapter-8--the-fft-the-algorithmic-win)
9. [Spectral denoising (STFT)](#chapter-9--spectral-denoising-stft)
10. [An effects chain](#chapter-10--an-effects-chain)
11. [Streaming & the real-time deadline](#chapter-11--streaming--the-real-time-deadline)
12. [Profiling & the playbook](#chapter-12--profiling--the-playbook)
13. [FM synthesis](#chapter-13--fm-synthesis)

---

## Chapter 1 — Digital Audio in Pure Python

### `demo_digital_audio()`

The domain fundamentals, deliberately in pure Python to show there is no
magic — audio is just a list of numbers:

- **Sampling**: sound is a pressure wave; digital audio measures it `SR`
  times per second (44,100 Hz — CD quality). The **Nyquist limit** `SR/2`
  is the highest representable frequency; 22,050 Hz covers human hearing.
- **A sine oscillator**: `s[i] = sin(2π · f · i / SR)`, one sample at a
  time in a list comprehension.
- **Loudness**: RMS (root mean square) expressed in **dBFS** — decibels
  relative to full scale. A full-scale sine is −3.0 dBFS; the demo
  verifies this exactly (RMS of a sine = amplitude/√2).
- **WAV output the painful way**: `struct.pack("<h", ...)` per sample into
  the stdlib `wave` module — manual clipping, manual int16 scaling. Every
  later chapter uses the vectorized `write_wav` helper and this chapter is
  why it's appreciated.

### `write_wav(path, signal, sr)`

The helper used everywhere else: `np.clip` to [−1, 1] (clipping distorts
gracefully; integer *wraparound* sounds like crackle — a real audio bug),
scale to ±32767, cast to `int16`, one `writeframes` call.

---

## Chapter 2 — The Cost of Pure Python

### `demo_interpreter_tax()`

Establishes the baseline with measurements. Multiplying 1,000,000 samples
by a gain in a `for` loop costs ~35 ms; the same as a list comprehension
~20 ms; `sum()` ~3 ms. Why: every Python bytecode operation pays
interpreter overhead — type dispatch, reference counting, boxed float
objects. A million samples pay it a million times.

### `bench(fn, repeat=3)`

The course's measurement tool: run three times, report the **minimum**.
The fastest run is the closest to the code's true cost; slower runs are
scheduler noise, cache misses from other processes, GC pauses. Min-of-N is
standard practice for micro-benchmarks (it's what `timeit` does).

The fix for the interpreter tax is not "write faster Python" — it is to
pay the overhead **once per array instead of once per element**.

---

## Chapter 3 — Vectorization with NumPy

### `demo_vectorization()`

The single biggest lever in numerical Python. `signal * 0.5` on a NumPy
array runs **one C loop** over unboxed, contiguous doubles (usually
SIMD-vectorized by the compiler): measured ~36× faster than the Python
loop; mixing two tracks (`0.5*a + 0.5*b`) and RMS
(`sqrt(mean(x*x))`) are similar one-liners at C speed.

The rule the chapter prints: if you wrote `for sample in signal` in
Python, you are leaving a ~100× speedup on the table. The rest of the
course is written in this style — no per-sample Python loops except where
they are the *point* (Chapters 7, 8, 12). The chapters 1–3 exercises are
solved separately in [`solutions/strange_attractor_saw.py`](solutions/strange_attractor_saw.py).

---

## Foundations project — A# minor Lorenz attractor

Lives in [`solutions/strange_attractor_saw.py`](solutions/strange_attractor_saw.py)
so the 12-chapter course file stays a teaching walkthrough. Run it with
`make solve` (or `python3 solutions/strange_attractor_saw.py`).

### `demo_attractor_foundations()`

The assigned exercises for chapters 1–3 — a **stereo WAV** (interleaved
channels) and the **break-even array size** where NumPy beats a Python
loop — solved as one audible object. Lorenz's 1963 convection model
(`σ=10`, `ρ=28`, `β=8/3`), the same three coupled ODEs as the
computational-physics course, is integrated with RK4. The state `(x, y, z)`
is the tuning CV of three **sawtooth** oscillators parked on an
**A# minor** triad (equal temperament, A4 = 440 Hz):

- `x` → A#4 = `440 · 2^(1/12)` ≈ 466.16 Hz (root)
- `y` → C#5 = `440 · 2^(4/12)` ≈ 554.37 Hz (minor third)
- `z` → F5  = `440 · 2^(8/12)` ≈ 698.46 Hz (perfect fifth; E# = F)

Exponential detune keeps it musical: `f = f0 · 2^(0.35 · u)` with each
coordinate clipped to roughly `[-1, 1]` against the classic attractor's
extents. Depth is ±0.35 octaves, so the triad stays recognizable while
the pitches wander. Lorenz is sampled at a 2 kHz **control rate**
(2 Lorenz-seconds per audio second — a few wing-flips over a 4 s clip)
and linearly interpolated up to 44.1 kHz.

RK4 is sequential — each step needs the last — so it stays in a Python
loop on purpose. That is the honest chapters 1–3 boundary: pay interpreter
cost where the math forces it; vectorize the oscillators.

**Chapter 1.** 0.75 s of the chord, entirely in lists: RK4, linear
resample, three phase-accumulator saws (`phase = (phase + f/SR) % 1`,
sample = `2·phase − 1` — the same identity Chapter 6 later writes as
`2·((t·f) mod 1) − 1`), then a stereo pan (left = A# + ½ F, right =
C# + ½ F) so the two Lorenz wings spatialize. Packed with
`struct.pack("<hh", …)` per frame into `out/asharp_minor_py.wav`. RMS/dBFS
is reported on the mid mix, same check as the sine demo. The Python saw
is verified against the NumPy `cumsum` form with `np.allclose`.

**Chapter 2.** `bench()` on 1 s of the same pipeline: RK4 at control
rate, three Python saws, the pan/mix. The saws dominate; every sample
pays bytecode. RK4 has to — sample *i* needs sample *i−1*. The saws do not.

**Chapter 3.** After `(x, y, z)` exist as arrays, `np.interp` to audio
rate and one expression per oscillator:

`saw = 2 · (cumsum(freq / SR) mod 1) − 1`

Four seconds of stereo lands in `out/asharp_minor.wav` via
`write_wav_stereo` (same clipping and int16 scaling as `write_wav`, two
channels interleaved `L0, R0, L1, R1, …`). A 1 s saw is timed Python vs
NumPy — this kernel is `cumsum` plus wrap, more arithmetic than
Chapter 3's `signal * 0.5`, so the ratio is smaller than that chapter's
~100× poster. Then a **break-even sweep** over
`n ∈ {10³, 10⁴, 10⁵, 10⁶}` on a constant-A#4 saw prints the smallest *n*
where NumPy wins — usually the first size in that list; the lesson is
that the crossover is a few thousand samples, not millions.

---

## Chapter 4 — dtypes & Memory

### `demo_dtypes()`

For big arrays, performance is often **memory bandwidth**: how many bytes
stream through the cache. The chapter measures 5M samples as float64
(40 MB), float32 (20 MB), and int16 (10 MB), then times the same
gain+offset on float64 vs float32 — roughly half the bytes, roughly half
the time (~7.8 ms vs ~2.7 ms measured).

Audio practice encoded here:

- **Process in float32** — 24 bits of mantissa is far more precision than
  any converter or ear; half the bandwidth of float64.
- **Store in int16** — the WAV format; conversion must `np.clip` first
  (61.7% of the deliberately-too-loud demo signal clips — visible in the
  output, audible in real life).
- **The silent promotion trap**: multiplying a float32 array by a Python
  float (float64) promotes the whole result to float64. The demo's
  `np.float32(0.5)` scalars are not pedantry — they keep the pipeline in
  32 bits.

---

## Chapter 5 — Views, Copies, In-Place

### `demo_views_and_copies()`

The chapter that separates NumPy users from NumPy understanders:

- **Slices are views**: `a[::2]` copies nothing — it is new strides over
  the same buffer, and writing through it writes the original (demoed:
  `view[0] = 99` changes `a`). `.copy()` is how independence is *chosen*.
  The printed `strides` make the mechanism concrete.
- **Arithmetic allocates**: `b = a * 0.5` creates a new 40 MB array every
  call (~4.0 ms); `a *= g` writes in place (~1.3 ms);
  `np.multiply(a, g, out=buf)` reuses a pre-allocated scratch buffer.
- **The real-time rule**: allocate buffers once at startup, never in the
  audio callback — hidden allocations mean latency spikes, and
  Chapter 11 measures why that matters.

---

## Chapter 6 — A Synthesis Toolbox

Fully vectorized building blocks — each oscillator is one array
expression, no loops:

- **`sine_osc(freq, duration)`** — `amp · sin(2π f t)` over a time vector.
- **`square_osc`** — `sign(sine)`: the sign function of a sine *is* a
  square wave.
- **`saw_osc`** — `2·((t·f) mod 1) − 1`: a rising ramp that resets each
  period.
- **`adsr(n)`** — the Attack-Decay-Sustain-Release envelope as four
  concatenated `linspace` ramps; multiplied onto a tone it turns a beep
  into a "note".
- **`make_chord`** — mixing is literally addition: three sines (A4, C♯5,
  E5 — an A-major triad), summed and scaled.
- **`make_arpeggio`** — four enveloped notes concatenated: a tiny
  sequencer.

2 seconds of chord (88,200 samples) synthesizes in ~2 ms. Both signals are
written to `out/` and reused by Chapters 8–11 as test material.

---

## Chapter 7 — Filters: Algorithms vs Constants

### `demo_filters()`

A moving average (the simplest low-pass filter) implemented three ways
teaches that speed has **two dimensions** — asymptotic complexity *and*
constant factors:

| Implementation | Complexity | Measured |
|---|---|---|
| Pure Python running sum | O(n) | ~12 ms |
| `np.convolve` | O(n·k) | ~2 ms |
| NumPy cumulative-sum trick | O(n) | ~0.8 ms |

The middle row is the punchline: **the asymptotically worse algorithm
wins** against interpreted O(n), because its constants are C constants
(with k=101, n·k is 20M simple operations — still faster than 200k
interpreted ones). The cumsum trick — window sums as differences of a
running total — wins both dimensions. All three are verified to agree with
`np.allclose`.

### The sequential exception

The one-pole IIR filter `y[i] = y[i-1] + α(x[i] − y[i-1])` has a feedback
dependency: sample i needs sample i−1's *output*. That is the one loop
shape NumPy cannot vectorize — there is no array expression for it. The
honest options, in order: keep the Python loop if it's fast enough
(measured ~7 ms for 200k samples), `scipy.signal.lfilter` (C loop),
`numba.njit` (JIT-compile the loop as-is, typically ~100×), Cython, or a C
extension — which is where this repository's C course connects.

---

## Chapter 8 — The FFT: The Algorithmic Win

### `naive_dft(x)` / `demo_fft()`

Chapters 2–5 bought constant factors; this chapter buys a different
**exponent**. The textbook DFT is O(n²) — for each of n output
frequencies, a sum over n samples. The Fast Fourier Transform computes the
identical result in O(n log n).

Measured at n=1024: naive pure-Python DFT ~120 ms, `np.fft.fft`
~0.012 ms — **~10,000×**, and verified equal with `np.allclose`. The gap
is not a constant: doubling n doubles the FFT's work but *quadruples* the
DFT's. At a million samples the naive version is simply not runnable.
The lesson printed by the chapter: **check the algorithm before
micro-optimizing** — no amount of vectorization rescues the wrong big-O.

The second half *uses* the tool: `np.fft.rfft` on the chord, find the
three strongest spectral peaks (with a small exclusion zone around each to
skip spectral leakage), and recover the notes: 440.1, 554.5, 659.5 Hz
against true values 440.0, 554.37, 659.25 — the difference is exactly the
bin resolution SR/n = 1.35 Hz, which the output explains.

---

## Chapter 9 — Spectral Denoising (STFT)

### `demo_spectral_denoise()`

The capstone DSP chapter — the core of every noise-removal plugin:

1. **Frame** the signal into overlapping windows:
   `sliding_window_view(noisy, 1024)[::512]` — Chapter 5's views doing
   real work: framing without copying.
2. **Window** each frame with a *periodic* Hann window
   (`np.hanning(N+1)[:-1]`). At 50% overlap, shifted periodic-Hann copies
   sum to exactly 1 — the **COLA** (constant overlap-add) property that
   makes step 5 reconstruct perfectly.
3. **FFT all frames at once**: `np.fft.rfft(frames, axis=1)` — the
   axis argument turns 171 FFTs into one vectorized call.
4. **Gate**: estimate the noise floor as the *median* spectral magnitude
   (valid because the signal's energy is concentrated in a few bins while
   noise spreads everywhere), zero every bin below 4× that —
   one boolean-mask assignment.
5. **Overlap-add** the inverse FFTs back into a signal.

Measured: 2 s of noisy audio processed in ~9 ms; SNR improves from
17.3 dB to 32.8 dB (**+15.5 dB**), computed against the known clean
signal with edges trimmed (they lack full window overlap). Both
`out/noisy.wav` and `out/denoised.wav` are written — the difference is
clearly audible. The RNG is seeded, so the result is reproducible.

---

## Chapter 10 — An Effects Chain

Three classic effects, each a couple of vectorized lines:

- **`echo(x, delay, feedback)`** — add a delayed, attenuated copy:
  `y[d:] += 0.4 * x[:-d]`. The "delay line" is an index shift.
- **`distortion(x, drive)`** — waveshaping: push the signal through
  `tanh`. Any nonlinear function bends the waveform and adds harmonics;
  tanh soft-clips like an overdriven amplifier.
- **`tremolo(x, rate, depth)`** — multiply by a low-frequency oscillator:
  amplitude modulation at 5 Hz.

The full chain on 1.2 s of arpeggio: ~0.6 ms — **~2000× faster than real
time**, which the chapter frames as *headroom*: that factor is what allows
a DAW to run dozens of tracks with dozens of plugins live.
`out/effects.wav` vs `out/arpeggio.wav` is the before/after.

---

## Chapter 11 — Streaming & the Real-Time Deadline

### `class StreamingEcho` / `demo_streaming()`

Live audio does not arrive as one array — it arrives in **blocks**
(512 samples = 11.6 ms at 44.1 kHz), and the iron rule of real-time audio
is: *every* block must be processed faster than it plays, or the output
audibly glitches.

`StreamingEcho` is the shape of every real-time effect:

- **State across blocks**: an echo needs input from `delay` samples ago,
  which usually falls in a *previous* block — so the last `delay` input
  samples are carried in `self.tail` between calls, exactly like a
  plugin's internal state.
- **Buffers allocated once** in `__init__` (Chapter 5's rule).

The demo processes 4.8 s of audio in 413 blocks and reports what real-time
engineers actually watch: not the average but the **worst block**
(~0.012 ms = 0.1% of the 11.6 ms deadline) and the real-time factor
(~3000×). One missed deadline is one glitch — which is why real-time
callbacks ban allocation, GC pressure, locks, and unbounded work.

---

## Chapter 12 — Profiling & the Playbook

### `demo_profiling()`

Never optimize blind. The chapter profiles a deliberately slow windowed-RMS
"loudness curve" (pure Python, nested loop) with **cProfile**, showing the
hotspot where the time actually goes — then fixes precisely that hotspot
with `sliding_window_view` + vectorized mean: ~18× measured, results
verified identical with `np.allclose`.

The optimization playbook the course has been building, in order:

1. **Measure** (`bench`, cProfile) — the hotspot is rarely where you think.
2. **Better algorithm** — Chapter 8's exponent beats any constant.
3. **Vectorize** — Chapter 3's ~100×.
4. **dtypes & copies** — Chapters 4–5's bandwidth and allocation wins.
5. **Only then, native code** — numba (JIT a loop with one decorator),
   Cython, or a C extension (see this repository's C course). Most audio
   code never needs step 5: NumPy *is* C underneath.

Beyond this course, the same ideas power the real ecosystem: `scipy.signal`
(filters done right), `librosa` (music analysis), `sounddevice`/`pyaudio`
(real microphone/speaker streams with exactly Chapter 11's callback model),
and `numba`/`cython` for the sequential exceptions.

---

## Chapter 13 — FM Synthesis

### `fm_osc(carrier, modulator, index, duration)` / `demo_fm()`

John Chowning's 1973 observation: a *complex spectrum from two sines*.
Phase-modulate a carrier by a modulator — no wavetable, no filter:

`y = sin(2π · fc · t + I · sin(2π · fm · t))`

Three knobs, all musical:

- **Ratio `fc:fm`**: an integer ratio (4:1, 1:1, 2:1) stacks sidebands on
  the harmonic series; a non-integer ratio (1.4, 1.414) is inharmonic —
  bells, struck metal.
- **Index `I`**: brightness. Roughly `I+1` sidebands on each side of the
  carrier, at `fc ± k·fm`. Carson's bandwidth rule of thumb:
  `BW ≈ 2(I+1)fm`.
- **Time-varying `I`**: the DX7 trick. Envelope the index with the same
  ADSR as the amplitude and the timbre *follows* loudness — brass gets
  brighter as it attacks, a bell darkens as it dies.

The formula is one NumPy expression (`index` may be a scalar or a
per-sample array). Measured on 1 s / 44,100 samples of a stationary
4:1 tone (`fc=800`, `fm=200`, `I=2.5`): the pure-Python loop vs
`fm_osc` — tens to hundreds of ×, results identical with `np.allclose`.

Chapter 8's FFT is the spectrum ruler. Energy is measured at the
predicted bins `fc ± k·fm` (200, 400, 600, 800, 1000, 1200, 1400 Hz)
rather than by picking the loudest peaks: at `I = 2.5` the carrier
itself is near a zero of the Bessel function `J0`, so it is *quieter*
than the first sidebands — a real FM quirk, not a bug. Two sines, a
harmonic spectrum, on the grid.

Two instruments, both closed-form:

- **`fm_bell`**: ratio 1.4, `I` and amplitude exponential-decay — the
  classic inharmonic strike. `out/fm_bell.wav`.
- **`fm_brass`**: 1:1 at A4, `I = 4 · ADSR` — brightness tracks the
  envelope. `out/fm_brass.wav`.

FM is the algorithmic counterpart of Chapter 8: additive synthesis of the
same spectrum would sum `I+1` sines weighted by Bessel `Jn(I)`. Two
sines plus a multiply replace that sum.

---

## Suggested Course Progression

| Stage | Chapters | Exercises to assign |
|---|---|---|
| Foundations | 1–3 | Lorenz `(x,y,z)` detunes three saws around A# minor; stereo interleaved WAV + break-even *n* — [`solutions/strange_attractor_saw.py`](solutions/strange_attractor_saw.py) |
| Memory | 4–5 | Convert the whole pipeline to float32 end-to-end and measure; find the hidden copy in a given snippet (`x = x[::2] * 2`) |
| DSP | 6–9, 13 | Add a triangle oscillator; implement biquad filtering with `scipy.signal.lfilter` and compare to the one-pole; window with Hamming and measure the COLA error; add a 2-operator feedback FM voice (`I` into its own modulator) |
| Systems | 10–12 | Add a flanger (modulated delay — needs interpolated reads); drive `StreamingEcho` from a real microphone with `sounddevice`; JIT the one-pole IIR with numba and measure |
