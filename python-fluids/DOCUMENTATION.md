# High Performance Python for Fluid Dynamics — Documentation

This course teaches **high performance Python** using **fluid dynamics**
as the domain. Every chapter updates real grid fields, *measures* it, and
then makes it faster. Fluids are ideal for performance work: grids are
large multi-dimensional arrays, stencils are array math, stability (CFL /
diffusive limits) imposes a hard step-size deadline, and the results are
visible — every run writes CSV/PPM files into `out/`.

The performance ladder the course climbs:

```
pure Python  →  vectorized NumPy  →  right dtypes, no copies
             →  better ALGORITHMS (FFT)  →  timestep budgets
             →  profiling; native code only as the last step
```

```bash
make deps   # pip install numpy (the only dependency)
make run    # run all 12 chapters, writes out/*
make clean  # remove generated fields
```

Timing numbers below are from one representative run; they vary by machine.
**The ratios are the lesson, not the absolute numbers.**

---

## Table of Contents

1. [Discrete fluids in pure Python](#chapter-1--discrete-fluids-in-pure-python)
2. [The cost of pure Python](#chapter-2--the-cost-of-pure-python)
3. [Vectorization with NumPy](#chapter-3--vectorization-with-numpy)
4. [dtypes & memory](#chapter-4--dtypes--memory)
5. [Views, copies, in-place](#chapter-5--views-copies-in-place)
6. [A fluids toolbox](#chapter-6--a-fluids-toolbox)
7. [Elliptic solvers](#chapter-7--elliptic-solvers-poisson)
8. [The FFT: spectral Poisson](#chapter-8--the-fft-spectral-poisson)
9. [Projection method](#chapter-9--projection-method)
10. [Simulation pipeline](#chapter-10--simulation-pipeline-lid-driven-cavity)
11. [Timestep budgets](#chapter-11--timestep-budgets)
12. [Profiling & the playbook](#chapter-12--profiling--the-playbook)

---

## Chapter 1 — Discrete Fluids in Pure Python

### `demo_discrete_fluids()`

The domain fundamentals, deliberately in pure Python:

- **Grid**: continuous field `q(x,t)` → samples `q[i]` at `x = i·Δx`.
- **Linear advection** `∂q/∂t + c ∂q/∂x = 0` with **first-order upwind**
  for `c > 0`: `q_i ← q_i − CFL·(q_i − q_{i−1})` where `CFL = c·Δt/Δx`.
- **CFL condition**: `|c|·Δt/Δx ≤ 1` or the scheme is unstable / non-monotone.
- **Check**: a Gaussian bump centered at 0.3 should arrive near
  `0.3 + c·T` after time `T` (numerical diffusion shifts/spreads it a bit).

Writes `out/advection1d.csv`. Later chapters use NumPy; this chapter is
why the speedups feel earned.

---

## Chapter 2 — The Cost of Pure Python

### `demo_interpreter_tax()`

A **5-point Laplacian** on a list-of-lists grid. Every cell pays bytecode
overhead (type dispatch, refcounts, boxed floats). A 128² field is only
16k cells — tiny for CFD — and already expensive in pure Python.

### `bench(fn, repeat=3)`

Same tool as the audio course: run three times, report the **minimum**.
Min-of-N filters scheduler noise; it is what `timeit` does.

The fix is not “write faster Python loops” — it is to pay interpreter
overhead **once per array** instead of once per cell.

---

## Chapter 3 — Vectorization with NumPy

### `demo_vectorization()`

Replace nested `for i / for j` with **slice arithmetic**:

```python
lap = f[2:,1:-1] + f[:-2,1:-1] + f[1:-1,2:] + f[1:-1,:-2] - 4*f[1:-1,1:-1]
```

One contiguous C loop (often SIMD). On a small grid the pure-Python double
loop is typically **tens to hundreds of times** slower. The rest of the
course stays in this style.

---

## Chapter 4 — dtypes & Memory

### `demo_dtypes()`

CFD grids are often **memory-bandwidth bound**. float64 is NumPy’s default;
float32 halves traffic. Production codes often keep float64 for elliptic
residuals / pressure and use float32 for transport where acceptable.

Trap: writing `4.0` (Python float → float64) next to a float32 array
**promotes** the whole expression back to float64. Use `np.float32(4.0)`.

---

## Chapter 5 — Views, Copies, In-Place

### `demo_views_and_copies()`

- Slices like `a[0, :]` are **views** (halo / ghost rows) — writes alias.
- `a * 0.5` **allocates**; `a *= 0.5` or `np.multiply(..., out=buf)` reuse memory.
- **Double-buffering**: allocate `u` and `u_new` once; swap each step.
  Allocating inside the time loop means GC pauses and cold caches.

---

## Chapter 6 — A Fluids Toolbox

### `diffuse_ftcs(q, nu, dt, dx)`

Forward-Time Centered-Space diffusion. Stability in 2D requires
`ν·Δt/Δx² ≤ 1/4`. Neumann edges are implemented by copying the adjacent
interior row/column (teaching BC; Dirichlet would zero the walls).

### `advect_upwind(q, u, v, dt, dx)`

Constant-velocity first-order upwind (assumes `u,v ≥ 0`). Diffusive but
stable under a CFL limit — the classic “get something moving” scheme.

### `demo_fluids_toolbox()`

Diffuses a `(1,1)` sine mode and compares to the analytic decay
`exp(−ν·2π²t)` on the interior (edges use Neumann, so expect a small
relative error). Advects a Gaussian blob and writes PPM snapshots.

---

## Chapter 7 — Elliptic Solvers (Poisson)

Incompressible flow needs a **pressure Poisson** solve each step:
`∇²p = rhs`.

### `jacobi_poisson` / `gauss_seidel_poisson`

Manufactured solution `p = sin(πx)sin(πy)` with
`rhs = ∇²p = −2π² p` and Dirichlet walls.

- **Jacobi** updates from a frozen neighbor field → trivial to vectorize.
- **Gauss–Seidel** uses new neighbors immediately → fewer iterations, but
  a sequential Python double loop reintroduces the interpreter tax.

Lesson: convergence rate ≠ wall-clock. Constants and vectorizability matter
as much as big-O until you change the algorithm (Chapter 8).

---

## Chapter 8 — The FFT: Spectral Poisson

### `spectral_poisson_periodic(rhs, dx)`

On a periodic box, Fourier modes are eigenfunctions of `∇²`:

`∇²p = rhs`  ⇒  `−|k|² p̂ = rhŝ`  ⇒  `p̂ = −rhŝ / |k|²` (zero mean mode).

One `rfft2` + multiply + `irfft2` replaces hundreds of Jacobi sweeps and
hits near machine precision on smooth eigenmodes. This is the course’s
biggest **algorithmic** win — the fluids analogue of replacing an O(n²)
DFT with an FFT in the audio course.

Writes `out/poisson_fft.ppm`.

---

## Chapter 9 — Projection Method

### `project_spectral(u, v, dx)` / `divergence`

Chorin / Stam idea: given any velocity, solve `∇²p = ∇·u`, then
`u ← u − ∇p`. The result is (discretely) divergence-free — the constraint
that makes the flow **incompressible**.

Demo starts from a deliberately divergent field and shows `max|∇·u|`
collapse after one projection.

---

## Chapter 10 — Simulation Pipeline (Lid-Driven Cavity)

### `lid_driven_step` / `demo_pipeline`

A compact **fractional-step** cavity:

1. Viscous FTCS + first-order self-advection → intermediate `u*`
2. No-slip walls; top lid `u = U`
3. Jacobi pressure Poisson with `rhs = (∇·u*)/Δt`
4. Correct `u ← u* − Δt ∇p`; re-apply BCs

This is a *teaching* cavity (coarse grid, heavy numerical diffusion), not
a publication-grade Ghia benchmark — but it produces a sensible speed
field and a mid-plane profile.

Writes `out/cavity_speed.ppm` and `out/cavity_u_mid.csv`.

---

## Chapter 11 — Timestep Budgets

### `demo_timestep_budget()`

Physical stability picks `Δt` (min of advective CFL and diffusive limits).
If you want **real-time** visualization or hardware-in-the-loop, each step
must finish in under `Δt` wall-clock seconds. Measure ms/step, compute
**headroom** = deadline / cost. Headroom &lt; 1 → miss the deadline: coarsen,
raise viscosity, reuse FFT plans, cut Poisson iterations, or go native.

Same discipline as the audio course’s callback deadline — different units.

---

## Chapter 12 — Profiling & the Playbook

### `demo_profiling()`

`cProfile` on the cavity step. Expect Poisson / stencil helpers near the
top. Then the **5-step playbook**:

1. **Measure** — cProfile / line_profiler; don’t guess
2. **Vectorize** — kill Python loops in hot stencils
3. **Bytes & allocs** — float32 where safe; preallocate buffers
4. **Better algorithm** — FFT / multigrid ≫ Jacobi for elliptic solves
5. **Native last** — Numba, Cython, or C++ only after 1–4

---

## Production pointers

| Need | Look at |
|---|---|
| FFTs / SciPy stack | `numpy.fft`, `scipy.fft`, `scipy.sparse.linalg` |
| JIT stencils | [Numba](https://numba.pydata.org/) |
| Differentiable / GPU CFD | [JAX](https://jax.readthedocs.io/), [Phiflow](https://github.com/tum-pbs/PhiFlow) |
| Full NS / unstructured | OpenFOAM, [PyFR](https://www.pyfr.org/), FEniCS |
| Same HP-Python ladder (audio) | [`../python-audio/`](../python-audio/) |

---

## Stability cheat-sheet

| Scheme | Limit (order-of-magnitude) |
|---|---|
| 1D upwind advection | `CFL = \|c\|Δt/Δx ≤ 1` |
| 2D FTCS diffusion | `ν Δt / Δx² ≤ 1/4` |
| Combined | `Δt ≤ min(Δt_adv, Δt_diff)` |
