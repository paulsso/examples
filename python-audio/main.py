#!/usr/bin/env python3
# ============================================================================
#  main.py — High Performance Python for Audio Processing
# ============================================================================
#
#  This course teaches HIGH PERFORMANCE PYTHON with audio as the domain:
#  every chapter processes real signal data, measures how long it takes, and
#  then makes it faster. The performance ladder it climbs:
#
#      pure Python  ->  vectorized NumPy  ->  better dtypes & no copies
#                   ->  better ALGORITHMS (FFT)  ->  streaming & profiling
#
#  Audio is the perfect domain for this: signals are big arrays, effects are
#  array math, real-time imposes a hard deadline (a block of samples MUST be
#  processed faster than it plays), and you can listen to your results —
#  every run writes .wav files into out/ that any player can open.
#
#  Chapters:
#    1.  Digital audio in pure Python  — samples, sample rate, dBFS, WAV
#    2.  The cost of pure Python      — measuring the interpreter tax
#    3.  Vectorization with NumPy     — the 10-100x free speedup
#    4.  dtypes & memory              — float64/float32/int16, bandwidth
#    5.  Views, copies, in-place      — allocation is the silent killer
#    6.  A synthesis toolbox          — oscillators, envelopes, mixing
#    7.  Filters                      — algorithms vs constants (and the
#                                       sequential IIR exception)
#    8.  The FFT                      — O(n^2) -> O(n log n), the biggest
#                                       win in signal processing
#    9.  Spectral denoising           — STFT, thresholding, overlap-add
#    10. An effects chain             — echo, distortion, tremolo
#    11. Streaming & real time       — block processing, the deadline
#    12. Profiling & the playbook    — cProfile, then fix the hotspot
#
#  Requires: Python 3.10+, NumPy. Run: `make run` (or python3 main.py).
#  Timing numbers vary per machine/run; the RATIOS are the lesson.
# ============================================================================

import cProfile
import io
import math
import os
import pstats
import struct
import wave
from time import perf_counter

import numpy as np
from numpy.lib.stride_tricks import sliding_window_view

SR = 44100                     # samples per second (CD quality)
OUT_DIR = "out"


def chapter(title):
    print("\n=============================================")
    print(" " + title)
    print("=============================================")


def bench(fn, repeat=3):
    """Runs fn() `repeat` times, returns (best milliseconds, last result).
    min-of-N filters out scheduler noise: the fastest run is the closest
    to the true cost of the code."""
    best = float("inf")
    result = None
    for _ in range(repeat):
        t0 = perf_counter()
        result = fn()
        best = min(best, perf_counter() - t0)
    return best * 1000.0, result


def write_wav(path, signal, sr=SR):
    """float array in [-1, 1] -> 16-bit mono WAV. Clipping is deliberate:
    silently wrapping (what plain int casting does) sounds like crackle."""
    pcm = (np.clip(signal, -1.0, 1.0) * 32767.0).astype(np.int16)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(pcm.tobytes())


# ===========================================================================
# CHAPTER 1 — DIGITAL AUDIO IN PURE PYTHON
# ===========================================================================
# Sound is a pressure wave; digital audio samples it SR times per second.
# Nyquist: SR/2 is the highest representable frequency (44100 covers human
# hearing's ~20 kHz). This chapter does everything in pure Python — slow,
# but it shows there is no magic: audio is just a list of numbers.

def demo_digital_audio():
    duration = 0.25
    freq = 440.0                       # A4
    n = int(SR * duration)

    # A sine wave, one sample at a time: s[t] = sin(2*pi*f*t/SR).
    t0 = perf_counter()
    samples = [math.sin(2.0 * math.pi * freq * i / SR) for i in range(n)]
    gen_ms = (perf_counter() - t0) * 1000.0

    print(f"sample rate {SR} Hz -> Nyquist limit {SR // 2} Hz")
    print(f"{duration}s of a {freq:.0f} Hz sine = {n} samples "
          f"(generated in {gen_ms:.1f} ms, pure Python)")

    # Loudness: RMS in dBFS (decibels relative to full scale).
    rms = math.sqrt(sum(s * s for s in samples) / n)
    print(f"RMS = {rms:.3f} -> {20.0 * math.log10(rms):.1f} dBFS "
          f"(a full-scale sine is -3.0)")

    # Write a WAV by hand with struct.pack — the painful way, once,
    # so the numpy one-liner used later is appreciated.
    os.makedirs(OUT_DIR, exist_ok=True)
    path = os.path.join(OUT_DIR, "tone.wav")
    t0 = perf_counter()
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        frames = b"".join(
            struct.pack("<h", int(max(-1.0, min(1.0, s)) * 32767))
            for s in samples
        )
        w.writeframes(frames)
    print(f"wrote {path} sample-by-sample in "
          f"{(perf_counter() - t0) * 1000.0:.1f} ms — listen to it!")


# ===========================================================================
# CHAPTER 2 — THE COST OF PURE PYTHON
# ===========================================================================
# Every Python bytecode op costs ~10-100 ns of interpreter overhead: type
# checks, refcounts, boxing. Multiplying a million floats does a million
# times ALL of that. Measure it — performance work starts with numbers,
# not opinions.

def demo_interpreter_tax():
    n = 1_000_000
    signal = [math.sin(0.001 * i) for i in range(n)]

    def gain_loop():
        out = [0.0] * n
        for i in range(n):
            out[i] = signal[i] * 0.5
        return out

    def gain_comprehension():
        return [s * 0.5 for s in signal]

    loop_ms, _ = bench(gain_loop)
    comp_ms, _ = bench(gain_comprehension)
    sum_ms, total = bench(lambda: sum(signal))

    print(f"gain (x0.5) over {n:,} samples, pure Python:")
    print(f"  explicit for loop     : {loop_ms:7.1f} ms")
    print(f"  list comprehension    : {comp_ms:7.1f} ms  (less bytecode per item)")
    print(f"  sum() of the signal   : {sum_ms:7.1f} ms  (C loop over boxed floats)")
    print(f"  (sum = {total:.3f}; numbers vary per run — the SHAPE doesn't)")
    print("every sample pays interpreter overhead; the fix is to pay it")
    print("ONCE per array instead of once per element -> Chapter 3")


# ===========================================================================
# CHAPTER 3 — VECTORIZATION WITH NUMPY
# ===========================================================================
# numpy stores numbers unboxed in one contiguous C array; an expression
# like x * 0.5 runs ONE C loop (often SIMD-vectorized by the compiler).
# Same work as Chapter 2, typically 50-100x faster.

def demo_vectorization():
    n = 1_000_000
    signal_list = [math.sin(0.001 * i) for i in range(n)]
    signal = np.array(signal_list)          # one-time conversion cost
    other = np.roll(signal, 1)

    loop_ms, _ = bench(lambda: [s * 0.5 for s in signal_list])
    np_gain_ms, _ = bench(lambda: signal * 0.5)
    np_mix_ms, _ = bench(lambda: 0.5 * signal + 0.5 * other)
    np_rms_ms, rms = bench(lambda: float(np.sqrt(np.mean(signal * signal))))

    print(f"gain over {n:,} samples:")
    print(f"  pure Python           : {loop_ms:7.1f} ms")
    print(f"  numpy signal * 0.5    : {np_gain_ms:7.2f} ms   "
          f"(~{loop_ms / max(np_gain_ms, 1e-9):.0f}x faster)")
    print(f"  numpy mix of 2 tracks : {np_mix_ms:7.2f} ms")
    print(f"  numpy RMS             : {np_rms_ms:7.2f} ms   (rms={rms:.3f})")
    print("rule: if you wrote `for sample in signal` in Python, you are")
    print("leaving a ~100x speedup on the table")


# ===========================================================================
# CHAPTER 4 — DTYPES & MEMORY
# ===========================================================================
# Performance is often BANDWIDTH: how many bytes move through the cache.
# float64 is numpy's default; audio pipelines run float32 (half the bytes,
# plenty of precision) and store int16 on disk. Half the bytes ~= half the
# time for memory-bound ops.

def demo_dtypes():
    n = 5_000_000
    rng = np.random.default_rng(42)
    a64 = rng.standard_normal(n)                # float64
    a32 = a64.astype(np.float32)

    print(f"{n:,} samples as float64: {a64.nbytes / 1e6:6.1f} MB")
    print(f"{n:,} samples as float32: {a32.nbytes / 1e6:6.1f} MB")
    print(f"{n:,} samples as int16  : {n * 2 / 1e6:6.1f} MB (storage/WAV form)")

    ms64, _ = bench(lambda: a64 * 0.5 + 0.1)
    ms32, _ = bench(lambda: a32 * np.float32(0.5) + np.float32(0.1))
    print(f"gain+offset on float64  : {ms64:6.2f} ms")
    print(f"gain+offset on float32  : {ms32:6.2f} ms  "
          f"(fewer bytes -> less time; ratio varies)")

    # int16 conversion: scale, CLIP (never wrap!), cast.
    loud = a32 * np.float32(2.0)                # deliberately clips
    pcm = (np.clip(loud, -1.0, 1.0) * 32767.0).astype(np.int16)
    clipped = int(np.count_nonzero((loud > 1.0) | (loud < -1.0)))
    print(f"int16 conversion clipped {clipped:,} of {n:,} samples "
          f"({100.0 * clipped / n:.1f}%) — clip beats integer wraparound")
    print(f"pcm dtype={pcm.dtype}, min={pcm.min()}, max={pcm.max()}")
    print("note the float32 scalars in the code: a float64 constant would")
    print("silently promote the whole array back to float64")


# ===========================================================================
# CHAPTER 5 — VIEWS, COPIES, AND IN-PLACE OPS
# ===========================================================================
# numpy slicing returns VIEWS (no data copied — just new strides over the
# same buffer). Arithmetic allocates NEW arrays unless told otherwise.
# In a per-block audio callback, hidden allocations mean latency spikes.

def demo_views_and_copies():
    a = np.arange(8)
    view = a[::2]                 # every 2nd sample — a view, not a copy
    view[0] = 99                  # writes through to a!
    print(f"a[::2] is a view: after view[0]=99, a = {a.tolist()}")
    print(f"  (view.strides={view.strides}: step 16 bytes over a's buffer)")

    safe = a[1::2].copy()         # explicit copy when independence is wanted
    safe[0] = -1
    print(f"a[1::2].copy() is independent: a = {a.tolist()}")

    n = 5_000_000
    rng = np.random.default_rng(1)
    big = rng.standard_normal(n)

    alloc_ms, _ = bench(lambda: big * 0.5)              # allocates 40 MB
    def inplace():
        big_local = big
        big_local *= 1.0000001                           # writes in place
        return big_local
    inplace_ms, _ = bench(inplace)
    out_buf = np.empty_like(big)
    out_ms, _ = bench(lambda: np.multiply(big, 0.5, out=out_buf))

    print(f"gain on {n:,} samples:")
    print(f"  b = a * 0.5 (allocates 40 MB) : {alloc_ms:6.2f} ms")
    print(f"  a *= g      (in place)        : {inplace_ms:6.2f} ms")
    print(f"  multiply(..., out=buf)        : {out_ms:6.2f} ms "
          f"(reuse ONE scratch buffer per block)")
    print("real-time rule: allocate buffers once at startup, never in the")
    print("audio callback")


# ===========================================================================
# CHAPTER 6 — A SYNTHESIS TOOLBOX
# ===========================================================================
# Everything vectorized from here on. Oscillators are closed-form array
# expressions; envelopes are concatenated ramps; mixing is addition.

def sine_osc(freq, duration, sr=SR, amp=1.0):
    t = np.arange(int(sr * duration)) / sr
    return amp * np.sin(2.0 * np.pi * freq * t)


def square_osc(freq, duration, sr=SR, amp=1.0):
    return amp * np.sign(sine_osc(freq, duration, sr))   # sign of a sine


def saw_osc(freq, duration, sr=SR, amp=1.0):
    t = np.arange(int(sr * duration)) / sr
    return amp * (2.0 * ((t * freq) % 1.0) - 1.0)        # rising ramp, resets


def adsr(n, sr=SR, attack=0.01, decay=0.05, sustain=0.7, release=0.08):
    """Attack-Decay-Sustain-Release envelope as four concatenated ramps."""
    a = int(sr * attack)
    d = int(sr * decay)
    r = int(sr * release)
    s = max(n - a - d - r, 0)
    return np.concatenate([
        np.linspace(0.0, 1.0, a, endpoint=False),
        np.linspace(1.0, sustain, d, endpoint=False),
        np.full(s, sustain),
        np.linspace(sustain, 0.0, r),
    ])[:n]


A4, CS5, E5, A5 = 440.0, 554.37, 659.25, 880.0


def make_chord(duration=2.0):
    """An A major triad: mixing is just adding arrays (then scaling)."""
    return 0.3 * (sine_osc(A4, duration)
                  + sine_osc(CS5, duration)
                  + sine_osc(E5, duration))


def make_arpeggio():
    """Four enveloped notes, concatenated — a tiny sequencer."""
    notes = []
    for freq in (A4, CS5, E5, A5):
        tone = sine_osc(freq, 0.3, amp=0.8)
        notes.append(tone * adsr(len(tone)))
    return np.concatenate(notes)


def demo_synthesis():
    ms, chord = bench(lambda: make_chord(2.0))
    print(f"synthesized a 2.0s A-major chord ({len(chord):,} samples) "
          f"in {ms:.2f} ms")

    arp = make_arpeggio()
    write_wav(os.path.join(OUT_DIR, "chord.wav"), chord)
    write_wav(os.path.join(OUT_DIR, "arpeggio.wav"), arp)
    print("square/saw oscillators are one expression each: sign(sine),")
    print("2*(t*f % 1)-1 — no per-sample loop anywhere")
    print(f"wrote {OUT_DIR}/chord.wav and {OUT_DIR}/arpeggio.wav")


# ===========================================================================
# CHAPTER 7 — FILTERS: ALGORITHMS vs CONSTANTS
# ===========================================================================
# A moving average is the simplest low-pass filter. Three implementations
# teach the two dimensions of speed — complexity AND constant factors:
#   1. pure Python running sum      O(n)   but interpreter constants
#   2. np.convolve                  O(n*k) but C constants
#   3. numpy cumulative-sum trick   O(n)   and C constants
# ...and then the exception: an IIR filter's feedback loop is inherently
# sequential — the one shape numpy cannot vectorize.

def demo_filters():
    n, k = 200_000, 101
    rng = np.random.default_rng(7)
    x = rng.standard_normal(n)
    x_list = x.tolist()

    def python_running_sum():
        out = []
        acc = sum(x_list[:k])
        out.append(acc / k)
        for i in range(k, n):
            acc += x_list[i] - x_list[i - k]     # slide the window in O(1)
            out.append(acc / k)
        return out

    py_ms, py_out = bench(python_running_sum, repeat=1)
    conv_ms, conv_out = bench(lambda: np.convolve(x, np.ones(k) / k, mode="valid"))

    def cumsum_trick():
        c = np.concatenate(([0.0], np.cumsum(x)))
        return (c[k:] - c[:-k]) / k

    cum_ms, cum_out = bench(cumsum_trick)

    agree = np.allclose(py_out, conv_out) and np.allclose(conv_out, cum_out)
    print(f"moving average, n={n:,}, window k={k} (all agree: {agree}):")
    print(f"  pure Python O(n) running sum : {py_ms:7.1f} ms")
    print(f"  np.convolve O(n*k) in C      : {conv_ms:7.2f} ms  "
          f"(worse big-O, better constants!)")
    print(f"  numpy cumsum trick O(n)      : {cum_ms:7.2f} ms  (both wins)")

    # The sequential exception: y[i] depends on y[i-1].
    alpha = 0.01

    def one_pole():
        y = 0.0
        out = [0.0] * n
        for i in range(n):
            y += alpha * (x_list[i] - y)
            out[i] = y
        return out

    iir_ms, _ = bench(one_pole, repeat=1)
    print(f"  one-pole IIR (pure Python)   : {iir_ms:7.1f} ms")
    print("the IIR's feedback makes each sample depend on the previous —")
    print("numpy can't help; the escape hatches are numba/Cython/C (see docs)")


# ===========================================================================
# CHAPTER 8 — THE FFT: THE ALGORITHMIC WIN
# ===========================================================================
# Chapters 2-5 bought constant factors. The FFT buys a different EXPONENT:
# the naive DFT is O(n^2); the FFT is O(n log n). At n=4096 that is ~340x
# less work; at a million samples it is the difference between usable and
# impossible. Algorithm beats micro-optimization — always check the
# algorithm first.

def naive_dft(x):
    """Textbook O(n^2) discrete Fourier transform, pure Python."""
    n = len(x)
    out = []
    for k in range(n):
        re = im = 0.0
        for i in range(n):
            angle = -2.0 * math.pi * k * i / n
            re += x[i] * math.cos(angle)
            im += x[i] * math.sin(angle)
        out.append(complex(re, im))
    return out


def demo_fft():
    n = 1024
    chord = make_chord(0.1)[:n]
    chord_list = chord.tolist()

    dft_ms, dft_out = bench(lambda: naive_dft(chord_list), repeat=1)
    fft_ms, fft_out = bench(lambda: np.fft.fft(chord))

    match = np.allclose(np.array(dft_out), fft_out, atol=1e-6)
    print(f"n={n}: naive O(n^2) DFT {dft_ms:8.1f} ms, "
          f"np.fft O(n log n) {fft_ms:.3f} ms "
          f"(~{dft_ms / max(fft_ms, 1e-9):.0f}x), results match: {match}")
    print("doubling n doubles FFT time but QUADRUPLES the naive DFT's —")
    print("the gap grows without bound; this one algorithm made real-time")
    print("spectral audio processing possible")

    # Use it: find which notes are in the chord.
    big_n = 32768
    window = make_chord(1.0)[:big_n]
    spectrum = np.abs(np.fft.rfft(window))
    freqs = np.fft.rfftfreq(big_n, 1.0 / SR)

    found = []
    mags = spectrum.copy()
    for _ in range(3):
        peak = int(np.argmax(mags))
        found.append(float(freqs[peak]))
        mags[max(0, peak - 5):peak + 6] = 0.0   # suppress this peak's leakage
    print(f"3 strongest frequencies in the chord: "
          f"{[round(f, 1) for f in sorted(found)]} Hz")
    print(f"(true notes: A4={A4}, C#5={CS5}, E5={E5} — bin width "
          f"{SR / big_n:.2f} Hz explains the rounding)")


# ===========================================================================
# CHAPTER 9 — SPECTRAL DENOISING (STFT + OVERLAP-ADD)
# ===========================================================================
# The Short-Time Fourier Transform: slice the signal into overlapping
# windowed frames, FFT each, process the spectrum, inverse-FFT, and
# overlap-add back together. Here: spectral gating — zero every bin below
# a noise threshold — the core idea of every noise-removal plugin.
# sliding_window_view frames the signal WITHOUT copying (Chapter 5's views
# doing real work).

def demo_spectral_denoise():
    frame = 1024
    hop = frame // 2
    # Periodic Hann window: shifted copies at 50% overlap sum EXACTLY to 1,
    # so overlap-add reconstructs perfectly (the COLA property).
    window = np.hanning(frame + 1)[:-1]

    clean = make_chord(2.0)
    rng = np.random.default_rng(42)
    noisy = clean + rng.normal(0.0, 0.05, len(clean))

    def snr_db(reference, x):
        err = reference - x
        return 10.0 * np.log10(np.sum(reference**2) / np.sum(err**2))

    t0 = perf_counter()

    frames = sliding_window_view(noisy, frame)[::hop] * window   # zero-copy view
    spectra = np.fft.rfft(frames, axis=1)                         # all frames at once

    mags = np.abs(spectra)
    threshold = 4.0 * np.median(mags)      # median = noise floor (peaks are sparse)
    spectra[mags < threshold] = 0.0        # the gate: boolean-mask indexing

    rebuilt = np.fft.irfft(spectra, axis=1)
    denoised = np.zeros(len(noisy))
    for i, start in enumerate(range(0, len(noisy) - frame + 1, hop)):
        denoised[start:start + frame] += rebuilt[i]               # overlap-add

    elapsed_ms = (perf_counter() - t0) * 1000.0

    lo, hi = frame, len(clean) - frame       # skip edges without full overlap
    before = snr_db(clean[lo:hi], noisy[lo:hi])
    after = snr_db(clean[lo:hi], denoised[lo:hi])
    print(f"STFT: {len(frames)} frames of {frame} samples, hop {hop}, "
          f"processed in {elapsed_ms:.1f} ms")
    print(f"SNR before gate: {before:5.1f} dB")
    print(f"SNR after gate : {after:5.1f} dB   "
          f"(+{after - before:.1f} dB — noise between the notes is gone)")

    write_wav(os.path.join(OUT_DIR, "noisy.wav"), noisy)
    write_wav(os.path.join(OUT_DIR, "denoised.wav"), denoised)
    print(f"wrote {OUT_DIR}/noisy.wav and {OUT_DIR}/denoised.wav — "
          f"hear the difference")


# ===========================================================================
# CHAPTER 10 — AN EFFECTS CHAIN
# ===========================================================================
# Three classic effects, each a couple of vectorized lines:
#   echo       : add a delayed, attenuated copy (index shifting, no loop)
#   distortion : waveshaping through tanh (any nonlinearity works)
#   tremolo    : multiply by a low-frequency oscillator (amplitude LFO)

def echo(x, delay_s=0.15, feedback=0.4):
    d = int(SR * delay_s)
    y = x.copy()
    y[d:] += feedback * x[:-d]        # the delayed copy, shifted by slicing
    return y


def distortion(x, drive=3.0):
    return np.tanh(drive * x) / math.tanh(drive)   # soft clip, normalized


def tremolo(x, rate_hz=5.0, depth=0.5):
    t = np.arange(len(x)) / SR
    lfo = 1.0 - depth / 2.0 + (depth / 2.0) * np.sin(2.0 * np.pi * rate_hz * t)
    return x * lfo


def demo_effects_chain():
    dry = make_arpeggio()

    ms, wet = bench(lambda: tremolo(distortion(echo(dry))))
    seconds = len(dry) / SR
    print(f"echo -> distortion -> tremolo on {seconds:.1f}s of audio "
          f"in {ms:.2f} ms")
    print(f"that is {seconds * 1000.0 / ms:.0f}x faster than real time — "
          f"headroom for dozens of tracks")

    write_wav(os.path.join(OUT_DIR, "effects.wav"), 0.8 * wet)
    print(f"wrote {OUT_DIR}/effects.wav (dry version is arpeggio.wav)")


# ===========================================================================
# CHAPTER 11 — STREAMING & THE REAL-TIME DEADLINE
# ===========================================================================
# Live audio arrives in BLOCKS (e.g. 512 samples = 11.6 ms at 44.1 kHz).
# The iron rule: each block must be processed in less than the time it
# takes to PLAY, every single time, or the output glitches. Effects with
# memory (delays, filters) must carry state across block boundaries —
# here the echo keeps its delay tail between blocks.

class StreamingEcho:
    """Echo that works block-by-block: the last `delay` input samples are
    carried across calls, exactly like a real-time plugin's internal state.
    All buffers are allocated ONCE (Chapter 5's rule)."""

    def __init__(self, delay_samples, feedback):
        self.tail = np.zeros(delay_samples)      # last inputs from the past
        self.feedback = feedback

    def process(self, block):
        joined = np.concatenate((self.tail, block))
        delayed = joined[:len(block)]            # input from delay_samples ago
        out = block + self.feedback * delayed
        self.tail = joined[len(block):]          # save the new tail
        return out


def demo_streaming():
    block_size = 512
    budget_ms = block_size / SR * 1000.0
    signal = np.tile(make_arpeggio(), 4)         # ~4.8 s of input
    n_blocks = len(signal) // block_size

    fx = StreamingEcho(delay_samples=4410, feedback=0.4)
    out = np.empty_like(signal)

    worst_ms = 0.0
    t0 = perf_counter()
    for b in range(n_blocks):
        s = b * block_size
        tb = perf_counter()
        out[s:s + block_size] = fx.process(signal[s:s + block_size])
        worst_ms = max(worst_ms, (perf_counter() - tb) * 1000.0)
    total_ms = (perf_counter() - t0) * 1000.0

    audio_ms = n_blocks * budget_ms
    print(f"block size {block_size} -> deadline {budget_ms:.1f} ms per block")
    print(f"processed {n_blocks} blocks ({audio_ms / 1000.0:.1f}s of audio) "
          f"in {total_ms:.1f} ms total")
    print(f"worst single block: {worst_ms:.3f} ms "
          f"({100.0 * worst_ms / budget_ms:.1f}% of the deadline)")
    print(f"real-time factor: {audio_ms / total_ms:.0f}x")
    print("the WORST block is what matters live, not the average — one")
    print("missed deadline is an audible glitch; that is why real-time code")
    print("bans allocation, GC pressure and unbounded work in the callback")


# ===========================================================================
# CHAPTER 12 — PROFILING & THE OPTIMIZATION PLAYBOOK
# ===========================================================================
# Never optimize blind: profile, find the ONE hotspot, fix it, measure
# again. cProfile shows where the time actually goes (it is rarely where
# you think). The playbook, in order:
#   1. measure   2. better algorithm   3. vectorize
#   4. fix dtypes/copies   5. only then: numba / Cython / C

def demo_profiling():
    signal = np.tile(make_arpeggio(), 2)
    x_list = signal.tolist()
    frame, hop = 1024, 512

    def rms_map_slow(samples):
        """Windowed RMS 'loudness curve' — the pure-Python way."""
        out = []
        for start in range(0, len(samples) - frame + 1, hop):
            acc = 0.0
            for i in range(start, start + frame):     # the hotspot
                acc += samples[i] * samples[i]
            out.append(math.sqrt(acc / frame))
        return out

    profiler = cProfile.Profile()
    profiler.enable()
    slow_result = rms_map_slow(x_list)
    profiler.disable()

    stats_text = io.StringIO()
    pstats.Stats(profiler, stream=stats_text).sort_stats("cumulative").print_stats(3)
    interesting = [ln for ln in stats_text.getvalue().splitlines()
                   if "rms_map_slow" in ln or "function calls" in ln]
    print("cProfile of the pure-Python loudness curve:")
    for line in interesting:
        print("  " + line.strip())

    slow_ms, _ = bench(lambda: rms_map_slow(x_list), repeat=1)

    def rms_map_fast(samples):
        windows = sliding_window_view(samples, frame)[::hop]   # views, no copy
        return np.sqrt(np.mean(windows * windows, axis=1))

    fast_ms, fast_result = bench(lambda: rms_map_fast(signal))

    match = np.allclose(slow_result, fast_result)
    print(f"pure Python : {slow_ms:7.1f} ms")
    print(f"vectorized  : {fast_ms:7.2f} ms  "
          f"(~{slow_ms / max(fast_ms, 1e-9):.0f}x, results match: {match})")
    print("\nthe playbook: 1 measure  2 algorithm (Ch8)  3 vectorize (Ch3)")
    print("4 dtypes & copies (Ch4-5)  5 native code — numba/Cython/C last")


# ===========================================================================
# MAIN
# ===========================================================================

def main():
    print("high performance Python for audio processing")
    print("(timings vary per machine; the ratios are the lesson)")

    chapter("1. Digital audio in pure Python")
    demo_digital_audio()

    chapter("2. The cost of pure Python")
    demo_interpreter_tax()

    chapter("3. Vectorization with NumPy")
    demo_vectorization()

    chapter("4. dtypes & memory")
    demo_dtypes()

    chapter("5. Views, copies, in-place")
    demo_views_and_copies()

    chapter("6. A synthesis toolbox")
    demo_synthesis()

    chapter("7. Filters: algorithms vs constants")
    demo_filters()

    chapter("8. The FFT: the algorithmic win")
    demo_fft()

    chapter("9. Spectral denoising (STFT)")
    demo_spectral_denoise()

    chapter("10. An effects chain")
    demo_effects_chain()

    chapter("11. Streaming & the real-time deadline")
    demo_streaming()

    chapter("12. Profiling & the playbook")
    demo_profiling()

    print("\nAll chapters completed successfully.")
    print(f"listen to the results: {OUT_DIR}/*.wav")


if __name__ == "__main__":
    main()
