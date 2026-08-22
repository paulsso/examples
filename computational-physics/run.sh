#!/usr/bin/env bash
# ============================================================================
#  run.sh — build, run the full course, and render the physics as ASCII art
# ============================================================================
#
#  The C program does the physics and writes data files; this script is the
#  glue: bash orchestrates, Python renders. (The course's supporting-script
#  philosophy: C for the numbers, scripts for everything around them.)

set -euo pipefail
cd "$(dirname "$0")"

make
./main

echo
echo "================ rendering the data ================"
python3 plot.py out/bifurcation.dat "logistic map: the period-doubling route to chaos (x vs r)"
python3 plot.py out/lorenz.dat     "the Lorenz attractor (z vs x): deterministic, never repeating"
python3 plot.py out/ising.dat      "Ising model: |M| per spin vs temperature (the phase transition)"
python3 plot.py out/tunnel.dat     "tunneling: |psi|^2 after hitting a barrier with V0 = E"
