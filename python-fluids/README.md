# High Performance Python for Fluid Dynamics

A course that teaches **high performance Python** through **fluid
dynamics**: every chapter updates real grid fields, *measures* how long
it takes, and then makes it faster. No external mesh files needed — all
flows are set up in code — and every run writes scalar fields into `out/`
(CSV profiles and PPM images you can open in any viewer).

The performance ladder:

```
pure Python  →  vectorized NumPy  →  right dtypes, no copies
             →  better ALGORITHMS (FFT Poisson)  →  timestep budgets
             →  profiling; native code only as the last resort
```

## Chapters

1. **Discrete fluids in pure Python** — grids, CFL, 1D upwind advection
2. **The cost of pure Python** — measuring the interpreter tax on stencils
3. **Vectorization with NumPy** — the ~50–100× free speedup
4. **dtypes & memory** — float64/float32, bandwidth on large grids
5. **Views, copies, in-place** — halos, double-buffering without alloc
6. **A fluids toolbox** — FTCS diffusion, upwind advection, sources
7. **Elliptic solvers** — Jacobi vs Gauss–Seidel for Poisson (pressure)
8. **The FFT** — spectral Poisson: iterations → one transform
9. **Projection method** — make ∇·u = 0 (incompressible NS lite)
10. **A simulation pipeline** — lid-driven cavity + frames/profiles
11. **Timestep budgets** — the CFL wall-clock deadline (real-time headroom)
12. **Profiling & the playbook** — cProfile the hotspot, then the 5-step order

Every function is documented in [DOCUMENTATION.md](DOCUMENTATION.md), with
the concepts, the numbers to expect, and pointers to the production
ecosystem (scipy.fft, JAX, PyFR, OpenFOAM bindings, Numba).

## Running

Requires Python 3.10+ and NumPy (the only dependency).

```bash
make deps   # python3 -m pip install -r requirements.txt
make run    # run all 12 chapters
make clean  # remove generated fields and caches
```

Timing output varies by machine — **the ratios are the lesson**, not the
absolute numbers.

## See your work

Each run writes to `out/`:

| File | Made by | What you see |
|---|---|---|
| `advection1d.csv` | Ch 1 | 1D Gaussian after upwind advection |
| `diffuse.ppm` / `advect.ppm` | Ch 6 | 2D diffusion / advected blob |
| `poisson_fft.ppm` | Ch 8 | spectral Poisson solution |
| `cavity_speed.ppm` | Ch 10 | lid-driven cavity speed field |
| `cavity_u_mid.csv` | Ch 10 | mid-plane `u(y)` profile |

PPM files are raw grayscale; open with Preview, ImageMagick (`display`),
or convert: `magick out/cavity_speed.ppm out/cavity_speed.png`.

## Layout

| File | Purpose |
|---|---|
| `main.py` | All 12 chapters, one runnable file |
| `DOCUMENTATION.md` | Written explanation of every chapter and function |
| `Makefile` | `run`, `deps`, `clean` |
| `requirements.txt` | numpy |

## Sibling course

[`python-audio/`](../python-audio/) climbs the **same performance ladder**
with audio as the domain (WAV instead of PPM). Read both to see how
vectorization, dtypes, FFTs, and profiling transfer across fields.
