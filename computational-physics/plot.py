#!/usr/bin/env python3
# ============================================================================
#  plot.py — ASCII renderer for the .dat files main.c writes into out/
# ============================================================================
#
#  Stdlib only (no matplotlib needed): reads two-column data and renders a
#  density scatter in the terminal. Good enough to SEE the physics — the
#  period-doubling cascade, the Lorenz butterfly, the Ising transition,
#  the tunneled wave packet — anywhere, including over ssh.
#
#  Usage:  python3 plot.py out/bifurcation.dat "title"
# ============================================================================

import sys

WIDTH, HEIGHT = 78, 22
SHADES = " .:+*#@"          # density ramp


def render(path, title):
    xs, ys = [], []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            xs.append(float(parts[0]))
            ys.append(float(parts[1]))

    if not xs:
        print(f"(no data in {path})")
        return

    xmin, xmax = min(xs), max(xs)
    ymin, ymax = min(ys), max(ys)
    xspan = (xmax - xmin) or 1.0
    yspan = (ymax - ymin) or 1.0

    counts = [[0] * WIDTH for _ in range(HEIGHT)]
    for x, y in zip(xs, ys):
        col = min(int((x - xmin) / xspan * (WIDTH - 1)), WIDTH - 1)
        row = min(int((y - ymin) / yspan * (HEIGHT - 1)), HEIGHT - 1)
        counts[HEIGHT - 1 - row][col] += 1     # y grows upward

    peak = max(max(row) for row in counts) or 1
    print(f"\n  {title}   [{path}]")
    print("  +" + "-" * WIDTH + "+")
    for row in counts:
        line = "".join(
            SHADES[min(int(c / peak * (len(SHADES) - 1) + (c > 0)),
                       len(SHADES) - 1)]
            for c in row
        )
        print("  |" + line + "|")
    print("  +" + "-" * WIDTH + "+")
    print(f"   x: [{xmin:.3g}, {xmax:.3g}]   y: [{ymin:.3g}, {ymax:.3g}]"
          f"   ({len(xs)} points)")


if __name__ == "__main__":
    path = sys.argv[1] if len(sys.argv) > 1 else "out/bifurcation.dat"
    title = sys.argv[2] if len(sys.argv) > 2 else path
    render(path, title)
