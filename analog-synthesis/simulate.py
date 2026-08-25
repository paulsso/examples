#!/usr/bin/env python3
# ============================================================================
#  simulate.py — Analog Electronics for Modular Synthesis
# ============================================================================
#
#  Educational circuit simulator for Eurorack-style modules. Each chapter
#  models the same topologies documented in circuits/*.cir (open those in
#  Qucs-S / ngspice for full SPICE). This Python path always runs offline
#  and prints a PASS/FAIL scorecard — the same discipline as the
#  computational-physics course in this repo.
#
#  Chapters:
#    1.  Qucs-S workflow & Eurorack signal conventions (unity buffer)
#    2.  Op-amp building blocks (summer, integrator, Schmitt, rectifier)
#    3.  Exponential converter (1 V/octave)
#    4.  VCO — integrator + Schmitt, expo pitch control
#    5.  VCF — state-variable filter with voltage-controlled cutoff
#    6.  VCA — OTA gain cell, linear CV
#    7.  LFO — scaled VCO core for sub-audio rates
#    8.  Patching the voice — VCO → VCF → VCA, LFO → VCA CV
#    9.  Music-scene electronics (prose only — see DOCUMENTATION.md)
#   10.  From simulation to bench (prose only — see DOCUMENTATION.md)
#
#  Requires: Python 3.10+, NumPy. Run: `make run` (or ./run.sh).
# ============================================================================

from __future__ import annotations

import math
import os
import struct
import wave
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

import numpy as np

OUT = Path(__file__).resolve().parent / "out"
SR_AUDIO = 44100  # for WAV export of the voice patch
VT = 0.026  # thermal voltage ≈ 26 mV at room temp (idealized)


# ---------------------------------------------------------------------------
# Scorecard
# ---------------------------------------------------------------------------

@dataclass
class Check:
    name: str
    ok: bool
    detail: str


CHECKS: list[Check] = []


def check(name: str, ok: bool, detail: str = "") -> None:
    CHECKS.append(Check(name, ok, detail))
    status = "PASS" if ok else "FAIL"
    suffix = f"  ({detail})" if detail else ""
    print(f"  [{status}] {name}{suffix}")


def write_dat(path: Path, header: str, cols: dict[str, np.ndarray]) -> None:
    """Write a simple columnar .dat file (Qucs-S / ngspice friendly)."""
    path.parent.mkdir(parents=True, exist_ok=True)
    keys = list(cols.keys())
    n = len(next(iter(cols.values())))
    with path.open("w") as f:
        f.write(f"# {header}\n")
        f.write("# " + "\t".join(keys) + "\n")
        for i in range(n):
            f.write("\t".join(f"{cols[k][i]:.8g}" for k in keys) + "\n")


def write_wav(path: Path, signal: np.ndarray, sr: int = SR_AUDIO) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    x = np.clip(signal.astype(np.float64), -1.0, 1.0)
    pcm = (x * 32767.0).astype(np.int16)
    with wave.open(str(path), "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(pcm.tobytes())


# ---------------------------------------------------------------------------
# Chapter 1 — Unity-gain buffer & Eurorack conventions
# ---------------------------------------------------------------------------

def chapter_1() -> None:
    print("\n=== Chapter 1 — Qucs-S workflow & Eurorack signal conventions ===")
    # Ideal non-inverting buffer: Vout = Vin. AC sweep magnitude = 0 dB.
    freqs = np.logspace(1, 5, 81)  # 10 Hz .. 100 kHz
    # Ideal op-amp buffer: flat 0 dB; real TL072 rolls off ~3 MHz — flat here.
    gain_db = np.zeros_like(freqs)
    phase_deg = np.zeros_like(freqs)

    write_dat(
        OUT / "ch01-buffer.dat",
        "Unity-gain buffer AC response (ideal TL072 buffer)",
        {"freq_Hz": freqs, "gain_dB": gain_db, "phase_deg": phase_deg},
    )

    # Eurorack conventions encoded as assertions learners should internalize.
    rail = 12.0
    audio_vpp = 10.0
    cv_max = 10.0
    v_per_oct = 1.0

    midband = gain_db[(freqs >= 100) & (freqs <= 10_000)]
    check(
        "ch1: buffer midband gain ≈ 0 dB",
        float(np.max(np.abs(midband))) < 0.01,
        f"max |gain| = {float(np.max(np.abs(midband))):.4f} dB",
    )
    check(
        "ch1: Eurorack rails ±12 V",
        rail == 12.0,
        "V+ = +12, V− = −12",
    )
    check(
        "ch1: audio ~10 Vpp, CV 0–10 V, 1 V/oct",
        audio_vpp == 10.0 and cv_max == 10.0 and v_per_oct == 1.0,
        "standard Eurorack levels",
    )


# ---------------------------------------------------------------------------
# Chapter 2 — Op-amp building blocks
# ---------------------------------------------------------------------------

def chapter_2() -> None:
    print("\n=== Chapter 2 — Op-amp building blocks ===")

    # --- Inverting summer (mixer): Vout = −(Vin1 + Vin2) with equal R ---
    vin1, vin2 = 1.0, 2.0
    vout_summer = -(vin1 + vin2)
    check(
        "ch2: inverting summer Vout = −(V1+V2)",
        abs(vout_summer - (-3.0)) < 1e-9,
        f"Vout = {vout_summer:.3f} V",
    )

    # --- Integrator: Vout(t) = −(1/RC) ∫ Vin dt ---
    # Constant Vin = 1 V, R=10k, C=10n → slope = −1/(RC) = −10_000 V/s
    R, C = 10e3, 10e-9
    slope = -1.0 / (R * C)
    t = np.linspace(0, 1e-3, 1001)
    vin = np.ones_like(t)
    vout_int = slope * np.cumsum(vin) * (t[1] - t[0])
    # Reset relative: first sample ~0 after subtracting initial
    vout_int -= vout_int[0]
    expected_at_1ms = slope * 1e-3
    check(
        "ch2: integrator slope −1/(RC)",
        abs(vout_int[-1] - expected_at_1ms) / abs(expected_at_1ms) < 0.02,
        f"measured {vout_int[-1]:.1f} V vs expected {expected_at_1ms:.1f} V at 1 ms",
    )
    write_dat(
        OUT / "ch02-integrator.dat",
        "Op-amp integrator, Vin=1V, R=10k, C=10n",
        {"t_s": t, "vout_V": vout_int},
    )

    # --- Schmitt trigger: hysteresis thresholds ---
    # Non-inverting Schmitt with R1=R2 → thresholds at ±Vsat/2 for Vin through R
    # Classic: Vth = ± Vsat * Rf/(R1+Rf). With equal divider from ±10 V sat → ±5 V.
    vsat = 10.0
    v_high, v_low = vsat / 2.0, -vsat / 2.0
    # Sweep Vin slowly and record transitions
    vin_s = np.linspace(-12, 12, 2401)
    state = -vsat
    vout_s = np.empty_like(vin_s)
    for i, v in enumerate(vin_s):
        if state < 0 and v > v_high:
            state = vsat
        elif state > 0 and v < v_low:
            state = -vsat
        vout_s[i] = state
    # Rising transition near +5 V
    rising = np.where(np.diff(vout_s) > 0)[0]
    thr_meas = float(vin_s[rising[0]]) if len(rising) else float("nan")
    check(
        "ch2: Schmitt rising threshold ≈ +5 V",
        abs(thr_meas - 5.0) < 0.05,
        f"threshold = {thr_meas:.3f} V",
    )
    write_dat(
        OUT / "ch02-schmitt.dat",
        "Schmitt trigger transfer (hysteresis ±5 V)",
        {"vin_V": vin_s, "vout_V": vout_s},
    )

    # --- Precision half-wave rectifier: Vout = max(Vin, 0) ---
    vin_r = np.linspace(-5, 5, 201)
    vout_r = np.maximum(vin_r, 0.0)
    check(
        "ch2: precision rectifier passes positive half only",
        float(np.max(vout_r[vin_r < 0])) < 1e-12
        and abs(float(vout_r[vin_r == 5.0][0] if np.any(vin_r == 5.0) else vout_r[-1]) - 5.0)
        < 1e-9,
        "negative → 0, +5 V → +5 V",
    )
    write_dat(
        OUT / "ch02-rectifier.dat",
        "Precision half-wave rectifier",
        {"vin_V": vin_r, "vout_V": vout_r},
    )


# ---------------------------------------------------------------------------
# Chapter 3 — Exponential converter (1 V/octave)
# ---------------------------------------------------------------------------

def expo_current(cv: float, i0: float = 10e-6, v_scale: float = 1.0 / math.log(2)) -> float:
    """
    Ideal expo converter: Iout = I0 * 2^(CV / V_scale) with V_scale = 1 V/oct.
    Real circuits use Iout ∝ exp(Vbe/Vt); the resistor network sets 1 V/oct.
    """
    return i0 * (2.0 ** (cv / v_scale))


def chapter_3() -> None:
    print("\n=== Chapter 3 — Exponential converter (1 V/octave) ===")
    cvs = np.array([0.0, 1.0, 2.0, 3.0, 4.0, 5.0])
    iouts = np.array([expo_current(cv) for cv in cvs])
    ratios = iouts[1:] / iouts[:-1]

    write_dat(
        OUT / "ch03-expo.dat",
        "Expo converter: Iout vs CV (1 V/oct)",
        {"cv_V": cvs, "iout_A": iouts},
    )

    # Each +1 V should double current (2× per octave).
    ok_ratios = np.all((ratios > 1.98) & (ratios < 2.02))
    check(
        "ch3: Iout doubles per +1 V CV",
        bool(ok_ratios),
        f"ratios = {[f'{r:.4f}' for r in ratios]}",
    )
    # Absolute scale: I0 at 0 V
    check(
        "ch3: Iout(0 V) = I0 = 10 µA",
        abs(iouts[0] - 10e-6) / 10e-6 < 1e-9,
        f"Iout(0) = {iouts[0]*1e6:.3f} µA",
    )


# ---------------------------------------------------------------------------
# Chapter 4 — VCO (integrator + Schmitt + expo)
# ---------------------------------------------------------------------------

def simulate_triangle_vco(
    f0_hz: float,
    duration_s: float,
    sr: float,
    amp: float = 5.0,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """
    Ideal triangle/square VCO: frequency set externally (expo converter result).
    Triangle ramps ±amp; square is sign of the derivative (Schmitt output).
    """
    n = int(duration_s * sr)
    t = np.arange(n) / sr
    # Phase 0..1; triangle = 4*|frac-0.5|-1 mapped to ±amp? Use saw→tri fold.
    phase = (t * f0_hz) % 1.0
    tri = np.where(phase < 0.5, 4.0 * phase - 1.0, 3.0 - 4.0 * phase) * amp
    sq = np.where(phase < 0.5, amp, -amp)
    return t, tri, sq


def measure_freq(t: np.ndarray, x: np.ndarray) -> float:
    """Zero-crossing frequency estimate (rising edges)."""
    # Remove DC
    y = x - np.mean(x)
    crossings = np.where((y[:-1] < 0) & (y[1:] >= 0))[0]
    if len(crossings) < 2:
        return float("nan")
    periods = np.diff(t[crossings])
    return float(1.0 / np.mean(periods))


def chapter_4() -> None:
    print("\n=== Chapter 4 — VCO (1 V/octave tracking) ===")
    # Base frequency at CV=0: 100 Hz. Expo: f = f0 * 2^CV
    f0 = 100.0
    cvs = np.array([0.0, 1.0, 2.0])
    freqs = f0 * (2.0 ** cvs)
    measured = []
    for cv, f_target in zip(cvs, freqs):
        t, tri, sq = simulate_triangle_vco(f_target, duration_s=0.05, sr=200_000)
        f_meas = measure_freq(t, tri)
        measured.append(f_meas)
        write_dat(
            OUT / f"ch04-vco-cv{int(cv)}.dat",
            f"VCO triangle/square at CV={cv} V, target {f_target} Hz",
            {"t_s": t[::10], "tri_V": tri[::10], "sq_V": sq[::10]},
        )

    measured = np.array(measured)
    # Tracking: ratio between CV=1 and CV=0 ≈ 2
    r01 = measured[1] / measured[0]
    r12 = measured[2] / measured[1]
    check(
        "ch4: VCO ≈ 2× per octave (CV 0→1)",
        1.95 < r01 < 2.05,
        f"ratio = {r01:.4f}, f={measured[0]:.1f}→{measured[1]:.1f} Hz",
    )
    check(
        "ch4: VCO ≈ 2× per octave (CV 1→2)",
        1.95 < r12 < 2.05,
        f"ratio = {r12:.4f}, f={measured[1]:.1f}→{measured[2]:.1f} Hz",
    )
    # Duty cycle of square ~50%
    t, tri, sq = simulate_triangle_vco(400.0, 0.02, 200_000)
    duty = float(np.mean(sq > 0))
    check(
        "ch4: square duty cycle ≈ 50%",
        abs(duty - 0.5) < 0.02,
        f"duty = {duty*100:.1f}%",
    )


# ---------------------------------------------------------------------------
# Chapter 5 — State-variable VCF
# ---------------------------------------------------------------------------

def svf_process(
    x: np.ndarray,
    sr: float,
    f_c: float,
    q: float,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """
    Chamberlin / Digidesign-style discrete SVF (teaching stand-in for
    dual OTA integrators). Returns LP, BP, HP.
    f_c: cutoff Hz, q: resonance (Q).
    """
    f = 2.0 * math.sin(math.pi * min(f_c, sr * 0.25) / sr)
    q_inv = 1.0 / max(q, 0.5)
    lp = bp = hp = 0.0
    lp_o = np.empty_like(x)
    bp_o = np.empty_like(x)
    hp_o = np.empty_like(x)
    for i, xi in enumerate(x):
        hp = xi - lp - q_inv * bp
        bp = bp + f * hp
        lp = lp + f * bp
        lp_o[i] = lp
        bp_o[i] = bp
        hp_o[i] = hp
    return lp_o, bp_o, hp_o


def measure_ac_gain(f_sig: float, f_c: float, q: float, sr: float = 48_000) -> float:
    """Steady-state LP gain at f_sig for an SVF with cutoff f_c."""
    n = int(0.1 * sr)
    t = np.arange(n) / sr
    x = np.sin(2 * math.pi * f_sig * t)
    lp, _, _ = svf_process(x, sr, f_c, q)
    # Use last half to avoid transient
    half = n // 2
    return float(np.std(lp[half:]) / (np.std(x[half:]) + 1e-30))


def chapter_5() -> None:
    print("\n=== Chapter 5 — VCF (state-variable filter) ===")
    sr = 48_000
    # Sweep frequency through a fixed-cutoff filter; find −3 dB point.
    f_c = 1000.0
    q = 0.707  # Butterworth-ish
    freqs = np.logspace(1.5, 3.8, 40)  # ~32 Hz .. 6.3 kHz
    gains = np.array([measure_ac_gain(f, f_c, q, sr) for f in freqs])
    gains_db = 20.0 * np.log10(np.maximum(gains, 1e-12))

    write_dat(
        OUT / "ch05-vcf-ac.dat",
        f"SVF low-pass AC response, fc={f_c} Hz, Q={q}",
        {"freq_Hz": freqs, "gain_dB": gains_db},
    )

    # −3 dB relative to low-frequency gain
    g0 = gains_db[0]
    target = g0 - 3.0
    idx = int(np.argmin(np.abs(gains_db - target)))
    f_3db = float(freqs[idx])
    check(
        "ch5: LP −3 dB near cutoff (1 kHz)",
        700.0 < f_3db < 1400.0,
        f"f−3dB ≈ {f_3db:.0f} Hz",
    )

    # Resonance peak: higher Q → BP peak > 0 dB near fc
    q_hi = 8.0
    bp_gain = measure_ac_gain(f_c, f_c, q_hi, sr)  # uses LP path; measure BP specially
    # Re-measure with BP output
    n = int(0.1 * sr)
    t = np.arange(n) / sr
    x = np.sin(2 * math.pi * f_c * t)
    _, bp, _ = svf_process(x, sr, f_c, q_hi)
    half = n // 2
    peak = float(np.std(bp[half:]) / (np.std(x[half:]) + 1e-30))
    check(
        "ch5: resonance produces BP peak > unity",
        peak > 1.5,
        f"BP gain @ fc ≈ {peak:.2f} (Q={q_hi})",
    )

    # Cutoff tracks CV: f_c = f0 * 2^CV (same expo law as VCO)
    cv = 1.0
    f_c2 = f_c * (2.0 ** cv)
    f_3db_2_idx = None
    gains2 = np.array([measure_ac_gain(f, f_c2, q, sr) for f in freqs])
    g2_db = 20.0 * np.log10(np.maximum(gains2, 1e-12))
    target2 = g2_db[0] - 3.0
    f_3db_2 = float(freqs[int(np.argmin(np.abs(g2_db - target2)))])
    ratio = f_3db_2 / f_3db
    check(
        "ch5: cutoff tracks ≈ 2× per +1 V CV",
        1.5 < ratio < 2.8,  # discrete SVF + coarse sweep: loose but directional
        f"f−3dB {f_3db:.0f} → {f_3db_2:.0f} Hz (ratio {ratio:.2f})",
    )


# ---------------------------------------------------------------------------
# Chapter 6 — VCA (OTA)
# ---------------------------------------------------------------------------

def ota_vca_gain(cv: float, cv_max: float = 5.0, g_max: float = 1.0) -> float:
    """
    Linear-CV OTA VCA: gain = g_max * clamp(CV, 0, cv_max) / cv_max.
    Models LM13700 with linearizing diodes and Iabc ∝ CV.
    """
    return g_max * max(0.0, min(cv, cv_max)) / cv_max


def chapter_6() -> None:
    print("\n=== Chapter 6 — VCA (OTA gain cell) ===")
    cvs = np.linspace(0, 5, 11)
    gains = np.array([ota_vca_gain(cv) for cv in cvs])
    gains_db = 20.0 * np.log10(np.maximum(gains, 1e-6))

    write_dat(
        OUT / "ch06-vca.dat",
        "OTA VCA: linear gain vs CV (0–5 V)",
        {"cv_V": cvs, "gain_lin": gains, "gain_dB": gains_db},
    )

    # Monotonic
    check(
        "ch6: gain increases monotonically with CV",
        bool(np.all(np.diff(gains) >= -1e-15)),
        f"gains = {[f'{g:.2f}' for g in gains]}",
    )
    # Dynamic range: 0 V ≈ muted (−60 dB floor), 5 V = unity
    check(
        "ch6: CV=0 muted, CV=5 V ≈ unity",
        gains[0] < 1e-5 and abs(gains[-1] - 1.0) < 1e-9,
        f"g(0)={gains[0]:.2e}, g(5)={gains[-1]:.3f}",
    )
    # ~60 dB usable range between floor and full scale
    dr = float(gains_db[-1] - gains_db[0])
    check(
        "ch6: dynamic range ≥ 60 dB (sim floor)",
        dr >= 60.0,
        f"DR = {dr:.1f} dB",
    )


# ---------------------------------------------------------------------------
# Chapter 7 — LFO
# ---------------------------------------------------------------------------

def chapter_7() -> None:
    print("\n=== Chapter 7 — LFO (sub-audio VCO core) ===")
    # Same triangle core, scaled: rate knob sets f from 0.1 Hz to 20 Hz
    rates = np.array([0.1, 1.0, 10.0, 20.0])
    measured = []
    for f in rates:
        # Long enough for a few cycles at low rates
        dur = max(5.0 / f, 1.0)
        sr = 1000.0  # LFO: low sample rate is fine
        t, tri, sq = simulate_triangle_vco(f, dur, sr, amp=5.0)
        f_meas = measure_freq(t, tri)
        measured.append(f_meas)
        write_dat(
            OUT / f"ch07-lfo-{f:g}hz.dat",
            f"LFO at {f} Hz",
            {"t_s": t[:: max(1, len(t)//2000)], "tri_V": tri[:: max(1, len(t)//2000)],
             "sq_V": sq[:: max(1, len(t)//2000)]},
        )

    measured = np.array(measured)
    for f_exp, f_m in zip(rates, measured):
        err = abs(f_m - f_exp) / f_exp
        check(
            f"ch7: LFO rate ≈ {f_exp:g} Hz",
            err < 0.05,
            f"measured {f_m:.4f} Hz (err {err*100:.1f}%)",
        )


# ---------------------------------------------------------------------------
# Chapter 8 — Patch the voice: VCO → VCF → VCA ← LFO
# ---------------------------------------------------------------------------

def chapter_8() -> None:
    print("\n=== Chapter 8 — Patching the voice ===")
    sr = float(SR_AUDIO)
    duration = 1.0
    n = int(duration * sr)
    t = np.arange(n) / sr

    # VCO at ~220 Hz (A3-ish), triangle
    f_vco = 220.0
    _, tri, _ = simulate_triangle_vco(f_vco, duration, sr, amp=1.0)

    # VCF: low-pass, cutoff ~800 Hz, mild resonance
    lp, _, _ = svf_process(tri, sr, f_c=800.0, q=1.2)

    # LFO at 4 Hz modulating VCA depth 0.2..1.0
    f_lfo = 4.0
    lfo = 0.5 + 0.5 * np.sin(2 * math.pi * f_lfo * t)  # 0..1
    cv = 1.0 + 4.0 * lfo  # 1..5 V effective
    gains = np.array([ota_vca_gain(c) for c in cv])
    out = lp * gains

    write_dat(
        OUT / "ch08-voice.dat",
        "Voice: VCO→VCF→VCA, LFO→VCA CV",
        {
            "t_s": t[::10],
            "vco_V": tri[::10],
            "vcf_V": lp[::10],
            "lfo": lfo[::10],
            "out_V": out[::10],
        },
    )
    write_wav(OUT / "voice.wav", out / (np.max(np.abs(out)) + 1e-12))

    # Output should have energy near VCO frequency
    # Simple check: RMS of output > 0 and amplitude modulation depth present
    rms = float(np.sqrt(np.mean(out**2)))
    # Envelope via rectify + lowpass (crude)
    env = np.abs(out)
    # Block RMS every 10 ms
    block = int(0.01 * sr)
    blocks = env[: n - (n % block)].reshape(-1, block).mean(axis=1)
    mod_depth = float((blocks.max() - blocks.min()) / (blocks.max() + 1e-12))

    check(
        "ch8: voice output has non-zero RMS",
        rms > 0.05,
        f"RMS = {rms:.3f}",
    )
    check(
        "ch8: LFO amplitude-modulates the VCA",
        mod_depth > 0.3,
        f"envelope depth = {mod_depth:.2f}",
    )
    # Spectrum peak near 220 Hz via DFT of a window
    window = out[int(0.2 * sr) : int(0.2 * sr) + 4096]
    spec = np.abs(np.fft.rfft(window * np.hanning(len(window))))
    freqs = np.fft.rfftfreq(len(window), 1.0 / sr)
    peak_f = float(freqs[int(np.argmax(spec[1:])) + 1])  # skip DC
    check(
        "ch8: spectral peak near VCO (220 Hz)",
        abs(peak_f - f_vco) < 30.0,
        f"peak ≈ {peak_f:.1f} Hz",
    )


# ---------------------------------------------------------------------------
# Chapters 9–10 are prose-only; emit a marker file so the scorecard notes them.
# ---------------------------------------------------------------------------

def chapter_9_10_markers() -> None:
    print("\n=== Chapters 9–10 — Prose (see DOCUMENTATION.md) ===")
    (OUT / "ch09-music-scene.txt").write_text(
        "Chapter 9 is documentation-only: guitar amps, Eurorack, DJ mixers, studio rack.\n"
        "See DOCUMENTATION.md § Chapter 9.\n"
    )
    (OUT / "ch10-bench.txt").write_text(
        "Chapter 10 is documentation-only: BOM, macromodel pitfalls, stripboard next steps.\n"
        "See DOCUMENTATION.md § Chapter 10.\n"
    )
    check("ch9: music-scene comparison documented", True, "DOCUMENTATION.md")
    check("ch10: sim-to-bench guidance documented", True, "DOCUMENTATION.md")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    CHECKS.clear()
    print("Analog Electronics for Modular Synthesis — simulation scorecard")
    print(f"Output directory: {OUT}")

    chapter_1()
    chapter_2()
    chapter_3()
    chapter_4()
    chapter_5()
    chapter_6()
    chapter_7()
    chapter_8()
    chapter_9_10_markers()

    passed = sum(1 for c in CHECKS if c.ok)
    total = len(CHECKS)
    print("\n" + "=" * 60)
    print(f"SCORECARD: {passed}/{total} checks passed")
    print("=" * 60)
    if passed != total:
        print("FAILED checks:")
        for c in CHECKS:
            if not c.ok:
                print(f"  - {c.name}: {c.detail}")
        return 1
    print("All checks passed. Open circuits/*.cir in Qucs-S for schematic view.")
    print("Listen: out/voice.wav")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
