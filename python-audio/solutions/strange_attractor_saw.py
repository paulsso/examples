#!/usr/bin/env python3
# ============================================================================
#  strange_attractor_saw.py — chapters 1–3 foundations solution
# ============================================================================
#
#  A Lorenz strange attractor (x, y, z) is the tuning CV of three sawtooth
#  oscillators parked on an A# minor triad. Chapter 1 builds a short stereo
#  WAV in pure Python; Chapter 2 measures the interpreter tax; Chapter 3
#  vectorizes the saws and reports the NumPy break-even n.
#
#  RK4 stays in Python (sequential). The oscillators are the thing Chapter 3
#  can vectorize.
#
#  Run from python-audio/:  python3 solutions/strange_attractor_saw.py
#  Writes out/asharp_minor_py.wav and out/asharp_minor.wav
# ============================================================================

import math
import os
import struct
import wave
from time import perf_counter

import numpy as np

SR = 44100
_HERE = os.path.dirname(os.path.abspath(__file__))
_COURSE = os.path.dirname(_HERE)
OUT_DIR = os.path.join(_COURSE, "out")

# Equal temperament, A4 = 440 Hz. A# minor: A#4, C#5, F5 (the fifth is E#).
NOTE_ASH4 = 440.0 * 2.0 ** (1.0 / 12.0)
NOTE_CS5 = 440.0 * 2.0 ** (4.0 / 12.0)
NOTE_F5 = 440.0 * 2.0 ** (8.0 / 12.0)

LORENZ_SIG = 10.0
LORENZ_RHO = 28.0
LORENZ_BET = 8.0 / 3.0
# Typical extents of the classic attractor, used to map state -> ~[-1, 1].
LORENZ_XSCALE = 20.0
LORENZ_YSCALE = 20.0
LORENZ_ZCENTER = 25.0
LORENZ_ZSCALE = 20.0
DETUNE_OCTAVES = 0.35          # wandering depth; triad stays recognizable
CTRL_HZ = 2000                 # Lorenz control rate (not audio rate)
LORENZ_SPEED = 2.0             # Lorenz time units per audio second


def chapter(title):
    print("\n=============================================")
    print(" " + title)
    print("=============================================")


def bench(fn, repeat=3):
    """Runs fn() `repeat` times, returns (best milliseconds, last result).
    min-of-N filters scheduler noise: the fastest run is closest to the
    true cost of the code."""
    best = float("inf")
    result = None
    for _ in range(repeat):
        t0 = perf_counter()
        result = fn()
        best = min(best, perf_counter() - t0)
    return best * 1000.0, result


def write_wav_stereo(path, left, right, sr=SR):
    """Two float channels in [-1, 1] -> 16-bit interleaved stereo WAV."""
    L = np.clip(np.asarray(left, dtype=np.float64), -1.0, 1.0)
    R = np.clip(np.asarray(right, dtype=np.float64), -1.0, 1.0)
    interleaved = np.empty(L.size * 2, dtype=np.float64)
    interleaved[0::2] = L
    interleaved[1::2] = R
    pcm = (interleaved * 32767.0).astype(np.int16)
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(pcm.tobytes())


def lorenz_deriv(x, y, z):
    dx = LORENZ_SIG * (y - x)
    dy = x * (LORENZ_RHO - z) - y
    dz = x * y - LORENZ_BET * z
    return dx, dy, dz


def lorenz_rk4_step(x, y, z, dt):
    """One RK4 step of Lorenz 1963 — same scheme as computational-physics."""
    k1x, k1y, k1z = lorenz_deriv(x, y, z)
    k2x, k2y, k2z = lorenz_deriv(
        x + 0.5 * dt * k1x, y + 0.5 * dt * k1y, z + 0.5 * dt * k1z)
    k3x, k3y, k3z = lorenz_deriv(
        x + 0.5 * dt * k2x, y + 0.5 * dt * k2y, z + 0.5 * dt * k2z)
    k4x, k4y, k4z = lorenz_deriv(
        x + dt * k3x, y + dt * k3y, z + dt * k3z)
    x += dt / 6.0 * (k1x + 2.0 * k2x + 2.0 * k3x + k4x)
    y += dt / 6.0 * (k1y + 2.0 * k2y + 2.0 * k3y + k4y)
    z += dt / 6.0 * (k1z + 2.0 * k2z + 2.0 * k3z + k4z)
    return x, y, z


def lorenz_trajectory(n, dt, x0=1.0, y0=1.0, z0=1.0):
    """n RK4 samples of (x, y, z). Sequential — NumPy cannot vectorize this."""
    xs = [0.0] * n
    ys = [0.0] * n
    zs = [0.0] * n
    x, y, z = x0, y0, z0
    for i in range(n):
        xs[i] = x
        ys[i] = y
        zs[i] = z
        x, y, z = lorenz_rk4_step(x, y, z, dt)
    return xs, ys, zs


def state_to_freq(x, y, z, depth=DETUNE_OCTAVES):
    """Map one Lorenz state to the three A# minor saw frequencies (Hz)."""
    ux = max(-1.0, min(1.0, x / LORENZ_XSCALE))
    uy = max(-1.0, min(1.0, y / LORENZ_YSCALE))
    uz = max(-1.0, min(1.0, (z - LORENZ_ZCENTER) / LORENZ_ZSCALE))
    fx = NOTE_ASH4 * (2.0 ** (depth * ux))
    fy = NOTE_CS5 * (2.0 ** (depth * uy))
    fz = NOTE_F5 * (2.0 ** (depth * uz))
    return fx, fy, fz


def freqs_from_states(xs, ys, zs, depth=DETUNE_OCTAVES):
    n = len(xs)
    fx, fy, fz = [0.0] * n, [0.0] * n, [0.0] * n
    for i in range(n):
        fx[i], fy[i], fz[i] = state_to_freq(xs[i], ys[i], zs[i], depth)
    return fx, fy, fz


def freqs_from_states_np(xs, ys, zs, depth=DETUNE_OCTAVES):
    ux = np.clip(np.asarray(xs, dtype=np.float64) / LORENZ_XSCALE, -1.0, 1.0)
    uy = np.clip(np.asarray(ys, dtype=np.float64) / LORENZ_YSCALE, -1.0, 1.0)
    uz = np.clip(
        (np.asarray(zs, dtype=np.float64) - LORENZ_ZCENTER) / LORENZ_ZSCALE,
        -1.0, 1.0)
    return (NOTE_ASH4 * (2.0 ** (depth * ux)),
            NOTE_CS5 * (2.0 ** (depth * uy)),
            NOTE_F5 * (2.0 ** (depth * uz)))


def lerp_series(ctrl, n_out):
    """Linear resample of a control-rate list up to n_out audio samples."""
    n_in = len(ctrl)
    if n_in == 1:
        return [ctrl[0]] * n_out
    out = [0.0] * n_out
    scale = (n_in - 1) / max(n_out - 1, 1)
    for i in range(n_out):
        pos = i * scale
        j = int(pos)
        if j >= n_in - 1:
            out[i] = ctrl[-1]
        else:
            frac = pos - j
            out[i] = ctrl[j] * (1.0 - frac) + ctrl[j + 1] * frac
    return out


def saw_python(freqs, sr=SR):
    """Phase-accumulator sawtooth: 2*((phase + f/sr) % 1) - 1, one sample
    at a time. The same identity Chapter 6 writes as 2*((t*f) % 1) - 1."""
    n = len(freqs)
    out = [0.0] * n
    phase = 0.0
    inv_sr = 1.0 / sr
    for i in range(n):
        phase = (phase + freqs[i] * inv_sr) % 1.0
        out[i] = 2.0 * phase - 1.0
    return out


def saw_numpy(freq, sr=SR):
    """Vectorized phase-accumulator saw: one C loop over the frequency CV."""
    return 2.0 * (np.cumsum(np.asarray(freq, dtype=np.float64) / sr) % 1.0) - 1.0


def pan_asharp_minor(saw_x, saw_y, saw_z):
    """Left = A# + 1/2 F, right = C# + 1/2 F — the two Lorenz wings spatialize."""
    gain = 0.45
    left = [gain * (sx + 0.5 * sz) for sx, sz in zip(saw_x, saw_z)]
    right = [gain * (sy + 0.5 * sz) for sy, sz in zip(saw_y, saw_z)]
    return left, right


def pan_asharp_minor_np(saw_x, saw_y, saw_z):
    gain = 0.45
    left = gain * (saw_x + 0.5 * saw_z)
    right = gain * (saw_y + 0.5 * saw_z)
    return left, right


def demo_attractor_foundations():
    """Solve chapters 1–3: stereo A# minor chord whose pitches are a
    Lorenz attractor, measured in Python then vectorized."""
    os.makedirs(OUT_DIR, exist_ok=True)
    print("A# minor (equal temperament, A4=440):")
    print(f"  A#4 = {NOTE_ASH4:.2f} Hz  (root,    Lorenz x)")
    print(f"  C#5 = {NOTE_CS5:.2f} Hz  (third,   Lorenz y)")
    print(f"  F5  = {NOTE_F5:.2f} Hz  (fifth E#, Lorenz z)")
    print("Lorenz: dx/dt=σ(y-x), dy/dt=x(ρ-z)-y, dz/dt=xy-βz  "
          f"(σ={LORENZ_SIG:g}, ρ={LORENZ_RHO:g}, β=8/3)")
    print(f"detune ±{DETUNE_OCTAVES:.2f} octaves; "
          f"{LORENZ_SPEED:g} Lorenz-seconds per audio second")

    # ----- Chapter 1: short stereo clip, packed by hand -----
    duration_py = 0.75
    n_audio_py = int(SR * duration_py)
    n_ctrl_py = max(int(CTRL_HZ * duration_py), 2)
    dt = LORENZ_SPEED / CTRL_HZ

    t0 = perf_counter()
    xs, ys, zs = lorenz_trajectory(n_ctrl_py, dt)
    fx, fy, fz = freqs_from_states(xs, ys, zs)
    fx_a = lerp_series(fx, n_audio_py)
    fy_a = lerp_series(fy, n_audio_py)
    fz_a = lerp_series(fz, n_audio_py)
    saw_x = saw_python(fx_a)
    saw_y = saw_python(fy_a)
    saw_z = saw_python(fz_a)
    left, right = pan_asharp_minor(saw_x, saw_y, saw_z)
    gen_ms = (perf_counter() - t0) * 1000.0

    mid = [(l + r) * 0.5 for l, r in zip(left, right)]
    rms = math.sqrt(sum(s * s for s in mid) / n_audio_py)
    print(f"\nCh 1  {duration_py}s stereo A#m attractor, pure Python "
          f"({n_audio_py} frames) in {gen_ms:.1f} ms")
    print(f"  mid RMS = {rms:.3f} -> {20.0 * math.log10(rms):.1f} dBFS")

    path_py = os.path.join(OUT_DIR, "asharp_minor_py.wav")
    t0 = perf_counter()
    with wave.open(path_py, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        frames = b"".join(
            struct.pack(
                "<hh",
                int(max(-1.0, min(1.0, l)) * 32767),
                int(max(-1.0, min(1.0, r)) * 32767),
            )
            for l, r in zip(left, right)
        )
        w.writeframes(frames)
    pack_ms = (perf_counter() - t0) * 1000.0
    print(f"  wrote {path_py} interleaved <hh> in {pack_ms:.1f} ms")

    # Python vs NumPy saws agree on this clip (cumsum is the vectorized acc).
    match = np.allclose(saw_x, saw_numpy(fx_a), atol=1e-10)
    print(f"  python saw vs numpy cumsum: match={match}")

    # ----- Chapter 2: measure the interpreter tax on this pipeline -----
    duration_b = 1.0
    n_audio_b = int(SR * duration_b)
    n_ctrl_b = max(int(CTRL_HZ * duration_b), 2)

    def run_rk4():
        return lorenz_trajectory(n_ctrl_b, dt)

    rk4_ms, traj = bench(run_rk4)
    xs_b, ys_b, zs_b = traj
    fx_b, fy_b, fz_b = freqs_from_states(xs_b, ys_b, zs_b)
    fx_ba = lerp_series(fx_b, n_audio_b)
    fy_ba = lerp_series(fy_b, n_audio_b)
    fz_ba = lerp_series(fz_b, n_audio_b)

    def run_saws():
        return (saw_python(fx_ba), saw_python(fy_ba), saw_python(fz_ba))

    saw_ms, saws_b = bench(run_saws)
    mix_ms, _ = bench(lambda: pan_asharp_minor(*saws_b))

    print(f"\nCh 2  interpreter tax on {duration_b:.0f}s ({n_audio_b:,} samples):")
    print(f"  RK4 Lorenz ({n_ctrl_b} control steps) : {rk4_ms:7.1f} ms")
    print(f"  3 python saws                       : {saw_ms:7.1f} ms")
    print(f"  stereo pan/mix                      : {mix_ms:7.1f} ms")
    print("  every sample (and every RK4 stage) pays bytecode; RK4 has to,")
    print("  because sample i needs sample i-1. The saws do not.")

    # ----- Chapter 3: vectorize the oscillators, 4 s render, break-even -----
    duration_np = 4.0
    n_audio_np = int(SR * duration_np)
    n_ctrl_np = max(int(CTRL_HZ * duration_np), 2)
    xs_n, ys_n, zs_n = lorenz_trajectory(n_ctrl_np, dt)
    fx_c, fy_c, fz_c = freqs_from_states_np(xs_n, ys_n, zs_n)
    t_ctrl = np.linspace(0.0, duration_np, n_ctrl_np)
    t_audio = np.linspace(0.0, duration_np, n_audio_np)
    fx_n = np.interp(t_audio, t_ctrl, fx_c)
    fy_n = np.interp(t_audio, t_ctrl, fy_c)
    fz_n = np.interp(t_audio, t_ctrl, fz_c)

    def render_numpy():
        sx = saw_numpy(fx_n)
        sy = saw_numpy(fy_n)
        sz = saw_numpy(fz_n)
        return pan_asharp_minor_np(sx, sy, sz)

    np_ms, (left_n, right_n) = bench(render_numpy)
    fx_ba_np = np.asarray(fx_ba, dtype=np.float64)
    py_1s_ms, _ = bench(lambda: saw_python(fx_ba))
    np_1s_ms, _ = bench(lambda: saw_numpy(fx_ba_np))

    path_np = os.path.join(OUT_DIR, "asharp_minor.wav")
    write_wav_stereo(path_np, left_n, right_n)
    print(f"\nCh 3  {duration_np:.0f}s stereo render (vectorized saws) "
          f"in {np_ms:.2f} ms")
    print(f"  1s saw, python : {py_1s_ms:7.1f} ms")
    print(f"  1s saw, numpy  : {np_1s_ms:7.2f} ms  "
          f"(~{py_1s_ms / max(np_1s_ms, 1e-9):.0f}x)")
    print(f"  wrote {path_np} — listen to the chord wander")

    sizes = (1_000, 10_000, 100_000, 1_000_000)
    print("\n  break-even n (one saw, constant A#4 CV):")
    break_even = None
    for n in sizes:
        freq_list = [NOTE_ASH4] * n
        freq_arr = np.full(n, NOTE_ASH4)
        py_ms, _ = bench(lambda fl=freq_list: saw_python(fl))
        np_ms_n, _ = bench(lambda fa=freq_arr: saw_numpy(fa))
        winner = "numpy" if np_ms_n < py_ms else "python"
        print(f"    n={n:>10,}   python {py_ms:7.2f} ms   "
              f"numpy {np_ms_n:7.3f} ms   ({winner})")
        if break_even is None and np_ms_n < py_ms:
            break_even = n
    if break_even is None:
        print("  numpy did not beat python on these sizes")
    else:
        print(f"  break-even: NumPy wins from n={break_even:,}")


def main():
    print("chapters 1-3 solution: A# minor Lorenz attractor")
    print("(timings vary per machine; the ratios are the lesson)")
    chapter("1-3. Foundations project: A# minor Lorenz attractor")
    demo_attractor_foundations()
    print(f"\nlisten to the results: {OUT_DIR}/*.wav")


if __name__ == "__main__":
    main()
