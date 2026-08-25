#!/usr/bin/env bash
# ============================================================================
# run.sh — Analog Electronics for Modular Synthesis
#
# Runs the educational Python simulator (always available) and prints the
# PASS/FAIL scorecard. If ngspice or qucsator is installed, also attempts
# a batch SPICE run of circuits/*.cir (best-effort; Python scorecard is
# authoritative for `make run`).
# ============================================================================
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
mkdir -p out

PYTHON="${PYTHON:-python3}"

echo "==> Checking dependencies"
if ! "$PYTHON" -c "import numpy" 2>/dev/null; then
  echo "NumPy missing. Run: make deps"
  exit 1
fi

echo "==> Running educational simulator (simulate.py)"
"$PYTHON" simulate.py
STATUS=$?

# Optional SPICE backend
SPICE_BIN=""
if command -v ngspice >/dev/null 2>&1; then
  SPICE_BIN="ngspice"
elif command -v qucsator >/dev/null 2>&1; then
  SPICE_BIN="qucsator"
fi

if [[ -n "$SPICE_BIN" ]]; then
  echo ""
  echo "==> Optional SPICE batch via $SPICE_BIN"
  for cir in circuits/ch*.cir; do
    base="$(basename "$cir" .cir)"
    echo "    simulating $cir"
    if [[ "$SPICE_BIN" == "ngspice" ]]; then
      # batch mode; ignore individual failures (behavioral netlists vary)
      ngspice -b -o "out/${base}-spice.log" "$cir" || true
    else
      qucsator -i "$cir" -o "out/${base}-spice.dat" || true
    fi
  done
  echo "    SPICE logs in out/*-spice.*"
else
  echo ""
  echo "==> Note: ngspice/qucsator not found — Python scorecard only."
  echo "    Install Qucs-S (https://ra3xdh.ru/qucs/) and open circuits/*.cir"
  echo "    for schematic exploration and full SPICE runs."
fi

exit "$STATUS"
