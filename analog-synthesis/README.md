# Analog Electronics for Modular Synthesis (Qucs-S)

An intermediate course that builds **Eurorack-style analog modules** —
a **VCO**, **VCF**, **VCA**, and **LFO** — in open-source circuit
simulation. Schematics ship as SPICE netlists for [Qucs-S](https://ra3xdh.ru/qucs/);
a Python educational simulator always runs offline and prints a
PASS/FAIL scorecard. The course text is [DOCUMENTATION.md](DOCUMENTATION.md).

**Audience:** you already know voltage/current, passives, and the idea of
an op-amp or transistor. We skip Ohm's-law primers and start at signal
circuits and SPICE workflow.

```
op-amp blocks → expo converter → VCO → VCF → VCA → LFO → voice patch
                                                      ↘ music-scene context
```

## Chapters

1. **Qucs-S workflow & Eurorack conventions** — rails, audio vs CV, 1 V/oct, unity buffer
2. **Op-amp building blocks** — summer, integrator, Schmitt trigger, precision rectifier
3. **Exponential converter** — why pitch needs `2^(CV)`, BJT expo pair
4. **VCO** — integrator + Schmitt, triangle/square, 1 V/oct tracking
5. **VCF** — state-variable filter, OTA integrators, resonance
6. **VCA** — LM13700 OTA gain cell, linear CV, headroom
7. **LFO** — the same core, scaled to 0.05–30 Hz
8. **Patching the voice** — VCO → VCF → VCA, LFO → VCA CV (+ `out/voice.wav`)
9. **Music-scene electronics** — guitar amps vs Eurorack vs DJ mixers vs studio rack
10. **From simulation to bench** — BOM, macromodel pitfalls, next hardware steps

Every runnable chapter verifies itself. `make run` is a circuit test suite.

## Running

Requires Python 3.10+ and NumPy. Qucs-S is optional for GUI schematics.

```bash
make deps   # python3 -m pip install -r requirements.txt
make run    # all chapters → scorecard + out/*.dat + out/voice.wav
make clean  # remove generated data
```

### Qucs-S (recommended for learning)

1. Install [Qucs-S](https://ra3xdh.ru/qucs/) (includes a SPICE backend).
2. Open any file in [`circuits/`](circuits/) — each `.cir` is a documented
   netlist with an ASCII schematic in the header. Rebuild as a visual
   `.sch` in the Qucs-S editor using the same topology and values.
3. Run DC / AC / transient simulations; compare plots to `out/chNN-*.dat`
   from the Python scorecard.

If `ngspice` or `qucsator` is on your `PATH`, `run.sh` also attempts a
best-effort batch SPICE pass after the Python scorecard.

## Listen to your work

| File | Chapter | What you hear |
|---|---|---|
| `out/voice.wav` | 8 | Triangle VCO through a low-pass VCF, amplitude-modulated by a 4 Hz LFO |

## Layout

| File | Purpose |
|---|---|
| `simulate.py` | Educational models + PASS/FAIL checks for chapters 1–8 |
| `verify.py` | Thin entry point → `simulate.main()` |
| `circuits/chNN-*.cir` | SPICE netlists for Qucs-S / ngspice |
| `DOCUMENTATION.md` | Full course text (theory, design choices, Ch 9–10) |
| `run.sh` / `Makefile` | `run`, `deps`, `clean` |
| `requirements.txt` | numpy |

## Relationship to other courses in this repo

- [`python-audio/`](../python-audio/) — the same oscillators, filters, and LFO ideas in **digital** form (NumPy). Chapter 10 points there.
- [`rust-embedded/`](../rust-embedded/) — firmware patterns for eventually *controlling* analog hardware from a microcontroller.
