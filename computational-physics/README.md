# Advanced Computational Physics in C

An advanced course progressing from **linear systems** through **coupled
nonlinear systems** into **chaos**, finishing with simulations in
**information theory**, **thermodynamics**, and **quantum mechanics**.
C is the engine; bash and Python provide the supporting scripts
(orchestration and ASCII plotting). The course text is
[DOCUMENTATION.md](DOCUMENTATION.md).

**The discipline: every chapter verifies itself against exact analytic
results** — the program is a physics test suite with 34 PASS/FAIL checks
and a final scorecard, deterministic on every run (seeded RNG).

```bash
make run     # all 12 chapters in ~0.3 s -> scorecard: 34/34 checks passed
./run.sh     # the same, plus ASCII renders of the data
make clean
```

## The progression

| Part | Ch | Topic | Verified against |
|---|---|---|---|
| **Linear** | 1 | floating point, finite differences | ε = 2⁻⁵², error-order ratios 2 and 4 |
| | 2 | Gaussian elimination, Jacobi eigenvalues | exact spectrum of tridiag(−1,2,−1) |
| | 3 | Euler vs RK4 vs symplectic Verlet | exact oscillator energy over 100 periods |
| | 4 | normal modes of a mass-spring chain | ω_k = 2 sin(kπ/(2(N+1))) |
| **Nonlinear & chaos** | 5 | the full pendulum | elliptic integral via AGM, to 0.01% |
| | 6 | Lotka–Volterra, Van der Pol limit cycle | conserved V to 10⁻¹⁵; amplitude ≈ 2 |
| | 7 | logistic map: bifurcation, Feigenbaum, Lyapunov | δ = 4.68 vs 4.669; **λ(4) = ln 2** |
| | 8 | Lorenz: butterfly effect, Benettin λ | λ = 0.898 vs 0.906 |
| **Information, heat, quanta** | 9 | entropy, mutual information, chaos as a bit source | 1 − H₂(0.1); **1.000 bits/step at r = 4** (Pesin) |
| | 10 | 2D Ising, Metropolis Monte Carlo | phase transition; C peak vs Onsager's Tc = 2.2692 |
| | 11 | Schrödinger eigenvalues (reusing Ch2's Jacobi!) | E_n = n²π²/2; **E_n = n + ½** |
| | 12 | Crank–Nicolson tunneling | unitarity to 10⁻¹²; T ≈ 0.5 at E = V₀ |

## See the physics

`./run.sh` renders the data files as terminal ASCII plots (stdlib Python,
no matplotlib needed): the period-doubling cascade into the chaotic sea,
the Lorenz butterfly, the Ising magnetization collapse at Tc, and the
wave packet split in two by a barrier it classically could not cross.

## Layout

| File | Purpose |
|---|---|
| `main.c` | All 12 chapters, C11 + libm only (~1100 lines) |
| `DOCUMENTATION.md` | The course text: theory, methods, verified numbers |
| `plot.py` | Stdlib-Python ASCII density plotter for `out/*.dat` |
| `run.sh` | bash orchestration: build → run → render |
| `Makefile` | `run`, `plots`, `debug`, `clean` |

Natural units (ħ = m = 1) throughout. Requires `gcc`, `make`, `python3`,
`bash` — nothing else.
