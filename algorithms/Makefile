# ============================================================================
# Makefile for the algorithms course — four languages, one output.
#
# Targets:
#   make          - build the C, C++ and Rust binaries (Python needs none)
#   make run      - run all four implementations in sequence
#   make verify   - THE COURSE'S THESIS AS A TEST: run all four, strip the
#                   language banner (first line), and diff the outputs.
#                   They must be byte-identical — algorithms are
#                   language-agnostic; only idioms change.
#   make clean    - remove binaries and captured outputs
# ============================================================================

CC     = gcc
CXX    = g++
RUSTC  = rustc
PYTHON = python3

CFLAGS   = -std=c11 -Wall -Wextra -pedantic -O2
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -O2
RSFLAGS  = --edition 2021 -O

.PHONY: all
all: main-c main-cpp main-rust

main-c: main.c
	$(CC) $(CFLAGS) -o $@ $<

main-cpp: main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

main-rust: main.rs
	$(RUSTC) $(RSFLAGS) -o $@ $<

.PHONY: run
run: all
	./main-c
	./main-cpp
	./main-rust
	$(PYTHON) main.py

# tail -n +2 skips each program's "language: X" banner; everything after it
# must match across all four implementations.
.PHONY: verify
verify: all
	./main-c        | tail -n +2 > out-c.txt
	./main-cpp      | tail -n +2 > out-cpp.txt
	./main-rust     | tail -n +2 > out-rust.txt
	$(PYTHON) main.py | tail -n +2 > out-py.txt
	diff out-c.txt out-cpp.txt
	diff out-c.txt out-rust.txt
	diff out-c.txt out-py.txt
	@echo "VERIFIED: outputs are byte-identical across C, C++, Rust and Python"

.PHONY: clean
clean:
	rm -f main-c main-cpp main-rust out-c.txt out-cpp.txt out-rust.txt out-py.txt
