# ============================================================================
# Makefile for the High Performance Python audio course.
#
# Targets:
#   make run    - run the course (default target)
#   make solve  - run the chapters 1-3 foundations solution
#   make deps   - install the single dependency (numpy)
#   make clean  - remove generated audio and caches
# ============================================================================

PYTHON = python3

.PHONY: run
run:
	$(PYTHON) main.py

.PHONY: solve
solve:
	$(PYTHON) solutions/strange_attractor_saw.py

.PHONY: deps
deps:
	$(PYTHON) -m pip install -r requirements.txt

.PHONY: clean
clean:
	rm -rf out __pycache__ solutions/__pycache__
