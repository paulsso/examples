# ============================================================================
# Makefile for the C++ game engine course examples.
#
# Targets:
#   make          - build the `main` binary (default)
#   make run      - build and run it
#   make debug    - build with debug symbols and no optimization
#   make clean    - remove build artifacts
# ============================================================================

# The compiler to use. Override on the command line: `make CXX=clang++`.
CXX      = g++

# Compiler flags:
#   -std=c++17  use the C++17 standard
#   -Wall       enable the common warnings
#   -Wextra     enable extra warnings
#   -pedantic   warn on non-standard extensions
#   -O2         optimize (matters for the Chapter 7 benchmarks)
#   -pthread    enable std::thread support (Chapter 14)
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic -O2 -pthread

TARGET   = main
SRCS     = main.cpp

.PHONY: all
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $^

.PHONY: run
run: $(TARGET)
	./$(TARGET)

# Debug build: -g embeds debugger info, -O0 keeps code un-optimized so
# stepping in gdb matches the source line by line.
.PHONY: debug
debug: CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -g -O0 -pthread
debug: clean $(TARGET)

.PHONY: clean
clean:
	rm -f $(TARGET)
