#!/usr/bin/env python3
"""
verify.py — thin entry point for the PASS/FAIL scorecard.

The educational models and checks live in simulate.py. This module exists
so `python3 verify.py` matches the course layout described in README.md.
If you have exported Qucs-S/ngspice .dat files into out/, simulate.py's
checks still use the Python reference models (authoritative); compare
waveforms visually in Qucs-S against out/chNN-*.dat.
"""
from simulate import main

if __name__ == "__main__":
    raise SystemExit(main())
