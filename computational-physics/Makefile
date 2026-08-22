# ============================================================================
# Makefile for the computational physics course.
#
# Targets:
#   make          - build the `main` binary
#   make run      - build and run all 12 chapters (writes out/*.dat)
#   make plots    - run + render ASCII plots of the data (./run.sh)
#   make debug    - debug build (-g -O0)
#   make clean    - remove binary and generated data
# ============================================================================

CC      = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -O2
LDLIBS  = -lm

TARGET  = main
SRCS    = main.c

.PHONY: all
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

.PHONY: run
run: $(TARGET)
	./$(TARGET)

.PHONY: plots
plots: $(TARGET)
	./run.sh

.PHONY: debug
debug: CFLAGS = -std=c11 -Wall -Wextra -pedantic -g -O0
debug: clean $(TARGET)

.PHONY: clean
clean:
	rm -rf $(TARGET) out
