# ============================================================================
# Makefile for the Analog Electronics for Modular Synthesis course.
#
# Targets:
#   make deps   - install Python dependencies (numpy)
#   make run    - run all chapter simulations + PASS/FAIL scorecard
#   make clean  - remove generated data and WAV files
# ============================================================================

PYTHON ?= python3

.PHONY: all
all: run

.PHONY: deps
deps:
	$(PYTHON) -m pip install -r requirements.txt

.PHONY: run
run:
	./run.sh

.PHONY: clean
clean:
	rm -rf out/*.dat out/*.csv out/*.wav out/*.txt
	@# keep out/.gitkeep
	touch out/.gitkeep
