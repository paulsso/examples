#!/usr/bin/env python3
# ============================================================================
#  main.py — High Performance Python for Fluid Dynamics
# ============================================================================
#
#  This course teaches HIGH PERFORMANCE PYTHON with fluid dynamics as the
#  domain: every chapter updates real grid fields, measures how long it
#  takes, and then makes it faster. The performance ladder it climbs:
#
#      pure Python  ->  vectorized NumPy  ->  better dtypes & no copies
#                   ->  better ALGORITHMS (FFT Poisson)  ->  timestep budgets
#                   ->  profiling; native code only as the last resort
#
#  Fluids are ideal for this: grids are large, stencils are array math,
#  stability (CFL) imposes a hard step-size deadline, and you can *see*
#  the results — every run writes fields into out/ (CSV / PPM / ASCII).
#
#  Chapters:
#    1.  Discrete fluids in pure Python — grids, CFL, 1D advection
#    2.  The cost of pure Python      — interpreter tax on stencils
#    3.  Vectorization with NumPy     — the 10-100x free speedup
#    4.  dtypes & memory              — float64/float32, bandwidth
#    5.  Views, copies, in-place      — halos, double-buffer without alloc
#    6.  A fluids toolbox             — advection, diffusion, sources
#    7.  Elliptic solvers             — Jacobi vs GS; constants matter
#    8.  The FFT                      — spectral Poisson: O(n²)→O(n log n)
#    9.  Projection method            — incompressible NS lite (Stam-style)
#   10.  A simulation pipeline        — lid-driven cavity + frames
#   11.  Timestep budgets             — CFL wall-clock deadline
#   12.  Profiling & the playbook     — cProfile, then fix the hotspot
#
#  Requires: Python 3.10+, NumPy. Run: `make run` (or python3 main.py).
#  Timing numbers vary per machine/run; the RATIOS are the lesson.
# ============================================================================

from __future__ import annotations

import cProfile
import io
import math
import os
import pstats
from time import perf_counter

import numpy as np

OUT_DIR = "out"


def chapter(title: str) -> None:
    print("\n=============================================")
    print(" " + title)
    print("=============================================")


def bench(fn, repeat: int = 3):
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


def write_ppm(path: str, field: np.ndarray, vmin: float | None = None,
              vmax: float | None = None) -> None:
    """Write a scalar 2D field as an 8-bit grayscale PPM (P5)."""
    a = np.asarray(field, dtype=np.float64)
    lo = float(np.min(a)) if vmin is None else vmin
    hi = float(np.max(a)) if vmax is None else vmax
    if hi <= lo:
        hi = lo + 1.0
    norm = np.clip((a - lo) / (hi - lo), 0.0, 1.0)
    img = (norm * 255.0).astype(np.uint8)
    h, w = img.shape
    with open(path, "wb") as f:
        f.write(f"P5\n{w} {h}\n255\n".encode("ascii"))
        f.write(img.tobytes())


def write_csv(path: str, *cols: np.ndarray, header: str = "") -> None:
    data = np.column_stack(cols)
    np.savetxt(path, data, delimiter=",", header=header, comments="# ")


# ===========================================================================
# CHAPTER 1 — DISCRETE FLUIDS IN PURE PYTHON
# ===========================================================================
# Continuum PDEs become finite-difference updates on a grid. The Courant–
# Friedrichs–Lewy (CFL) condition says information must not jump more than
# one cell per step: |u|·Δt/Δx ≤ 1. This chapter does 1D linear advection
# in pure Python — slow, but it shows there is no magic: a fluid solver is
# nested loops over floats.

def demo_discrete_fluids():
    # 1D linear advection: ∂q/∂t + c ∂q/∂x = 0, upwind for c > 0.
    n = 200
    L = 1.0
    dx = L / n
    c = 1.0
    cfl = 0.5
    dt = cfl * dx / c
    steps = int(0.5 / dt)  # advect for ~0.5 time units

    # Initial Gaussian bump
    x = [i * dx for i in range(n)]
    q = [math.exp(-((xi - 0.3) / 0.05) ** 2) for xi in x]

    t0 = perf_counter()
    for _ in range(steps):
        q_new = [0.0] * n
        for i in range(1, n):
            q_new[i] = q[i] - cfl * (q[i] - q[i - 1])
        q_new[0] = q_new[n - 1]  # periodic
        q = q_new
    gen_ms = (perf_counter() - t0) * 1000.0

    # Exact: bump centered at 0.3 + c*0.5 = 0.8
    x_exact = 0.3 + c * 0.5
    peak_i = max(range(n), key=lambda i: q[i])
    peak_x = x[peak_i]
    err = abs(peak_x - x_exact)

    print(f"1D advection: N={n}, CFL={cfl}, {steps} steps, pure Python")
    print(f"  wall time {gen_ms:.1f} ms")
    print(f"  peak at x={peak_x:.3f} (exact {x_exact:.3f}, |err|={err:.3f})")
    print(f"  CFL |c|Δt/Δx = {cfl} ≤ 1 → stable upwind")

    os.makedirs(OUT_DIR, exist_ok=True)
    path = os.path.join(OUT_DIR, "advection1d.csv")
    with open(path, "w") as f:
        f.write("# x,q\n")
        for xi, qi in zip(x, q):
            f.write(f"{xi:.6g},{qi:.6g}\n")
    print(f"wrote {path}")


# ===========================================================================
# CHAPTER 2 — THE COST OF PURE PYTHON
# ===========================================================================
# Every Python bytecode op costs ~10–100 ns of interpreter overhead. A 2D
# 5-point Laplacian over a 256² grid does ~65k cells × that tax. Measure it.

def demo_interpreter_tax():
    n = 128
    # Flat lists of lists for a pure-Python "grid"
    field = [[math.sin(0.1 * i) * math.cos(0.1 * j) for j in range(n)]
             for i in range(n)]

    def laplace_loops():
        out = [[0.0] * n for _ in range(n)]
        for i in range(1, n - 1):
            for j in range(1, n - 1):
                out[i][j] = (
                    field[i + 1][j] + field[i - 1][j]
                    + field[i][j + 1] + field[i][j - 1]
                    - 4.0 * field[i][j]
                )
        return out

    def sum_cells():
        return sum(field[i][j] for i in range(n) for j in range(n))

    lap_ms, _ = bench(laplace_loops)
    sum_ms, total = bench(sum_cells)

    print(f"5-point Laplacian on {n}×{n} = {n*n:,} cells, pure Python:")
    print(f"  nested for loops      : {lap_ms:7.1f} ms")
    print(f"  sum of all cells      : {sum_ms:7.1f} ms  (sum={total:.3f})")
    print("every cell pays interpreter overhead; the fix is to pay it")
    print("ONCE per array instead of once per element → Chapter 3")


# ===========================================================================
# CHAPTER 3 — VECTORIZATION WITH NUMPY
# ===========================================================================
# numpy stores numbers unboxed in one contiguous C array; slicing + arithmetic
# runs ONE C loop (often SIMD). Same Laplacian, typically 50–100× faster.

def demo_vectorization():
    n = 256
    y = np.arange(n)[:, None]
    x = np.arange(n)[None, :]
    field = np.sin(0.1 * y) * np.cos(0.1 * x)

    def laplace_py():
        out = np.zeros_like(field)
        for i in range(1, n - 1):
            for j in range(1, n - 1):
                out[i, j] = (
                    field[i + 1, j] + field[i - 1, j]
                    + field[i, j + 1] + field[i, j - 1]
                    - 4.0 * field[i, j]
                )
        return out

    def laplace_np():
        return (
            field[2:, 1:-1] + field[:-2, 1:-1]
            + field[1:-1, 2:] + field[1:-1, :-2]
            - 4.0 * field[1:-1, 1:-1]
        )

    # Warm-up / correctness
    a = laplace_np()
    # Only compare interior; pure-Python version for timing on smaller n
    n_small = 64
    field_s = field[:n_small, :n_small].copy()

    def laplace_py_small():
        out = np.zeros_like(field_s)
        for i in range(1, n_small - 1):
            for j in range(1, n_small - 1):
                out[i, j] = (
                    field_s[i + 1, j] + field_s[i - 1, j]
                    + field_s[i, j + 1] + field_s[i, j - 1]
                    - 4.0 * field_s[i, j]
                )
        return out

    def laplace_np_small():
        f = field_s
        return (
            f[2:, 1:-1] + f[:-2, 1:-1]
            + f[1:-1, 2:] + f[1:-1, :-2]
            - 4.0 * f[1:-1, 1:-1]
        )

    py_ms, _ = bench(laplace_py_small, repeat=2)
    np_ms, _ = bench(laplace_np)
    np_small_ms, b = bench(laplace_np_small)
    # residual check on small
    py_out = laplace_py_small()
    max_diff = float(np.max(np.abs(py_out[1:-1, 1:-1] - b)))

    print(f"Laplacian {n}×{n} with NumPy slices : {np_ms:7.2f} ms")
    print(f"same op {n_small}×{n_small} pure Python loops : {py_ms:7.1f} ms")
    print(f"same op {n_small}×{n_small} NumPy           : {np_small_ms:7.2f} ms  "
          f"(~{py_ms / max(np_small_ms, 1e-9):.0f}× faster)")
    print(f"max |py − np| on interior = {max_diff:.2e} (should be ~0)")
    print("rule: if you wrote `for i in range(ny): for j in range(nx)` in")
    print("Python, you are leaving a ~100× speedup on the table")
    _ = a  # silence unused


# ===========================================================================
# CHAPTER 4 — DTYPES & MEMORY
# ===========================================================================
# CFD grids are memory-bound: float32 halves bandwidth vs float64. CFD often
# keeps float64 for residuals/pressure; many transport fields are fine in f32.

def demo_dtypes():
    n = 1024
    rng = np.random.default_rng(42)
    a64 = rng.standard_normal((n, n))
    a32 = a64.astype(np.float32)

    print(f"{n}×{n} as float64: {a64.nbytes / 1e6:6.1f} MB")
    print(f"{n}×{n} as float32: {a32.nbytes / 1e6:6.1f} MB")

    def stencil64():
        return a64[2:, 1:-1] + a64[:-2, 1:-1] + a64[1:-1, 2:] + a64[1:-1, :-2] - 4.0 * a64[1:-1, 1:-1]

    def stencil32():
        four = np.float32(4.0)
        return (
            a32[2:, 1:-1] + a32[:-2, 1:-1]
            + a32[1:-1, 2:] + a32[1:-1, :-2]
            - four * a32[1:-1, 1:-1]
        )

    ms64, _ = bench(stencil64)
    ms32, _ = bench(stencil32)
    print(f"5-point stencil float64 : {ms64:6.2f} ms")
    print(f"5-point stencil float32 : {ms32:6.2f} ms  "
          f"(fewer bytes → less time; ratio varies)")
    print("note the float32 scalar: a bare `4.0` would promote back to f64")


# ===========================================================================
# CHAPTER 5 — VIEWS, COPIES, AND IN-PLACE OPS
# ===========================================================================
# Halo / ghost cells are slices (views). Double-buffering must reuse arrays;
# allocating every step kills cache and GC.

def demo_views_and_copies():
    a = np.arange(16).reshape(4, 4)
    halo = a[0, :]  # view of first row
    halo[:] = -1
    print(f"a[0,:] is a view: after halo[:]=-1,\n{a}")

    n = 512
    rng = np.random.default_rng(1)
    u = rng.standard_normal((n, n))
    v = np.empty_like(u)

    alloc_ms, _ = bench(lambda: u * 0.5)  # allocates

    def swap_copy():
        # naive: allocate every step
        return u[2:, 1:-1] + u[:-2, 1:-1] + u[1:-1, 2:] + u[1:-1, :-2] - 4.0 * u[1:-1, 1:-1]

    def double_buf():
        # write into preallocated v interior
        v[1:-1, 1:-1] = (
            u[2:, 1:-1] + u[:-2, 1:-1]
            + u[1:-1, 2:] + u[1:-1, :-2]
            - 4.0 * u[1:-1, 1:-1]
        )
        return v

    ms_alloc, _ = bench(swap_copy)
    ms_buf, _ = bench(double_buf)
    print(f"stencil that returns a new array : {ms_alloc:6.2f} ms")
    print(f"stencil into preallocated buffer : {ms_buf:6.2f} ms")
    print("CFD rule: allocate u,v (and pressure buffers) once; ping-pong")
    print("pointers / views each step — never allocate inside the time loop")


# ===========================================================================
# CHAPTER 6 — A FLUIDS TOOLBOX
# ===========================================================================
# Building blocks, all vectorized: diffusion, upwind advection, forcing.

def diffuse_ftcs(q: np.ndarray, nu: float, dt: float, dx: float) -> np.ndarray:
    """Forward-Time Centered-Space diffusion, Neumann (copy) boundaries."""
    alpha = nu * dt / (dx * dx)
    out = q.copy()
    out[1:-1, 1:-1] = (
        q[1:-1, 1:-1]
        + alpha * (
            q[2:, 1:-1] + q[:-2, 1:-1]
            + q[1:-1, 2:] + q[1:-1, :-2]
            - 4.0 * q[1:-1, 1:-1]
        )
    )
    # Neumann: copy edge from interior
    out[0, :] = out[1, :]
    out[-1, :] = out[-2, :]
    out[:, 0] = out[:, 1]
    out[:, -1] = out[:, -2]
    return out


def advect_upwind(q: np.ndarray, u: float, v: float, dt: float, dx: float) -> np.ndarray:
    """Constant-velocity upwind advection on a uniform grid (interior)."""
    cx = u * dt / dx
    cy = v * dt / dx
    out = q.copy()
    # u > 0, v > 0 assumed for this teaching helper
    assert u >= 0 and v >= 0
    out[1:-1, 1:-1] = (
        q[1:-1, 1:-1]
        - cx * (q[1:-1, 1:-1] - q[1:-1, :-2])
        - cy * (q[1:-1, 1:-1] - q[:-2, 1:-1])
    )
    return out


def demo_fluids_toolbox():
    n = 64
    L = 1.0
    dx = L / (n - 1)
    x = np.linspace(0, L, n)
    y = np.linspace(0, L, n)
    X, Y = np.meshgrid(x, y)
    # Exact 1D heat: q=sin(πx) e^{-ν π² t} on [0,1] with Dirichlet 0 —
    # use a 2D strip variant: q = sin(π X) * ones in y, but BCs complicate.
    # Verify diffusion L2 decay on a periodic-ish interior mode instead.
    q0 = np.sin(np.pi * X) * np.sin(np.pi * Y)
    nu = 0.1
    # FTCS stability: α = ν dt/dx² ≤ 1/4 in 2D
    dt = 0.2 * dx * dx / nu
    t_end = 0.05
    steps = max(1, int(t_end / dt))
    q = q0.copy()
    for _ in range(steps):
        q = diffuse_ftcs(q, nu, dt, dx)
    t = steps * dt
    # Mode (1,1) decays as exp(-ν * 2 π² t) for Dirichlet sin on unit square
    q_exact = q0 * math.exp(-nu * 2.0 * math.pi ** 2 * t)
    # Compare interior away from Neumann boundaries we imposed
    interior = (slice(4, -4), slice(4, -4))
    rel = float(
        np.linalg.norm(q[interior] - q_exact[interior])
        / (np.linalg.norm(q_exact[interior]) + 1e-30)
    )
    print(f"diffusion FTCS: N={n}, steps={steps}, t={t:.4f}")
    print(f"  rel L2 error vs mode decay (interior) = {rel:.3f}")
    print(f"  (Neumann edges vs Dirichlet analytic → expect O(0.1) error OK)")

    # Advection: shift a blob
    blob = np.exp(-((X - 0.3) ** 2 + (Y - 0.3) ** 2) / (2 * 0.05 ** 2))
    u_adv, v_adv = 1.0, 0.5
    cfl = 0.4
    dt_a = cfl * dx / max(u_adv, v_adv)
    steps_a = int(0.2 / dt_a)
    qb = blob.copy()
    for _ in range(steps_a):
        qb = advect_upwind(qb, u_adv, v_adv, dt_a, dx)
    peak = np.unravel_index(int(np.argmax(qb)), qb.shape)
    print(f"upwind advection: peak moved to (i,j)=({peak[0]},{peak[1]}) "
          f"~ ({x[peak[1]]:.2f},{y[peak[0]]:.2f})")

    os.makedirs(OUT_DIR, exist_ok=True)
    write_ppm(os.path.join(OUT_DIR, "diffuse.ppm"), q)
    write_ppm(os.path.join(OUT_DIR, "advect.ppm"), qb)
    print(f"wrote {OUT_DIR}/diffuse.ppm, {OUT_DIR}/advect.ppm")


# ===========================================================================
# CHAPTER 7 — ELLIPTIC SOLVERS (PRESSURE / POISSON)
# ===========================================================================
# Incompressible flow needs ∇²p = rhs each step. Jacobi is easy to vectorize;
# Gauss–Seidel converges faster but is sequential — same algorithm-vs-constants
# lesson as audio filters.

def jacobi_poisson(p: np.ndarray, rhs: np.ndarray, dx: float, iters: int) -> np.ndarray:
    """∇²p = rhs on interior; Dirichlet p=0 on boundary. Jacobi."""
    p = p.copy()
    dx2 = dx * dx
    for _ in range(iters):
        p_new = p.copy()
        p_new[1:-1, 1:-1] = 0.25 * (
            p[2:, 1:-1] + p[:-2, 1:-1]
            + p[1:-1, 2:] + p[1:-1, :-2]
            - dx2 * rhs[1:-1, 1:-1]
        )
        p = p_new
        p[0, :] = 0.0
        p[-1, :] = 0.0
        p[:, 0] = 0.0
        p[:, -1] = 0.0
    return p


def gauss_seidel_poisson(p: np.ndarray, rhs: np.ndarray, dx: float, iters: int) -> np.ndarray:
    """Same Poisson, Gauss–Seidel (in-place, sequential) — harder to SIMD."""
    p = p.copy()
    dx2 = dx * dx
    ny, nx = p.shape
    for _ in range(iters):
        for i in range(1, ny - 1):
            for j in range(1, nx - 1):
                p[i, j] = 0.25 * (
                    p[i + 1, j] + p[i - 1, j]
                    + p[i, j + 1] + p[i, j - 1]
                    - dx2 * rhs[i, j]
                )
        p[0, :] = 0.0
        p[-1, :] = 0.0
        p[:, 0] = 0.0
        p[:, -1] = 0.0
    return p


def demo_elliptic_solvers():
    n = 65  # odd → nice center
    L = 1.0
    dx = L / (n - 1)
    x = np.linspace(0, L, n)
    y = np.linspace(0, L, n)
    X, Y = np.meshgrid(x, y)
    # Manufactured: p* = sin(πx)sin(πy), ∇²p* = -2π² p*
    p_exact = np.sin(np.pi * X) * np.sin(np.pi * Y)
    rhs = -2.0 * math.pi ** 2 * p_exact

    # Jacobi needs many iters on this grid; GS converges faster per iter
    # but the pure-Python GS loop pays the interpreter tax (Ch 2).
    iters_j = 2500
    iters_gs = 200

    t0 = perf_counter()
    p_j = jacobi_poisson(np.zeros((n, n)), rhs, dx, iters_j)
    j_ms = (perf_counter() - t0) * 1000.0

    t0 = perf_counter()
    p_gs = gauss_seidel_poisson(np.zeros((n, n)), rhs, dx, iters_gs)
    gs_ms = (perf_counter() - t0) * 1000.0

    err_j = float(np.linalg.norm(p_j - p_exact) / np.linalg.norm(p_exact))
    err_gs = float(np.linalg.norm(p_gs - p_exact) / np.linalg.norm(p_exact))

    print(f"Poisson manufactured solution on {n}×{n}:")
    print(f"  Jacobi       {iters_j} iters : {j_ms:7.1f} ms, rel L2 err={err_j:.3e}")
    print(f"  Gauss–Seidel {iters_gs} iters : {gs_ms:7.1f} ms, rel L2 err={err_gs:.3e}")
    print("Jacobi vectorizes beautifully; GS needs fewer iters but the")
    print("Python double loop is the interpreter tax again — Chapter 8")
    print("removes the iteration bottleneck entirely with an FFT.")


# ===========================================================================
# CHAPTER 8 — THE FFT (SPECTRAL POISSON)
# ===========================================================================
# On a periodic box, ∇²p = rhs ⇒ −k² p̂ = rhŝ. One FFT + multiply + IFFT
# replaces hundreds of Jacobi sweeps. O(N²) iterations → O(N log N).

def spectral_poisson_periodic(rhs: np.ndarray, dx: float) -> np.ndarray:
    """Solve ∇²p = rhs with periodic BCs via rFFT2. Mean mode set to 0."""
    ny, nx = rhs.shape
    rhs_hat = np.fft.rfft2(rhs)
    ky = 2.0 * np.pi * np.fft.fftfreq(ny, d=dx)
    kx = 2.0 * np.pi * np.fft.rfftfreq(nx, d=dx)
    KX, KY = np.meshgrid(kx, ky)
    k2 = KX * KX + KY * KY
    # Avoid divide by zero at k=0 (compatibility: mean(rhs) should be ~0)
    inv_k2 = np.zeros_like(k2)
    mask = k2 > 0
    inv_k2[mask] = 1.0 / k2[mask]
    p_hat = -rhs_hat * inv_k2
    return np.fft.irfft2(p_hat, s=rhs.shape)


def demo_fft_poisson():
    n = 128
    L = 1.0
    dx = L / n
    x = np.arange(n) * dx
    y = np.arange(n) * dx
    X, Y = np.meshgrid(x, y)
    # Periodic manufactured: p = sin(2πx)sin(2πy)
    p_exact = np.sin(2 * np.pi * X) * np.sin(2 * np.pi * Y)
    rhs = -2.0 * (2 * np.pi) ** 2 * p_exact  # ∇²p = -8π² p

    # Jacobi on same size would need huge iters — time a modest Jacobi vs FFT
    def run_jacobi():
        return jacobi_poisson(np.zeros((n, n)), rhs, dx, iters=80)

    def run_fft():
        return spectral_poisson_periodic(rhs, dx)

    j_ms, p_j = bench(run_jacobi, repeat=2)
    f_ms, p_f = bench(run_fft, repeat=3)

    err_j = float(np.linalg.norm(p_j - p_exact) / (np.linalg.norm(p_exact) + 1e-30))
    err_f = float(np.linalg.norm(p_f - p_exact) / (np.linalg.norm(p_exact) + 1e-30))

    print(f"periodic Poisson {n}×{n}:")
    print(f"  Jacobi 80 iters : {j_ms:7.2f} ms, rel err={err_j:.3e}")
    print(f"  FFT spectral    : {f_ms:7.2f} ms, rel err={err_f:.3e}  "
          f"(~{j_ms / max(f_ms, 1e-9):.0f}× wall-time vs this Jacobi budget)")
    print("FFT wins on accuracy *and* asymptotics: one shot, machine precision")
    print("on the eigenmodes — the biggest algorithmic lever in this course")

    os.makedirs(OUT_DIR, exist_ok=True)
    write_ppm(os.path.join(OUT_DIR, "poisson_fft.ppm"), p_f)


# ===========================================================================
# CHAPTER 9 — PROJECTION METHOD (INCOMPRESSIBLE NS LITE)
# ===========================================================================
# Jos Stam / Chorin projection: advect → diffuse → project (make ∇·u = 0).
# We use a compact periodic spectral projection for clarity + speed.

def project_spectral(u: np.ndarray, v: np.ndarray, dx: float):
    """Remove divergence: u := u − ∂p/∂x with ∇²p = ∇·u (periodic)."""
    # Discrete divergence via spectral derivatives
    uy_hat = np.fft.rfft2(u)
    vy_hat = np.fft.rfft2(v)
    ny, nx = u.shape
    ky = 2.0 * np.pi * np.fft.fftfreq(ny, d=dx)[:, None]
    kx = 2.0 * np.pi * np.fft.rfftfreq(nx, d=dx)[None, :]
    # div_hat = i kx û + i ky v̂
    # div̂ = i kx û + i ky v̂;  ∇²p = div ⇒ −k² p̂ = div̂ ⇒ p̂ = −div̂/k²
    div_hat = 1j * kx * uy_hat + 1j * ky * vy_hat
    k2 = kx * kx + ky * ky
    inv = np.zeros_like(k2, dtype=np.float64)
    mask = k2 > 0
    inv[mask] = 1.0 / k2[mask]
    p_hat = -div_hat * inv
    # u ← u − ∂p/∂x  ⇒ û ← û − i kx p̂
    u_hat = uy_hat - 1j * kx * p_hat
    v_hat = vy_hat - 1j * ky * p_hat
    u_hat[0, 0] = 0.0  # keep zero mean
    v_hat[0, 0] = 0.0
    return np.fft.irfft2(u_hat, s=u.shape), np.fft.irfft2(v_hat, s=v.shape)


def divergence(u: np.ndarray, v: np.ndarray, dx: float) -> np.ndarray:
    """Central-difference divergence (periodic wrap via roll)."""
    dudx = (np.roll(u, -1, axis=1) - np.roll(u, 1, axis=1)) / (2 * dx)
    dvdy = (np.roll(v, -1, axis=0) - np.roll(v, 1, axis=0)) / (2 * dx)
    return dudx + dvdy


def demo_projection():
    n = 64
    L = 1.0
    dx = L / n
    x = np.arange(n) * dx
    y = np.arange(n) * dx
    X, Y = np.meshgrid(x, y)
    # Divergent field: u = sin(2πx), v = 0 → div = 2π cos(2πx)
    u = np.sin(2 * np.pi * X)
    v = np.zeros_like(u)
    div0 = divergence(u, v, dx)
    u2, v2 = project_spectral(u, v, dx)
    div1 = divergence(u2, v2, dx)

    max0 = float(np.max(np.abs(div0)))
    max1 = float(np.max(np.abs(div1)))
    print(f"projection on {n}×{n} periodic grid:")
    print(f"  max|∇·u| before = {max0:.3e}")
    print(f"  max|∇·u| after  = {max1:.3e}")
    print("incompressible step: enforce divergence-free velocity each frame")


# ===========================================================================
# CHAPTER 10 — A SIMULATION PIPELINE (LID-DRIVEN CAVITY LITE)
# ===========================================================================
# Classic CFD benchmark: unit square, top lid u=U, other walls no-slip.
# We run a simplified vorticity–streamfunction / viscous Burgers-like
# velocity update with Jacobi pressure projection (Dirichlet-friendly).

def lid_driven_step(u, v, p, dt, dx, nu, lid_u, pressure_iters=40):
    """One fractional-step: viscous + lid BC, then pressure projection (Jacobi)."""
    # Viscous FTCS on interior
    alpha = nu * dt / (dx * dx)
    u_star = u.copy()
    v_star = v.copy()
    u_star[1:-1, 1:-1] = u[1:-1, 1:-1] + alpha * (
        u[2:, 1:-1] + u[:-2, 1:-1] + u[1:-1, 2:] + u[1:-1, :-2] - 4.0 * u[1:-1, 1:-1]
    )
    v_star[1:-1, 1:-1] = v[1:-1, 1:-1] + alpha * (
        v[2:, 1:-1] + v[:-2, 1:-1] + v[1:-1, 2:] + v[1:-1, :-2] - 4.0 * v[1:-1, 1:-1]
    )
    # Simple upwind-ish self-advection (first-order, teaching)
    cx = dt / dx
    u_star[1:-1, 1:-1] -= cx * u[1:-1, 1:-1] * (u[1:-1, 1:-1] - u[1:-1, :-2])
    u_star[1:-1, 1:-1] -= cx * v[1:-1, 1:-1] * (u[1:-1, 1:-1] - u[:-2, 1:-1])
    v_star[1:-1, 1:-1] -= cx * u[1:-1, 1:-1] * (v[1:-1, 1:-1] - v[1:-1, :-2])
    v_star[1:-1, 1:-1] -= cx * v[1:-1, 1:-1] * (v[1:-1, 1:-1] - v[:-2, 1:-1])

    # Boundary conditions: no-slip, lid
    u_star[0, :] = 0.0
    u_star[-1, :] = lid_u
    u_star[:, 0] = 0.0
    u_star[:, -1] = 0.0
    v_star[0, :] = 0.0
    v_star[-1, :] = 0.0
    v_star[:, 0] = 0.0
    v_star[:, -1] = 0.0

    # Divergence of intermediate field
    div = np.zeros_like(u_star)
    div[1:-1, 1:-1] = (
        (u_star[1:-1, 2:] - u_star[1:-1, :-2]) / (2 * dx)
        + (v_star[2:, 1:-1] - v_star[:-2, 1:-1]) / (2 * dx)
    )
    # ∇²p = div / dt   (Chorin)
    rhs = div / dt
    p = jacobi_poisson(p, rhs, dx, pressure_iters)

    # Correct velocity
    u_new = u_star.copy()
    v_new = v_star.copy()
    u_new[1:-1, 1:-1] -= dt * (p[1:-1, 2:] - p[1:-1, :-2]) / (2 * dx)
    v_new[1:-1, 1:-1] -= dt * (p[2:, 1:-1] - p[:-2, 1:-1]) / (2 * dx)
    # Re-apply BCs
    u_new[0, :] = 0.0
    u_new[-1, :] = lid_u
    u_new[:, 0] = 0.0
    u_new[:, -1] = 0.0
    v_new[0, :] = 0.0
    v_new[-1, :] = 0.0
    v_new[:, 0] = 0.0
    v_new[:, -1] = 0.0
    return u_new, v_new, p


def demo_pipeline():
    n = 49
    L = 1.0
    dx = L / (n - 1)
    nu = 0.1
    lid_u = 1.0
    # CFL-ish dt
    dt = 0.15 * dx * dx / nu
    steps = 80

    u = np.zeros((n, n))
    v = np.zeros((n, n))
    p = np.zeros((n, n))
    u[-1, :] = lid_u

    t0 = perf_counter()
    for _ in range(steps):
        u, v, p = lid_driven_step(u, v, p, dt, dx, nu, lid_u, pressure_iters=30)
    ms = (perf_counter() - t0) * 1000.0

    speed = np.hypot(u, v)
    # Lid should stay ≈ lid_u; bottom ≈ 0
    print(f"lid-driven cavity lite: {n}×{n}, {steps} steps in {ms:.1f} ms")
    print(f"  mean |u_lid|={float(np.mean(np.abs(u[-1, 1:-1]))):.3f} "
          f"(target {lid_u})")
    print(f"  mean |u_bottom|={float(np.mean(np.abs(u[0, 1:-1]))):.3e}")
    print(f"  max speed={float(np.max(speed)):.3f}")

    os.makedirs(OUT_DIR, exist_ok=True)
    write_ppm(os.path.join(OUT_DIR, "cavity_speed.ppm"), speed)
    # Save a mid-line u-profile for plotting
    mid = n // 2
    y = np.linspace(0, L, n)
    write_csv(
        os.path.join(OUT_DIR, "cavity_u_mid.csv"),
        y,
        u[:, mid],
        header="y,u_mid",
    )
    print(f"wrote {OUT_DIR}/cavity_speed.ppm, {OUT_DIR}/cavity_u_mid.csv")


# ===========================================================================
# CHAPTER 11 — TIMESTEP BUDGETS (THE CFL DEADLINE)
# ===========================================================================
# Real-time viz / coupling: each physical dt must finish before the next
# frame. Same idea as audio's 11.6 ms callback — here the deadline is dt.

def demo_timestep_budget():
    n = 64
    L = 1.0
    dx = L / n
    nu = 0.05
    # Stability-limited dt (diffusive)
    dt_diff = 0.2 * dx * dx / nu
    u_char = 1.0
    dt_adv = 0.4 * dx / u_char
    dt = min(dt_diff, dt_adv)

    # Time one projection + viscous step (periodic spectral path)
    rng = np.random.default_rng(0)
    u = rng.standard_normal((n, n)) * 0.1
    v = rng.standard_normal((n, n)) * 0.1
    u, v = project_spectral(u, v, dx)

    def one_step():
        nonlocal u, v
        alpha = nu * dt / (dx * dx)
        u2 = u + alpha * (
            np.roll(u, 1, 0) + np.roll(u, -1, 0)
            + np.roll(u, 1, 1) + np.roll(u, -1, 1)
            - 4.0 * u
        )
        v2 = v + alpha * (
            np.roll(v, 1, 0) + np.roll(v, -1, 0)
            + np.roll(v, 1, 1) + np.roll(v, -1, 1)
            - 4.0 * v
        )
        u, v = project_spectral(u2, v2, dx)
        return u

    # Measure many steps for stable timing
    def run_block(k=20):
        for _ in range(k):
            one_step()

    ms20, _ = bench(lambda: run_block(20), repeat=3)
    ms_per = ms20 / 20.0
    deadline_ms = dt * 1000.0
    headroom = deadline_ms / max(ms_per, 1e-12)

    print(f"grid {n}×{n}: dt={dt:.4e} s → deadline {deadline_ms:.4f} ms/step")
    print(f"  measured {ms_per:.4f} ms/step  (headroom ×{headroom:.0f})")
    print("if headroom < 1, you miss real-time: coarsen grid, raise ν, or")
    print("optimize (dtype, FFT plan reuse, fewer Jacobi iters)")


# ===========================================================================
# CHAPTER 12 — PROFILING & THE PLAYBOOK
# ===========================================================================

def demo_profiling():
    n = 96
    L = 1.0
    dx = L / (n - 1)
    nu = 0.08
    lid_u = 1.0
    dt = 0.12 * dx * dx / nu
    u = np.zeros((n, n))
    v = np.zeros((n, n))
    p = np.zeros((n, n))
    u[-1, :] = lid_u

    def run():
        nonlocal u, v, p
        for _ in range(25):
            u, v, p = lid_driven_step(u, v, p, dt, dx, nu, lid_u, pressure_iters=25)

    pr = cProfile.Profile()
    pr.enable()
    run()
    pr.disable()
    buf = io.StringIO()
    stats = pstats.Stats(pr, stream=buf).sort_stats("cumulative")
    stats.print_stats(8)
    text = buf.getvalue()
    print("cProfile top (cumulative):")
    # Print a few non-empty lines from the stats table
    lines = [ln for ln in text.splitlines() if ln.strip()]
    for ln in lines[:12]:
        print("  " + ln)

    print("\n5-step optimization playbook (same as audio, fluids edition):")
    print("  1. measure (cProfile / line_profiler) — don't guess")
    print("  2. vectorize / remove Python loops in hot stencils")
    print("  3. cut bytes (float32) and allocations (preallocate buffers)")
    print("  4. better algorithm (FFT Poisson ≫ Jacobi sweeps)")
    print("  5. native code (Numba/Cython/C++) only after 1–4")


# ===========================================================================
# MAIN
# ===========================================================================

def main() -> None:
    os.makedirs(OUT_DIR, exist_ok=True)
    print("High Performance Python for Fluid Dynamics")
    print("Timing varies by machine — the RATIOS are the lesson.\n")

    chapter("1. Discrete fluids in pure Python")
    demo_discrete_fluids()

    chapter("2. The cost of pure Python")
    demo_interpreter_tax()

    chapter("3. Vectorization with NumPy")
    demo_vectorization()

    chapter("4. dtypes & memory")
    demo_dtypes()

    chapter("5. Views, copies, in-place")
    demo_views_and_copies()

    chapter("6. A fluids toolbox")
    demo_fluids_toolbox()

    chapter("7. Elliptic solvers (Poisson)")
    demo_elliptic_solvers()

    chapter("8. The FFT (spectral Poisson)")
    demo_fft_poisson()

    chapter("9. Projection method")
    demo_projection()

    chapter("10. Simulation pipeline (lid-driven cavity)")
    demo_pipeline()

    chapter("11. Timestep budgets")
    demo_timestep_budget()

    chapter("12. Profiling & the playbook")
    demo_profiling()

    print("\n=============================================")
    print(" done — see out/ for CSV and PPM fields")
    print("=============================================")


if __name__ == "__main__":
    main()
