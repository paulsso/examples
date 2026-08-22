# Advanced Computational Physics in C — Course Text

This course progresses from **linear systems** through **coupled nonlinear
systems** into **chaos**, and finishes with simulations in **information
theory**, **thermodynamics**, and **quantum mechanics**. C is the engine
(`main.c`, zero dependencies beyond libm); small supporting scripts do what
scripts do best — `run.sh` (bash) orchestrates, `plot.py` (stdlib Python)
renders the data as terminal ASCII plots.

**The course's discipline: every chapter verifies itself against exact
analytic results.** The program is a physics test suite — 34 PASS/FAIL
checks, from machine epsilon to Feigenbaum's constant to the unitarity of
quantum evolution — ending in a scorecard. All randomness is seeded, so
every run reproduces the same physics to the digit. Natural units
(ħ = m = 1) throughout.

```bash
make run     # all 12 chapters, ~0.3 s, 34/34 checks
./run.sh     # the same + ASCII renders of the bifurcation diagram,
             # Lorenz attractor, Ising transition, and tunneling packet
```

---

## Table of Contents

**Part I — Linear**
1. [Floating point & finite differences](#chapter-1--floating-point--finite-differences)
2. [Linear algebra: Ax=b and eigenvalues](#chapter-2--linear-algebra-axb-and-eigenvalues)
3. [ODE integrators: Euler vs RK4 vs Verlet](#chapter-3--ode-integrators-euler-vs-rk4-vs-verlet)
4. [Normal modes of a coupled chain](#chapter-4--normal-modes-of-a-coupled-chain)

**Part II — Nonlinear & Chaos**
5. [The nonlinear pendulum](#chapter-5--the-nonlinear-pendulum)
6. [Coupled nonlinear systems](#chapter-6--coupled-nonlinear-systems)
7. [The route to chaos: the logistic map](#chapter-7--the-route-to-chaos-the-logistic-map)
8. [Continuous chaos: the Lorenz system](#chapter-8--continuous-chaos-the-lorenz-system)

**Part III — Information, Heat, Quanta**
9. [Information theory](#chapter-9--information-theory)
10. [Thermodynamics: the 2D Ising model](#chapter-10--thermodynamics-the-2d-ising-model)
11. [Quantum stationary states](#chapter-11--quantum-stationary-states)
12. [Quantum dynamics: tunneling](#chapter-12--quantum-dynamics-tunneling)

---

## Chapter 1 — Floating Point & Finite Differences

Every simulation downstream rests on knowing what a `double` can do.

- **`machine_epsilon()`** computes ε by halving until `1 + ε/2 == 1`, and
  the check confirms it equals 2⁻⁵² — IEEE-754 double, ~16 significant
  digits. Every physics number in this course carries that noise floor.
- **Catastrophic cancellation**: `(1 + 1e-15) - 1` comes out 11% wrong —
  subtracting nearly equal numbers destroys precision, which shapes how
  formulas must be arranged (the reason Ch2 pivots, and why variance is
  never computed as ⟨E²⟩−⟨E⟩² without care).
- **Finite differences, verified empirically**: the forward difference of
  sin at x=1 has error ∝ h (halving h halves the error — measured ratio
  2.00), the central difference ∝ h² (measured ratio 4.00). *Measuring
  the convergence order* is the standard sanity check for any numerical
  scheme, and the course's first habit.

## Chapter 2 — Linear Algebra: Ax=b and Eigenvalues

The two workhorses, both reused by later chapters:

- **`gauss_solve`** — Gaussian elimination with **partial pivoting**
  (choose the largest remaining pivot; without it, small pivots amplify
  rounding into garbage — Ch1's lesson applied). Verified by building
  b = A·x_true and demanding x back at machine precision (measured
  1.1×10⁻¹⁶).
- **`jacobi_eigenvalues`** — the cyclic Jacobi rotation method for
  symmetric matrices: repeatedly zero off-diagonal elements with plane
  rotations; the diagonal converges to the spectrum. Verified on
  tridiag(−1,2,−1) whose exact eigenvalues are 2−√2, 2, 2+√2.

The physics motivation is the punchline of Part I: *linear physics IS
eigenvalue analysis* — Ch4 (normal modes) and Ch11 (quantum states) are
both "build the matrix, call Jacobi".

## Chapter 3 — ODE Integrators: Euler vs RK4 vs Verlet

The harmonic oscillator x″ = −x has exact energy, making it the perfect
integrator test bench. 100 periods at dt = 0.02:

| Method | Order | Energy error after 100 T | Character |
|---|---|---|---|
| Euler | O(h) | 2.9×10⁵ (×!) | error compounds exponentially — unusable |
| RK4 | O(h⁴) | 2.8×10⁻⁸ | tiny but **one-directional drift** |
| velocity Verlet | O(h²) | 1.0×10⁻⁴ worst-case, **bounded** | symplectic |

The deep lesson is the third row: Verlet is *less accurate per step* than
RK4 yet its energy error **never grows** — it preserves the symplectic
structure of Hamiltonian mechanics, so the energy oscillates instead of
drifting. That is why molecular dynamics and orbital mechanics run Verlet
for billions of steps while RK4 is the general-purpose workhorse
everywhere else (and is used for the non-Hamiltonian systems of
Chapters 6–8).

## Chapter 4 — Normal Modes of a Coupled Chain

N unit masses joined by unit springs between fixed walls: the equations of
motion couple every mass to its neighbors, but small oscillations decouple
into **normal modes** with frequencies ω_k = 2 sin(kπ/(2(N+1))). The
stiffness matrix is exactly Chapter 2's tridiag(−1,2,−1); its eigenvalues
are ω². All five computed frequencies match theory to 10⁻⁹.

This chapter closes Part I with its moral: a linear coupled system, no
matter how tangled, is a change of basis away from independent
oscillators. Everything after this chapter is about what happens when
that stops being true.

## Chapter 5 — The Nonlinear Pendulum

θ″ = −sin θ: the first honest nonlinearity. The period now **depends on
amplitude**: T = 4·K(sin(θ₀/2)), with K the complete elliptic integral of
the first kind — computed here by the **arithmetic-geometric mean** (AGM),
a quadratically-convergent jewel of classical numerics (15 digits in ~5
iterations).

The verification integrates the pendulum with RK4 from rest, times the
fall to θ = 0 (a quarter period, with linear interpolation at the
crossing), and compares: simulation matches the elliptic result to 0.01%
at all four amplitudes. The physics: T/T₀ is 1.001 at θ₀ = 0.1 but
**2.57 at θ₀ = 3.0** — nonlinearity is not a small correction, and no
superposition principle survives it.

## Chapter 6 — Coupled Nonlinear Systems

Two canonical ecosystems:

- **Lotka–Volterra predator–prey** (x′ = x−xy, y′ = xy−y): populations
  cycle endlessly. The system hides a conserved quantity
  V = x − ln x + y − ln y, and its drift under RK4 — 3×10⁻¹⁵ over the
  whole run — validates the integration. *Finding and monitoring
  invariants is how you trust a nonlinear simulation* (there is no
  analytic solution to compare against).
- **Van der Pol** (x″ = μ(1−x²)x′ − x): nonlinear damping pumps small
  oscillations and drains large ones, creating a **limit cycle** — an
  attracting periodic orbit that linear systems *cannot possess* (linear
  theory offers only decay, growth, or neutral centers). Verified: starts
  at x₀ = 0.01 and x₀ = 6.0 converge to the *same* amplitude 2.020.

Attractors — this chapter's limit cycle, next chapters' stranger ones —
are the organizing concept of nonlinear dynamics.

## Chapter 7 — The Route to Chaos: the Logistic Map

x_{n+1} = r·x_n(1−x_n): one line containing a universality class. Three
quantitative results, all machine-verified:

- **The bifurcation diagram** (out/bifurcation.dat, rendered by
  `plot.py`): the fixed point splits into a 2-cycle, 4-cycle, 8-cycle…
  then the chaotic sea, with periodic windows inside it.
- **Feigenbaum's δ**: the doubling parameters converge geometrically, and
  the ratio is *universal* — the same 4.6692… for every one-hump map, a
  fact that earned its own corner of mathematical physics. The course
  computes the **superstable parameters** R₁, R₂, R₃ (where the cycle
  contains x = ½) by bisection on f^(2ⁿ)(½) = ½ and gets
  δ = (R₂−R₁)/(R₃−R₂) = **4.681** (limit: 4.669; the first three
  doublings are this close).
- **Lyapunov exponents** λ = ⟨ln|f′(x)|⟩: measured λ(3.2) = −0.92
  (periodic: perturbations die), λ(3.9) = +0.50 (chaotic: perturbations
  explode), and at r = 4 the map is exactly conjugate to a bit shift, so
  λ = ln 2 *analytically* — the simulation delivers **0.693146 vs
  0.693147**. That number is the bridge to Chapter 9.

## Chapter 8 — Continuous Chaos: the Lorenz System

Lorenz's 1963 convection model (σ=10, ρ=28, β=8/3) — three coupled
nonlinear ODEs, integrated with Ch3's RK4:

- **The butterfly effect, watched live**: twin trajectories starting
  10⁻⁸ apart diverge to order-1 separation by t ≈ 28. Determinism
  without predictability.
- **The largest Lyapunov exponent** by **Benettin renormalization**
  (evolve a perturbed copy, log the stretch, renormalize the separation
  every step): λ = 0.898 against the literature value 0.906.
- **The prediction horizon**: t ≈ ln(tolerance/error)/λ ≈ 21 for these
  numbers — the honest formulation of chaos: *predictable, but at a
  price that grows exponentially with the horizon*. Ten times better
  initial data buys only ~2.5 more time units. This is why weather
  forecasts have a wall.

The attractor itself (out/lorenz.dat) is the third kind of attractor:
not a point, not a cycle — a **strange attractor**, on which the system
wanders forever without repeating.

## Chapter 9 — Information Theory

Shannon entropy H = −Σ p log₂ p is the exact price of information, and
this chapter connects it to the physics:

- **Closed-form checks**: H(fair coin) = 1 bit exactly; H(90/10) =
  0.469 bits; mutual information through a 10%-noise binary channel
  matches the analytic 1 − H₂(0.1) = 0.531 bits to 10⁻⁹.
- **Chaos as an information source**: read the logistic map through
  symbolic dynamics (emit 0 if x < ½, else 1) and measure the block
  entropy rate. At r = 4: **1.000 bits per iteration** — the map
  *manufactures* one bit of new information per step, which is exactly
  its Lyapunov exponent ln 2 expressed in bits. At r = 3.2 (periodic):
  0.000 bits — nothing new ever happens. This is **Pesin's identity**
  (entropy production = sum of positive Lyapunov exponents) made
  tangible: *sensitivity to initial conditions and information
  generation are the same phenomenon*. Chapter 7's λ **is** an
  information rate.

## Chapter 10 — Thermodynamics: the 2D Ising Model

L×L spins, E = −Σ s_i s_j over neighbors, sampled by **Metropolis Monte
Carlo**: flip a random spin with probability min(1, e^(−ΔE/T)). That
three-line rule provably samples the Boltzmann distribution — statistical
mechanics as an algorithm.

The measured physics (16×16, seeded RNG, out/ising.dat):

- **Spontaneous magnetization** |M| = 0.986 per spin at T = 1.5 —
  long-range order with no external field;
- **disorder** |M| = 0.124 at T = 3.5;
- **the specific-heat peak at T = 2.25**, against Onsager's exact
  Tc = 2/ln(1+√2) = 2.2692 — the fingerprint of the phase transition,
  on a lattice of just 256 spins.

The conceptual payload: **emergence**. No spin knows about temperature or
magnets; the transition appears only in the collective. And the method's
fine print is teaching material too: the ⟨E²⟩−⟨E⟩² form of the specific
heat is exactly the cancellation-prone expression Chapter 1 warned about,
and near Tc, Metropolis suffers critical slowing-down (the reason cluster
algorithms like Wolff exist — a natural follow-on exercise).

## Chapter 11 — Quantum Stationary States

Discretize −ψ″/2 + Vψ = Eψ on a grid and the time-independent Schrödinger
equation **is a symmetric eigenvalue problem** — solved by the very
Jacobi routine written in Chapter 2:

- **Particle in a box**: computed E₁, E₂, E₃ match n²π²/2 to better
  than 1% (the residual is the O(h²) discretization error of Ch1 —
  every chapter's error has a known origin).
- **Harmonic oscillator** (V = x²/2 on [−8,8]): the ladder
  E_n = n + ½ emerges — 0.4997, 1.4985, 2.4960, 3.4923 — evenly spaced.

Quantization is not imposed anywhere: it falls out of the boundary
conditions and the matrix. And the even spacing of the oscillator ladder
is *why* light of one frequency comes in identical photons — the spectrum
of the field's oscillation modes.

## Chapter 12 — Quantum Dynamics: Tunneling

The time-dependent equation, integrated by **Crank–Nicolson**:

```
(I + i·dt·H/2) ψ_{n+1} = (I − i·dt·H/2) ψ_n
```

— implicit, unconditionally stable, and **unitary**: it conserves total
probability *exactly* (up to roundoff), which the run verifies to 10⁻¹²
over 4000 steps. Each step solves one complex tridiagonal system by the
Thomas algorithm — O(N), so the whole evolution is instant.

The experiment: a Gaussian wave packet with energy E = 2 hits a barrier
with V₀ = 2 — **exactly its own energy**. Classically the outcome is
certain reflection. The quantum result: the packet **splits** —
T = 0.457 transmitted, R = 0.543 reflected, T + R = 1 to 10⁻⁶ (for a
plane wave at E = V₀ the analytic transmission is 1/(1+(k₀w/2)²) = 0.500;
the packet's energy spread accounts for the shift). The final |ψ|² in
out/tunnel.dat shows two humps with the barrier between them: one
particle, probability flowed *through* the classically forbidden region.

---

## The supporting scripts

- **`run.sh`** (bash) — build, run, render: the orchestration layer.
- **`plot.py`** (Python, stdlib only) — a density-scatter ASCII renderer
  for the .dat files. The bifurcation cascade, the Lorenz butterfly, the
  Ising transition and the split wave packet are all clearly visible in
  a terminal. The division of labor is the course's scripting
  philosophy: **C for the numbers, scripts for everything around them.**

## Numbers at a glance (one representative run)

| Quantity | Computed | Exact/literature |
|---|---|---|
| Machine epsilon | 2.220×10⁻¹⁶ | 2⁻⁵² |
| tridiag eigenvalues | 2±√2, 2 to 10⁻⁹ | exact |
| Pendulum T(θ₀=3) | 16.1555 | 4K(sin 1.5) = 16.1555 |
| Feigenbaum δ | 4.681 | 4.669 (n→∞) |
| λ logistic, r=4 | 0.693146 | ln 2 = 0.693147 |
| λ Lorenz | 0.898 | ≈0.906 |
| Entropy rate, r=4 | 1.000 bits/step | 1 (conjugacy to bit shift) |
| Ising C-peak | T = 2.25 | Tc = 2.2692 (Onsager) |
| Oscillator E₀ | 0.49969 | 0.5 |
| CN norm drift | < 10⁻¹² | 0 (unitarity) |
| Tunneling T at E=V₀ | 0.457 | 0.5 (plane wave) |

## Suggested Course Progression

| Stage | Chapters | Exercises to assign |
|---|---|---|
| Foundations | 1–4 | Richardson extrapolation on Ch1's derivatives; add eigenVECTORS to Jacobi and plot the mode shapes; leapfrog vs velocity Verlet |
| Nonlinear | 5–8 | The damped driven pendulum's chaotic regime; delay-embed the Lorenz x(t) series and reconstruct the attractor; period-3 window and intermittency at r ≈ 3.8284 |
| Statistical | 9–10 | Compute the full λ(r) curve and overlay the bifurcation diagram; Wolff cluster updates vs Metropolis near Tc; finite-size scaling of the C peak (L = 8, 16, 32) |
| Quantum | 11–12 | Anharmonic oscillator V = x²/2 + λx⁴ (perturbation theory check); double-well tunneling splitting; two-slit interference with the CN propagator |
