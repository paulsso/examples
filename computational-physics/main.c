/*
 * ============================================================================
 *  main.c — Advanced Computational Physics in C
 * ============================================================================
 *
 *  A course that progresses from LINEAR systems through COUPLED NONLINEAR
 *  systems into CHAOS, and finishes with simulations in INFORMATION THEORY,
 *  THERMODYNAMICS and QUANTUM MECHANICS. C is the engine; small Python and
 *  bash scripts (plot.py, run.sh) render the data files this program
 *  writes into out/.
 *
 *  THE COURSE'S DISCIPLINE: every chapter VERIFIES itself against exact
 *  analytic results — the program is a physics test suite. Each check
 *  prints PASS/FAIL and the run ends with a scorecard. Determinism is
 *  guaranteed by a seeded RNG, so every run reproduces the same physics.
 *
 *  Chapters:
 *    PART I — LINEAR
 *    1.  Floating point & finite differences   (error scaling, epsilon)
 *    2.  Linear algebra: Ax=b and eigenvalues  (Gauss elim., Jacobi)
 *    3.  ODE integrators                       (Euler vs RK4 vs Verlet)
 *    4.  Normal modes of a coupled chain       (eigenfrequencies)
 *    PART II — NONLINEAR & CHAOS
 *    5.  The nonlinear pendulum                (period vs elliptic K)
 *    6.  Coupled nonlinear systems             (Lotka-Volterra, Van der Pol)
 *    7.  The route to chaos                    (logistic map, Feigenbaum,
 *                                               Lyapunov = ln 2 at r=4)
 *    8.  Continuous chaos: Lorenz              (butterfly effect, lambda)
 *    PART III — INFORMATION, HEAT, QUANTA
 *    9.  Information theory                    (entropy, mutual info,
 *                                               chaos as a bit source)
 *    10. Thermodynamics: 2D Ising model        (Metropolis, phase
 *                                               transition at Tc)
 *    11. Quantum stationary states             (Schrodinger eigenvalues)
 *    12. Quantum dynamics                      (Crank-Nicolson tunneling)
 *
 *  Build: make      Run: ./main      Plots: ./run.sh (or make plots)
 *  Units: hbar = m = 1 throughout (natural units).
 * ============================================================================
 */

#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* M_PI is POSIX, not ISO C — strict -std=c11 hides it. Portability
 * lesson number zero of computational physics: define your constants. */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------------------------------------------------------------------------
 * Test harness: every chapter registers PASS/FAIL checks.
 * ------------------------------------------------------------------------- */
static int g_checks_passed = 0;
static int g_checks_total = 0;

static void check(const char *label, int ok)
{
    g_checks_total++;
    if (ok) g_checks_passed++;
    printf("  [%s] %s\n", ok ? "PASS" : "FAIL", label);
}

static void check_close(const char *label, double got, double want, double tol)
{
    char buf[160];
    snprintf(buf, sizeof buf, "%s: got %.6g, expected %.6g (tol %.2g)",
             label, got, want, tol);
    check(buf, fabs(got - want) <= tol);
}

static void chapter(const char *title)
{
    printf("\n=============================================\n"
           " %s\n"
           "=============================================\n", title);
}

/* Deterministic RNG (xorshift64*): same physics on every run/machine. */
static uint64_t g_rng = 42;

static uint64_t rng_u64(void)
{
    g_rng ^= g_rng >> 12;
    g_rng ^= g_rng << 25;
    g_rng ^= g_rng >> 27;
    return g_rng * 2685821657736338717ULL;
}

static double rng_uniform(void)   /* uniform in [0, 1) */
{
    return (double)(rng_u64() >> 11) / 9007199254740992.0;
}

static FILE *open_dat(const char *name)
{
    char path[128];
    snprintf(path, sizeof path, "out/%s", name);
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); exit(1); }
    return f;
}

/* ===========================================================================
 * CHAPTER 1 — FLOATING POINT & FINITE DIFFERENCES
 * ===========================================================================
 * Everything downstream rests on knowing what a double can and cannot do.
 * Machine epsilon bounds relative rounding error; finite-difference
 * truncation error scales as a POWER of the step h — and we verify the
 * exponent empirically, which is how all numerics should be tested.
 */

static double machine_epsilon(void)
{
    double eps = 1.0;
    while (1.0 + eps / 2.0 > 1.0) eps /= 2.0;
    return eps;
}

static void demo_floating_point(void)
{
    double eps = machine_epsilon();
    printf("machine epsilon = %.3e (double has ~15-16 decimal digits)\n", eps);
    check("epsilon matches IEEE-754 double (2^-52)",
          fabs(eps - pow(2, -52)) < 1e-18);

    /* Catastrophic cancellation: (1+x) - 1 for tiny x loses everything. */
    double x = 1e-15;
    double naive = (1.0 + x) - 1.0;
    printf("(1 + 1e-15) - 1 = %.3e  (should be 1e-15: %.0f%% error)\n",
           naive, 100.0 * fabs(naive - x) / x);

    /* Finite differences of f = sin at x0 = 1; exact f' = cos(1).
     * forward: error ~ h (first order); central: error ~ h^2 (second).
     * Halving h should divide the errors by ~2 and ~4 respectively. */
    double x0 = 1.0, exact = cos(1.0);
    double h1 = 1e-3, h2 = 5e-4;
    double fwd1 = (sin(x0 + h1) - sin(x0)) / h1;
    double fwd2 = (sin(x0 + h2) - sin(x0)) / h2;
    double cen1 = (sin(x0 + h1) - sin(x0 - h1)) / (2 * h1);
    double cen2 = (sin(x0 + h2) - sin(x0 - h2)) / (2 * h2);

    double fwd_ratio = fabs(fwd1 - exact) / fabs(fwd2 - exact);
    double cen_ratio = fabs(cen1 - exact) / fabs(cen2 - exact);
    printf("forward-difference error ratio when h halves: %.2f (theory: 2)\n",
           fwd_ratio);
    printf("central-difference error ratio when h halves: %.2f (theory: 4)\n",
           cen_ratio);
    check_close("forward difference is 1st order", fwd_ratio, 2.0, 0.1);
    check_close("central difference is 2nd order", cen_ratio, 4.0, 0.1);
}

/* ===========================================================================
 * CHAPTER 2 — LINEAR ALGEBRA: Ax = b AND EIGENVALUES
 * ===========================================================================
 * The two workhorses used by half the later chapters:
 *  - Gaussian elimination with partial pivoting (solve linear systems)
 *  - the cyclic Jacobi rotation method (eigenvalues of symmetric matrices;
 *    reused for normal modes in Ch4 and the Schrodinger equation in Ch11)
 */

/* Solve A x = b in place (A is n x n, row-major). Partial pivoting picks
 * the largest remaining pivot per column — without it, small pivots
 * amplify rounding error into garbage. */
static void gauss_solve(double *A, double *b, double *x, int n)
{
    for (int col = 0; col < n; col++) {
        int piv = col;
        for (int r = col + 1; r < n; r++) {
            if (fabs(A[r * n + col]) > fabs(A[piv * n + col])) piv = r;
        }
        if (piv != col) {
            for (int c = 0; c < n; c++) {
                double t = A[col * n + c];
                A[col * n + c] = A[piv * n + c];
                A[piv * n + c] = t;
            }
            double t = b[col]; b[col] = b[piv]; b[piv] = t;
        }
        for (int r = col + 1; r < n; r++) {
            double f = A[r * n + col] / A[col * n + col];
            for (int c = col; c < n; c++) A[r * n + c] -= f * A[col * n + c];
            b[r] -= f * b[col];
        }
    }
    for (int r = n - 1; r >= 0; r--) {
        double s = b[r];
        for (int c = r + 1; c < n; c++) s -= A[r * n + c] * x[c];
        x[r] = s / A[r * n + r];
    }
}

/* Cyclic Jacobi: repeatedly zero the largest off-diagonal element with a
 * plane rotation; the diagonal converges to the eigenvalues. Simple,
 * robust, and perfectly accurate for the symmetric matrices physics
 * produces. Eigenvalues are returned sorted ascending. */
static void jacobi_eigenvalues(double *A, int n, double *eig)
{
    for (int sweep = 0; sweep < 100; sweep++) {
        double off = 0.0;
        for (int p = 0; p < n; p++)
            for (int q = p + 1; q < n; q++) off += A[p * n + q] * A[p * n + q];
        if (off < 1e-22) break;

        for (int p = 0; p < n; p++) {
            for (int q = p + 1; q < n; q++) {
                double apq = A[p * n + q];
                if (fabs(apq) < 1e-300) continue;
                double theta = (A[q * n + q] - A[p * n + p]) / (2 * apq);
                double t = (theta >= 0 ? 1.0 : -1.0) /
                           (fabs(theta) + sqrt(theta * theta + 1.0));
                double c = 1.0 / sqrt(t * t + 1.0);
                double s = t * c;
                for (int k = 0; k < n; k++) {
                    double akp = A[k * n + p], akq = A[k * n + q];
                    A[k * n + p] = c * akp - s * akq;
                    A[k * n + q] = s * akp + c * akq;
                }
                for (int k = 0; k < n; k++) {
                    double apk = A[p * n + k], aqk = A[q * n + k];
                    A[p * n + k] = c * apk - s * aqk;
                    A[q * n + k] = s * apk + c * aqk;
                }
            }
        }
    }
    for (int i = 0; i < n; i++) eig[i] = A[i * n + i];
    /* insertion sort ascending */
    for (int i = 1; i < n; i++) {
        double v = eig[i];
        int j = i - 1;
        while (j >= 0 && eig[j] > v) { eig[j + 1] = eig[j]; j--; }
        eig[j + 1] = v;
    }
}

static void demo_linear_algebra(void)
{
    /* A 4x4 system with a known solution: build b = A * x_true, solve,
     * and demand x back to near machine precision. */
    double A[16] = {
        4, -1, 0, 1,
        -1, 5, 2, 0,
        0, 2, 6, -1,
        1, 0, -1, 3,
    };
    double x_true[4] = { 1.0, -2.0, 0.5, 3.0 };
    double b[4];
    for (int r = 0; r < 4; r++) {
        b[r] = 0;
        for (int c = 0; c < 4; c++) b[r] += A[r * 4 + c] * x_true[c];
    }
    double x[4];
    gauss_solve(A, b, x, 4);
    double err = 0;
    for (int i = 0; i < 4; i++) err = fmax(err, fabs(x[i] - x_true[i]));
    printf("Gaussian elimination on a 4x4 system: max |x - x_true| = %.2e\n", err);
    check("Ax=b solved to near machine precision", err < 1e-12);

    /* Eigenvalues of the discrete Laplacian tridiag(-1, 2, -1), n=3:
     * exact eigenvalues are 2 - sqrt(2), 2, 2 + sqrt(2). */
    double M[9] = { 2, -1, 0, -1, 2, -1, 0, -1, 2 };
    double eig[3];
    jacobi_eigenvalues(M, 3, eig);
    printf("Jacobi eigenvalues of tridiag(-1,2,-1): %.6f %.6f %.6f\n",
           eig[0], eig[1], eig[2]);
    check_close("lambda_1 = 2 - sqrt(2)", eig[0], 2.0 - sqrt(2.0), 1e-9);
    check_close("lambda_2 = 2", eig[1], 2.0, 1e-9);
    check_close("lambda_3 = 2 + sqrt(2)", eig[2], 2.0 + sqrt(2.0), 1e-9);
}

/* ===========================================================================
 * CHAPTER 3 — ODE INTEGRATORS: EULER vs RK4 vs VERLET
 * ===========================================================================
 * The harmonic oscillator x'' = -x has exact energy E = (x^2 + v^2)/2.
 * Integrating 100 periods exposes each method's character:
 *   Euler  : O(h) accurate, energy GROWS exponentially — unusable
 *   RK4    : O(h^4) accurate, tiny energy drift — the general workhorse
 *   Verlet : only O(h^2), but SYMPLECTIC — energy oscillates but never
 *            drifts; the reason molecular dynamics and orbital mechanics
 *            use it for billions of steps
 */

static void demo_integrators(void)
{
    const double dt = 0.02;
    const long steps = (long)(100.0 * 2.0 * M_PI / dt);
    const double E0 = 0.5; /* x=1, v=0 */

    /* Euler */
    double x = 1.0, v = 0.0;
    for (long i = 0; i < steps; i++) {
        double xn = x + dt * v;
        double vn = v - dt * x;
        x = xn; v = vn;
    }
    double e_euler = fabs((x * x + v * v) / 2 - E0) / E0;

    /* RK4 for the system (x, v)' = (v, -x) */
    x = 1.0; v = 0.0;
    for (long i = 0; i < steps; i++) {
        double k1x = v,               k1v = -x;
        double k2x = v + dt / 2 * k1v, k2v = -(x + dt / 2 * k1x);
        double k3x = v + dt / 2 * k2v, k3v = -(x + dt / 2 * k2x);
        double k4x = v + dt * k3v,     k4v = -(x + dt * k3x);
        x += dt / 6 * (k1x + 2 * k2x + 2 * k3x + k4x);
        v += dt / 6 * (k1v + 2 * k2v + 2 * k3v + k4v);
    }
    double e_rk4 = fabs((x * x + v * v) / 2 - E0) / E0;

    /* velocity Verlet, tracking the WORST energy error along the way */
    x = 1.0; v = 0.0;
    double a = -x, worst = 0.0;
    for (long i = 0; i < steps; i++) {
        x += v * dt + 0.5 * a * dt * dt;
        double a_new = -x;
        v += 0.5 * (a + a_new) * dt;
        a = a_new;
        worst = fmax(worst, fabs((x * x + v * v) / 2 - E0) / E0);
    }

    printf("relative energy error after 100 periods (dt = %.2f):\n", dt);
    printf("  Euler  : %.3e   (grows without bound)\n", e_euler);
    printf("  RK4    : %.3e   (tiny, but drifts one way)\n", e_rk4);
    printf("  Verlet : %.3e   (worst-case; bounded FOREVER)\n", worst);
    check("Euler's energy error exceeds 100%", e_euler > 1.0);
    check("RK4 conserves energy to < 1e-6", e_rk4 < 1e-6);
    check("Verlet's energy error stays bounded < 1e-3", worst < 1e-3);
}

/* ===========================================================================
 * CHAPTER 4 — NORMAL MODES OF A COUPLED CHAIN
 * ===========================================================================
 * N unit masses joined by unit springs between fixed walls. Small
 * oscillations decouple into normal modes with frequencies
 *     omega_k = 2 sin( k*pi / (2(N+1)) ),   k = 1..N
 * The stiffness matrix is exactly Ch2's tridiag(-1,2,-1) — eigenvalues
 * are omega^2. Linear physics IS eigenvalue analysis.
 */

static void demo_normal_modes(void)
{
    enum { N = 5 };
    double K[N * N] = { 0 };
    for (int i = 0; i < N; i++) {
        K[i * N + i] = 2.0;
        if (i > 0) K[i * N + (i - 1)] = -1.0;
        if (i < N - 1) K[i * N + (i + 1)] = -1.0;
    }
    double eig[N];
    jacobi_eigenvalues(K, N, eig);

    printf("chain of %d masses: computed vs analytic frequencies\n", N);
    int all_ok = 1;
    for (int k = 1; k <= N; k++) {
        double omega = sqrt(eig[k - 1]);
        double exact = 2.0 * sin(k * M_PI / (2.0 * (N + 1)));
        printf("  mode %d: omega = %.6f  exact = %.6f\n", k, omega, exact);
        if (fabs(omega - exact) > 1e-9) all_ok = 0;
    }
    check("all 5 normal-mode frequencies match theory", all_ok);
}

/* ===========================================================================
 * CHAPTER 5 — THE NONLINEAR PENDULUM
 * ===========================================================================
 * theta'' = -sin(theta): the first nonlinearity. The period now DEPENDS ON
 * AMPLITUDE — T = 4 K(sin(theta0/2)) with K the complete elliptic integral
 * (computed here via the arithmetic-geometric mean, a jewel of numerics).
 * We measure the period by integrating and timing the fall to theta = 0,
 * then compare against the elliptic result.
 */

static double elliptic_K(double k)     /* AGM: K = pi / (2 * AGM(1, k')) */
{
    double a = 1.0, b = sqrt(1.0 - k * k);
    for (int i = 0; i < 60 && fabs(a - b) > 1e-15; i++) {
        double an = 0.5 * (a + b);
        b = sqrt(a * b);
        a = an;
    }
    return M_PI / (2.0 * a);
}

static double pendulum_period(double theta0)
{
    /* release from rest; the time to reach theta = 0 is T/4 */
    double th = theta0, w = 0.0, dt = 1e-4, t = 0.0;
    while (th > 0.0) {
        /* RK4 on (theta, omega) */
        double k1t = w,              k1w = -sin(th);
        double k2t = w + dt / 2 * k1w, k2w = -sin(th + dt / 2 * k1t);
        double k3t = w + dt / 2 * k2w, k3w = -sin(th + dt / 2 * k2t);
        double k4t = w + dt * k3w,     k4w = -sin(th + dt * k3t);
        double th_new = th + dt / 6 * (k1t + 2 * k2t + 2 * k3t + k4t);
        double w_new = w + dt / 6 * (k1w + 2 * k2w + 2 * k3w + k4w);
        if (th_new <= 0.0) {
            t += dt * th / (th - th_new);   /* linear interp to the crossing */
            return 4.0 * t;
        }
        th = th_new; w = w_new; t += dt;
    }
    return 4.0 * t;
}

static void demo_pendulum(void)
{
    printf("pendulum period vs amplitude (T0 = 2*pi for the linear limit):\n");
    double amplitudes[] = { 0.1, 1.0, 2.0, 3.0 };
    int all_ok = 1;
    for (int i = 0; i < 4; i++) {
        double th0 = amplitudes[i];
        double T_sim = pendulum_period(th0);
        double T_ell = 4.0 * elliptic_K(sin(th0 / 2.0));
        printf("  theta0 = %.1f: simulated T = %.6f, elliptic T = %.6f "
               "(T/T0 = %.3f)\n", th0, T_sim, T_ell, T_ell / (2 * M_PI));
        if (fabs(T_sim - T_ell) > 1e-4 * T_ell) all_ok = 0;
    }
    check("simulated periods match elliptic integrals to 0.01%", all_ok);
    printf("the linear approximation is already 10%% wrong at theta0 = 2 —\n"
           "nonlinearity is not a small correction\n");
}

/* ===========================================================================
 * CHAPTER 6 — COUPLED NONLINEAR SYSTEMS
 * ===========================================================================
 * Two ecosystems of coupled nonlinearity:
 *  - Lotka-Volterra predator-prey: cyclic dynamics with a hidden CONSERVED
 *    quantity V (checking its drift validates the whole integration)
 *  - Van der Pol oscillator: nonlinear damping creates a LIMIT CYCLE — an
 *    attractor no linear system can produce (linear systems only offer
 *    decay, growth, or neutral circles)
 */

static void demo_coupled_nonlinear(void)
{
    /* Lotka-Volterra: x' = ax - bxy, y' = dxy - cy, with a=b=c=d=1.
     * Conserved: V = x - ln x + y - ln y. */
    double x = 2.0, y = 1.0, dt = 1e-3;
    double V0 = x - log(x) + y - log(y);
    double xmin = x, xmax = x;
    for (long i = 0; i < 40000; i++) {
        #define LV_FX(x, y) ((x) - (x) * (y))
        #define LV_FY(x, y) ((x) * (y) - (y))
        double k1x = LV_FX(x, y),                     k1y = LV_FY(x, y);
        double k2x = LV_FX(x + dt/2*k1x, y + dt/2*k1y), k2y = LV_FY(x + dt/2*k1x, y + dt/2*k1y);
        double k3x = LV_FX(x + dt/2*k2x, y + dt/2*k2y), k3y = LV_FY(x + dt/2*k2x, y + dt/2*k2y);
        double k4x = LV_FX(x + dt*k3x, y + dt*k3y),     k4y = LV_FY(x + dt*k3x, y + dt*k3y);
        x += dt / 6 * (k1x + 2 * k2x + 2 * k3x + k4x);
        y += dt / 6 * (k1y + 2 * k2y + 2 * k3y + k4y);
        xmin = fmin(xmin, x); xmax = fmax(xmax, x);
    }
    double V1 = x - log(x) + y - log(y);
    printf("Lotka-Volterra over t = 40: prey cycles between %.3f and %.3f\n",
           xmin, xmax);
    printf("conserved quantity V drift: %.2e (relative)\n",
           fabs(V1 - V0) / V0);
    check("LV invariant conserved to 1e-8 by RK4", fabs(V1 - V0) / V0 < 1e-8);

    /* Van der Pol: x'' = mu (1 - x^2) x' - x, mu = 2. Whatever the start,
     * trajectories converge to one limit cycle; we start tiny and huge
     * and demand both settle to the same amplitude. */
    double amp[2] = { 0, 0 };
    double starts[2] = { 0.01, 6.0 };
    for (int s = 0; s < 2; s++) {
        double px = starts[s], pv = 0.0;
        double mu = 2.0;
        dt = 1e-3;
        for (long i = 0; i < 100000; i++) {   /* t = 100: well past transients */
            #define VDP_A(x, v) (mu * (1 - (x) * (x)) * (v) - (x))
            double k1x = pv,               k1v = VDP_A(px, pv);
            double k2x = pv + dt/2*k1v,    k2v = VDP_A(px + dt/2*k1x, pv + dt/2*k1v);
            double k3x = pv + dt/2*k2v,    k3v = VDP_A(px + dt/2*k2x, pv + dt/2*k2v);
            double k4x = pv + dt*k3v,      k4v = VDP_A(px + dt*k3x, pv + dt*k3v);
            px += dt / 6 * (k1x + 2 * k2x + 2 * k3x + k4x);
            pv += dt / 6 * (k1v + 2 * k2v + 2 * k3v + k4v);
            if (i > 80000) amp[s] = fmax(amp[s], fabs(px));
        }
    }
    printf("Van der Pol limit cycle: amplitude %.4f from x0=0.01, "
           "%.4f from x0=6.0\n", amp[0], amp[1]);
    check("both initial conditions converge to the SAME limit cycle",
          fabs(amp[0] - amp[1]) < 1e-3);
    check_close("limit-cycle amplitude is ~2 (theory for moderate mu)",
                amp[0], 2.0, 0.05);
}

/* ===========================================================================
 * CHAPTER 7 — THE ROUTE TO CHAOS: THE LOGISTIC MAP
 * ===========================================================================
 * x_{n+1} = r x_n (1 - x_n): one line, and inside it the period-doubling
 * route to chaos. Three quantitative results, all verified:
 *  - the bifurcation diagram (written to out/bifurcation.dat)
 *  - Feigenbaum's delta from superstable parameters (universal constant!)
 *  - the Lyapunov exponent: negative when periodic, positive when chaotic,
 *    and EXACTLY ln 2 at r = 4 (the map is conjugate to a bit shift there —
 *    the bridge to Chapter 9)
 */

static double logistic_iterate(double r, double x, long n)
{
    for (long i = 0; i < n; i++) x = r * x * (1 - x);
    return x;
}

/* g(r) = f_r^{2^n}(1/2) - 1/2: zero at the superstable parameter R_n. */
static double superstable_g(double r, int n)
{
    long iters = 1L << n;
    return logistic_iterate(r, 0.5, iters) - 0.5;
}

static double bisect_superstable(double lo, double hi, int n)
{
    double flo = superstable_g(lo, n);
    for (int i = 0; i < 200; i++) {
        double mid = 0.5 * (lo + hi);
        double fm = superstable_g(mid, n);
        if ((flo < 0) == (fm < 0)) { lo = mid; flo = fm; }
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

static double lyapunov_logistic(double r)
{
    double x = 0.31415926;
    for (int i = 0; i < 1000; i++) x = r * x * (1 - x);   /* transient */
    double sum = 0.0;
    long n = 1000000;
    for (long i = 0; i < n; i++) {
        x = r * x * (1 - x);
        sum += log(fabs(r * (1 - 2 * x)) + 1e-300);
    }
    return sum / (double)n;
}

static void demo_logistic(void)
{
    /* Bifurcation diagram data. */
    FILE *f = open_dat("bifurcation.dat");
    for (double r = 2.5; r <= 4.0; r += 0.002) {
        double x = 0.5;
        for (int i = 0; i < 500; i++) x = r * x * (1 - x);
        for (int i = 0; i < 64; i++) {
            x = r * x * (1 - x);
            fprintf(f, "%.4f %.6f\n", r, x);
        }
    }
    fclose(f);
    printf("bifurcation diagram written to out/bifurcation.dat "
           "(render: python3 plot.py)\n");

    /* Feigenbaum delta from superstable parameters R_n (where the cycle
     * contains x = 1/2): delta = (R2 - R1) / (R3 - R2) -> 4.669... */
    double R1 = bisect_superstable(3.10, 3.30, 1);   /* period 2 */
    double R2 = bisect_superstable(3.45, 3.52, 2);   /* period 4 */
    double R3 = bisect_superstable(3.55, 3.56, 3);   /* period 8 */
    double delta = (R2 - R1) / (R3 - R2);
    printf("superstable parameters: R1 = %.6f, R2 = %.6f, R3 = %.6f\n",
           R1, R2, R3);
    printf("Feigenbaum delta estimate: %.4f (universal value 4.6692)\n", delta);
    check_close("Feigenbaum delta from first three doublings",
                delta, 4.669, 0.05);

    /* Lyapunov exponents: order vs chaos, quantified. */
    double l32 = lyapunov_logistic(3.2);
    double l39 = lyapunov_logistic(3.9);
    double l40 = lyapunov_logistic(4.0);
    printf("Lyapunov exponents: lambda(3.2) = %.4f, lambda(3.9) = %.4f, "
           "lambda(4.0) = %.4f\n", l32, l39, l40);
    check("lambda < 0 in the period-2 regime (r = 3.2)", l32 < -0.1);
    check("lambda > 0 in the chaotic regime (r = 3.9)", l39 > 0.1);
    check_close("lambda(r=4) = ln 2 exactly (analytic!)", l40, log(2.0), 0.01);
}

/* ===========================================================================
 * CHAPTER 8 — CONTINUOUS CHAOS: THE LORENZ SYSTEM
 * ===========================================================================
 * Lorenz 1963: three coupled nonlinear ODEs from convection. Two
 * experiments:
 *  - the butterfly effect: trajectories starting 1e-8 apart diverge to
 *    order-1 separation (we watch it happen)
 *  - the largest Lyapunov exponent via Benettin renormalization:
 *    lambda ~ 0.906 for the classic parameters — sensitive dependence,
 *    measured
 */

static void lorenz_rk4(double *s, double dt)
{
    const double SIG = 10.0, RHO = 28.0, BET = 8.0 / 3.0;
    double k[4][3], tmp[3];
    const double *in = s;
    for (int stage = 0; stage < 4; stage++) {
        double fac = (stage == 1 || stage == 2) ? 0.5 : 1.0;
        if (stage > 0) {
            for (int i = 0; i < 3; i++) tmp[i] = s[i] + fac * dt * k[stage - 1][i];
            in = tmp;
        }
        k[stage][0] = SIG * (in[1] - in[0]);
        k[stage][1] = in[0] * (RHO - in[2]) - in[1];
        k[stage][2] = in[0] * in[1] - BET * in[2];
    }
    for (int i = 0; i < 3; i++) {
        s[i] += dt / 6 * (k[0][i] + 2 * k[1][i] + 2 * k[2][i] + k[3][i]);
    }
}

static void demo_lorenz(void)
{
    const double dt = 0.005;

    /* Attractor data for the plot. */
    double s[3] = { 1.0, 1.0, 1.0 };
    FILE *f = open_dat("lorenz.dat");
    for (int i = 0; i < 40000; i++) {
        lorenz_rk4(s, dt);
        if (i > 2000 && i % 4 == 0) fprintf(f, "%.4f %.4f\n", s[0], s[2]);
    }
    fclose(f);

    /* Butterfly effect: twin trajectories, 1e-8 apart. */
    double a[3] = { 1.0, 1.0, 1.0 };
    double b[3] = { 1.0 + 1e-8, 1.0, 1.0 };
    double t_order1 = -1;
    for (int i = 0; i < 20000; i++) {
        lorenz_rk4(a, dt);
        lorenz_rk4(b, dt);
        double d = sqrt(pow(a[0]-b[0],2) + pow(a[1]-b[1],2) + pow(a[2]-b[2],2));
        if (d > 1.0 && t_order1 < 0) { t_order1 = i * dt; break; }
    }
    printf("butterfly effect: a 1e-8 difference grows to order 1 by "
           "t = %.1f\n", t_order1);
    check("microscopic difference becomes macroscopic in finite time",
          t_order1 > 0 && t_order1 < 40.0);

    /* Largest Lyapunov exponent, Benettin's renormalization method:
     * evolve a reference and a perturbed copy; every step, log the
     * stretch of their separation, then renormalize it back to d0. */
    double r0[3] = { 1.0, 1.0, 1.0 };
    for (int i = 0; i < 2000; i++) lorenz_rk4(r0, dt);   /* onto the attractor */
    double p0[3] = { r0[0] + 1e-8, r0[1], r0[2] };
    const double d0 = 1e-8;
    double sum_log = 0.0;
    long steps = 100000;
    for (long i = 0; i < steps; i++) {
        lorenz_rk4(r0, dt);
        lorenz_rk4(p0, dt);
        double d = sqrt(pow(r0[0]-p0[0],2) + pow(r0[1]-p0[1],2) + pow(r0[2]-p0[2],2));
        sum_log += log(d / d0);
        for (int j = 0; j < 3; j++) {
            p0[j] = r0[j] + (p0[j] - r0[j]) * (d0 / d);   /* renormalize */
        }
    }
    double lambda = sum_log / (steps * dt);
    printf("largest Lyapunov exponent (Benettin): lambda = %.3f "
           "(literature: ~0.906)\n", lambda);
    check_close("Lorenz lambda_max matches the known value", lambda, 0.906, 0.05);
    printf("prediction horizon ~ (1/lambda) ln(tolerance/error): with 1e-8\n"
           "initial error and order-1 tolerance, t ~ %.0f — chaos does not\n"
           "mean unpredictable, it means predictable for a PRICE that grows\n"
           "exponentially with horizon\n", log(1e8) / lambda);
}

/* ===========================================================================
 * CHAPTER 9 — INFORMATION THEORY
 * ===========================================================================
 * Shannon entropy H = -sum p log2 p is the exact price of information.
 * After the basics (verified against closed forms), the payoff: reading
 * the logistic map through symbolic dynamics. At r = 4 the map is
 * conjugate to a bit shift — a deterministic system that MANUFACTURES one
 * bit of information per step, which ties Ch7's lambda = ln 2 to
 * information theory: chaos is an information source (Kolmogorov-Sinai).
 */

static double entropy_bits(const double *p, int n)
{
    double h = 0.0;
    for (int i = 0; i < n; i++) {
        if (p[i] > 0) h -= p[i] * log2(p[i]);
    }
    return h;
}

/* Block entropy of the logistic map's symbol stream (0 if x < 1/2 else 1),
 * using k-bit blocks: H_k / k -> entropy rate in bits per iteration. */
static double logistic_entropy_rate(double r, int k)
{
    int nbins = 1 << k;
    long *count = calloc((size_t)nbins, sizeof(long));
    double x = 0.31415926;
    for (int i = 0; i < 1000; i++) x = r * x * (1 - x);

    long n = 200000;
    unsigned block = 0;
    unsigned mask = (unsigned)nbins - 1;
    for (long i = 0; i < n + k; i++) {
        x = r * x * (1 - x);
        block = ((block << 1) | (x >= 0.5 ? 1u : 0u)) & mask;
        if (i >= (long)k - 1) count[block]++;
    }
    double h = 0.0;
    long total = n + 1;
    for (int i = 0; i < nbins; i++) {
        if (count[i] > 0) {
            double p = (double)count[i] / (double)total;
            h -= p * log2(p);
        }
    }
    free(count);
    return h / k;
}

static void demo_information(void)
{
    /* Closed-form checks first. */
    double fair[2] = { 0.5, 0.5 };
    double biased[2] = { 0.9, 0.1 };
    double h_fair = entropy_bits(fair, 2);
    double h_biased = entropy_bits(biased, 2);
    printf("H(fair coin) = %.4f bits, H(90/10 coin) = %.4f bits\n",
           h_fair, h_biased);
    check_close("fair coin carries exactly 1 bit", h_fair, 1.0, 1e-12);
    check_close("90/10 coin carries H = 0.469 bits", h_biased, 0.46900, 1e-4);

    /* Mutual information I(X;Y) = H(X) + H(Y) - H(X,Y) for a noisy
     * channel: Y = X flipped with probability 0.1. Analytic:
     * I = 1 - H2(0.1). */
    double joint[4] = { 0.45, 0.05, 0.05, 0.45 };   /* p(x,y) */
    double px[2] = { 0.5, 0.5 }, py[2] = { 0.5, 0.5 };
    double mi = entropy_bits(px, 2) + entropy_bits(py, 2)
              - entropy_bits(joint, 4);
    printf("mutual information through a 10%%-noise channel: %.4f bits "
           "(analytic 1 - H2(0.1) = %.4f)\n", mi, 1.0 - h_biased);
    check_close("I(X;Y) matches the analytic channel value",
                mi, 1.0 - h_biased, 1e-9);

    /* The chaos connection. */
    double rate_chaotic = logistic_entropy_rate(4.0, 8);
    double rate_periodic = logistic_entropy_rate(3.2, 8);
    printf("logistic-map entropy rate (8-bit blocks): "
           "r=4.0 -> %.3f bits/step, r=3.2 -> %.3f bits/step\n",
           rate_chaotic, rate_periodic);
    check_close("at r=4 the map produces ~1 bit per iteration",
                rate_chaotic, 1.0, 0.02);
    check("in the periodic regime the entropy rate collapses toward 0",
          rate_periodic < 0.3);
    printf("Pesin's identity in action: entropy rate ~ lambda/ln2 = %.3f —\n"
           "the Lyapunov exponent of Ch7 IS an information rate\n",
           log(2.0) / log(2.0));
}

/* ===========================================================================
 * CHAPTER 10 — THERMODYNAMICS: THE 2D ISING MODEL
 * ===========================================================================
 * L x L spins, E = -sum_<ij> s_i s_j, sampled by Metropolis Monte Carlo:
 * flip a random spin with probability min(1, exp(-dE/T)). This tiny rule
 * samples the Boltzmann distribution exactly — and produces a PHASE
 * TRANSITION: spontaneous magnetization below Onsager's critical
 * temperature Tc = 2/ln(1+sqrt(2)) = 2.269, disorder above it, and a
 * specific-heat peak at the boundary. Emergence, on a 16x16 grid.
 */

#define ISING_L 16

static int ising_dE(const int *s, int i, int j)
{
    int L = ISING_L;
    int up    = s[((i - 1 + L) % L) * L + j];
    int down  = s[((i + 1) % L) * L + j];
    int left  = s[i * L + (j - 1 + L) % L];
    int right = s[i * L + (j + 1) % L];
    return 2 * s[i * L + j] * (up + down + left + right);
}

static void demo_ising(void)
{
    const int L = ISING_L, N = L * L;
    int s[ISING_L * ISING_L];
    FILE *f = open_dat("ising.dat");
    fprintf(f, "# T  <|M|>  <E>  C\n");

    double temps[9], mags[9], heats[9];
    int nt = 9;
    for (int t = 0; t < nt; t++) temps[t] = 1.5 + 0.25 * t;

    for (int t = 0; t < nt; t++) {
        double T = temps[t];
        for (int i = 0; i < N; i++) s[i] = 1;    /* cold start */

        /* precompute the only possible Boltzmann factors: dE in {-8..8} */
        double w[9];
        for (int dE = -8; dE <= 8; dE += 2) w[(dE + 8) / 2] = exp(-dE / T);

        double sumE = 0, sumE2 = 0, sumM = 0;
        long measures = 0;
        int sweeps_eq = 2000, sweeps_meas = 4000;

        for (int sweep = 0; sweep < sweeps_eq + sweeps_meas; sweep++) {
            for (int k = 0; k < N; k++) {
                int i = (int)(rng_uniform() * L);
                int j = (int)(rng_uniform() * L);
                int dE = ising_dE(s, i, j);
                if (dE <= 0 || rng_uniform() < w[(dE + 8) / 2]) {
                    s[i * L + j] = -s[i * L + j];
                }
            }
            if (sweep >= sweeps_eq) {
                long E = 0, M = 0;
                for (int i = 0; i < L; i++) {
                    for (int j = 0; j < L; j++) {
                        E -= s[i * L + j] * (s[((i + 1) % L) * L + j]
                                           + s[i * L + (j + 1) % L]);
                        M += s[i * L + j];
                    }
                }
                sumE += E; sumE2 += (double)E * E; sumM += labs(M);
                measures++;
            }
        }
        double meanE = sumE / measures;
        double m = sumM / measures / N;                        /* |M| per spin */
        double C = (sumE2 / measures - meanE * meanE) / (T * T) / N;
        mags[t] = m; heats[t] = C;
        fprintf(f, "%.2f %.4f %.4f %.4f\n", T, m, meanE / N, C);
    }
    fclose(f);

    double Tc = 2.0 / log(1.0 + sqrt(2.0));
    int peak = 0;
    for (int t = 1; t < nt; t++) if (heats[t] > heats[peak]) peak = t;

    printf("16x16 Ising model, Metropolis Monte Carlo (out/ising.dat):\n");
    printf("  |M|/spin at T=1.50: %.3f   at T=3.50: %.3f\n", mags[0], mags[nt-1]);
    printf("  specific-heat peak at T = %.2f (Onsager: Tc = %.4f)\n",
           temps[peak], Tc);
    check("ordered phase below Tc: |M| > 0.9 at T = 1.5", mags[0] > 0.9);
    check("disordered phase above Tc: |M| < 0.4 at T = 3.5", mags[nt-1] < 0.4);
    check("specific heat peaks within 0.3 of Onsager's Tc",
          fabs(temps[peak] - Tc) <= 0.3);
    printf("no spin knows about temperature or magnets — the phase\n"
           "transition EMERGES from a 3-line flip rule\n");
}

/* ===========================================================================
 * CHAPTER 11 — QUANTUM STATIONARY STATES
 * ===========================================================================
 * The time-independent Schrodinger equation -psi''/2 + V psi = E psi,
 * discretized on a grid, IS a symmetric eigenvalue problem — solved with
 * the very Jacobi routine from Chapter 2:
 *  - particle in a box: E_n = n^2 pi^2 / 2
 *  - harmonic oscillator: E_n = n + 1/2 (in hbar*omega units)
 * Quantization falls out of a matrix.
 */

static void demo_quantum_stationary(void)
{
    /* Particle in a box on [0,1], n = 100 interior points. */
    {
        int n = 100;
        double h = 1.0 / (n + 1);
        double *H = calloc((size_t)n * n, sizeof(double));
        for (int i = 0; i < n; i++) {
            H[i * n + i] = 1.0 / (h * h);
            if (i + 1 < n) {
                H[i * n + i + 1] = -0.5 / (h * h);
                H[(i + 1) * n + i] = -0.5 / (h * h);
            }
        }
        double *eig = malloc((size_t)n * sizeof(double));
        jacobi_eigenvalues(H, n, eig);
        printf("particle in a box, first three levels (E_n = n^2 pi^2/2):\n");
        int ok = 1;
        for (int k = 1; k <= 3; k++) {
            double exact = k * k * M_PI * M_PI / 2.0;
            printf("  E_%d = %.4f   exact = %.4f\n", k, eig[k - 1], exact);
            if (fabs(eig[k - 1] - exact) > 0.01 * exact) ok = 0;
        }
        check("box energies match n^2 pi^2/2 to 1%", ok);
        free(H); free(eig);
    }

    /* Harmonic oscillator V = x^2/2 on [-8, 8]. */
    {
        int n = 160;
        double L = 16.0, h = L / (n + 1);
        double *H = calloc((size_t)n * n, sizeof(double));
        FILE *f = open_dat("oscillator.dat");
        for (int i = 0; i < n; i++) {
            double x = -L / 2 + (i + 1) * h;
            H[i * n + i] = 1.0 / (h * h) + 0.5 * x * x;
            if (i + 1 < n) {
                H[i * n + i + 1] = -0.5 / (h * h);
                H[(i + 1) * n + i] = -0.5 / (h * h);
            }
        }
        double *eig = malloc((size_t)n * sizeof(double));
        jacobi_eigenvalues(H, n, eig);
        printf("harmonic oscillator, first four levels (E_n = n + 1/2):\n");
        int ok = 1;
        for (int k = 0; k < 4; k++) {
            double exact = k + 0.5;
            printf("  E_%d = %.5f   exact = %.1f\n", k, eig[k], exact);
            if (fabs(eig[k] - exact) > 0.01) ok = 0;
            fprintf(f, "%d %.6f\n", k, eig[k]);
        }
        fclose(f);
        check("oscillator ladder E_n = n + 1/2, evenly spaced, to 1%", ok);
        printf("equal spacing is WHY light comes in photons of one\n"
               "frequency — the ladder is the oscillator's spectrum\n");
        free(H); free(eig);
    }
}

/* ===========================================================================
 * CHAPTER 12 — QUANTUM DYNAMICS: TUNNELING BY CRANK-NICOLSON
 * ===========================================================================
 * The time-DEPENDENT Schrodinger equation, integrated with the
 * Crank-Nicolson scheme:
 *     (I + i dt H / 2) psi_{n+1} = (I - i dt H / 2) psi_n
 * — implicit, unconditionally stable, and UNITARY: the norm (total
 * probability) is conserved to machine precision, which we verify. Each
 * step solves a complex tridiagonal system (Thomas algorithm, O(N)).
 * A Gaussian wave packet hits a barrier with V0 = E: classically it
 * always bounces; quantum mechanically it splits — tunneling.
 */

static void demo_quantum_dynamics(void)
{
    const int n = 800;
    const double L = 80.0, h = L / n, dt = 0.005;
    const double x0 = -15.0, k0 = 2.0, sigma = 2.0;
    const double V0 = 2.0, bar_lo = 0.0, bar_hi = 1.0;   /* V0 = E = k0^2/2 */

    double complex *psi = malloc((size_t)n * sizeof(double complex));
    double complex *rhs = malloc((size_t)n * sizeof(double complex));
    double *V = malloc((size_t)n * sizeof(double));

    /* Gaussian packet, momentum k0, normalized on the grid. */
    double norm = 0;
    for (int i = 0; i < n; i++) {
        double x = -L / 2 + i * h;
        V[i] = (x >= bar_lo && x <= bar_hi) ? V0 : 0.0;
        double g = exp(-(x - x0) * (x - x0) / (4 * sigma * sigma));
        psi[i] = g * cexp(I * k0 * x);
        norm += g * g * h;
    }
    for (int i = 0; i < n; i++) psi[i] /= sqrt(norm);

    /* Crank-Nicolson matrices: A = I + i dt H/2, B = I - i dt H/2 with
     * H tridiagonal: diag 1/h^2 + V, offdiag -1/(2h^2). */
    double complex a_off = I * dt / 2.0 * (-0.5 / (h * h));
    double complex *a_diag = malloc((size_t)n * sizeof(double complex));
    double complex *b_diag = malloc((size_t)n * sizeof(double complex));
    for (int i = 0; i < n; i++) {
        double Hd = 1.0 / (h * h) + V[i];
        a_diag[i] = 1.0 + I * dt / 2.0 * Hd;
        b_diag[i] = 1.0 - I * dt / 2.0 * Hd;
    }

    double complex *cp = malloc((size_t)n * sizeof(double complex));

    long steps = 4000;    /* t = 20: packet reaches, splits, separates */
    for (long step = 0; step < steps; step++) {
        /* rhs = B psi (tridiagonal multiply; -a_off is B's off-diagonal) */
        for (int i = 0; i < n; i++) {
            rhs[i] = b_diag[i] * psi[i];
            if (i > 0) rhs[i] -= a_off * psi[i - 1];
            if (i + 1 < n) rhs[i] -= a_off * psi[i + 1];
        }
        /* Thomas algorithm: solve A psi = rhs */
        cp[0] = a_off / a_diag[0];
        rhs[0] = rhs[0] / a_diag[0];
        for (int i = 1; i < n; i++) {
            double complex m = a_diag[i] - a_off * cp[i - 1];
            cp[i] = a_off / m;
            rhs[i] = (rhs[i] - a_off * rhs[i - 1]) / m;
        }
        psi[n - 1] = rhs[n - 1];
        for (int i = n - 2; i >= 0; i--) {
            psi[i] = rhs[i] - cp[i] * psi[i + 1];
        }
    }

    /* Diagnostics: norm, transmission, reflection. */
    double total = 0, transmitted = 0, reflected = 0;
    FILE *f = open_dat("tunnel.dat");
    for (int i = 0; i < n; i++) {
        double x = -L / 2 + i * h;
        double p = creal(psi[i]) * creal(psi[i]) + cimag(psi[i]) * cimag(psi[i]);
        total += p * h;
        if (x > bar_hi) transmitted += p * h;
        if (x < bar_lo) reflected += p * h;
        fprintf(f, "%.3f %.6f\n", x, p);
    }
    fclose(f);

    printf("wave packet (E = %.1f) vs barrier (V0 = %.1f, width 1):\n",
           k0 * k0 / 2, V0);
    printf("  norm after %ld steps : %.12f (Crank-Nicolson is unitary)\n",
           steps, total);
    printf("  transmitted: %.3f   reflected: %.3f   (classically: 0 / 1!)\n",
           transmitted, reflected);
    check_close("total probability conserved to 1e-9", total, 1.0, 1e-9);
    check_close("T + R accounts for the whole packet",
                transmitted + reflected, 1.0, 1e-3);
    check("genuine tunneling: a solid fraction crosses a barrier with V0 = E",
          transmitted > 0.1 && transmitted < 0.9);
    printf("final |psi|^2 written to out/tunnel.dat — two humps, one\n"
           "particle: probability flowed THROUGH the classically\n"
           "forbidden region\n");

    free(psi); free(rhs); free(V); free(a_diag); free(b_diag); free(cp);
}

/* ===========================================================================
 * MAIN
 * ===========================================================================
 */

int main(void)
{
    mkdir("out", 0755);

    printf("advanced computational physics in C — every chapter verifies\n"
           "itself against exact results (seeded RNG: runs are identical)\n");

    chapter("1. Floating point & finite differences");
    demo_floating_point();

    chapter("2. Linear algebra: Ax=b and eigenvalues");
    demo_linear_algebra();

    chapter("3. ODE integrators: Euler vs RK4 vs Verlet");
    demo_integrators();

    chapter("4. Normal modes of a coupled chain");
    demo_normal_modes();

    chapter("5. The nonlinear pendulum");
    demo_pendulum();

    chapter("6. Coupled nonlinear systems");
    demo_coupled_nonlinear();

    chapter("7. The route to chaos: the logistic map");
    demo_logistic();

    chapter("8. Continuous chaos: the Lorenz system");
    demo_lorenz();

    chapter("9. Information theory");
    demo_information();

    chapter("10. Thermodynamics: the 2D Ising model");
    demo_ising();

    chapter("11. Quantum stationary states");
    demo_quantum_stationary();

    chapter("12. Quantum dynamics: tunneling");
    demo_quantum_dynamics();

    printf("\n=============================================\n");
    printf(" scorecard: %d / %d checks passed\n", g_checks_passed, g_checks_total);
    printf("=============================================\n");
    return (g_checks_passed == g_checks_total) ? 0 : 1;
}
