# ============================================================================
# Makefile for the C programming course examples.
#
# Targets:
#   make          - build the `main` binary (default)
#   make run      - build and run it
#   make debug    - build with debug symbols and no optimization
#   make clean    - remove build artifacts and demo output files
# ============================================================================

# The compiler to use. Override on the command line: `make CC=clang`.
CC       = gcc

# Compiler flags:
#   -std=c11    use the C11 standard
#   -Wall       enable the common warnings
#   -Wextra     enable extra warnings
#   -pedantic   warn on non-standard extensions
#   -O2         optimize
CFLAGS  ?= -std=c11 -Wall -Wextra -pedantic -O2

# Linker flags: -lm links the math library (needed for sqrt).
LDLIBS   = -lm

# Name of the output binary and its sources.
TARGET   = main
SRCS     = main.c

# The default target: `make` with no arguments builds this.
.PHONY: all
all: $(TARGET)

# Rule: the binary depends on the sources; rebuild when they change.
# $@ expands to the target name, $^ to all prerequisites.
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

# Convenience target to build and run in one step.
.PHONY: run
run: $(TARGET)
	./$(TARGET)

# Debug build: -g embeds debugger info, -O0 keeps code un-optimized so
# stepping in gdb matches the source line by line.
.PHONY: debug
debug: CFLAGS = -std=c11 -Wall -Wextra -pedantic -g -O0
debug: clean $(TARGET)

# Remove everything the build (and the file I/O demo) can produce.
.PHONY: clean
clean:
	rm -f $(TARGET) course_demo.txt students.bin
