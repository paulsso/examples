# ============================================================================
# Makefile for the C/C++ patterns & anti-patterns course.
#
# Targets:
#   make          - build the `main` binary (default)
#   make run      - build and run it
#   make debug    - build with debug symbols and no optimization
#   make sanitize - build with AddressSanitizer + UBSanitizer and run.
#                   Sanitizers are part of the course's message (Chapter 6):
#                   warnings catch bugs at compile time, sanitizers catch
#                   the rest at test time.
#   make clean    - remove build artifacts
# ============================================================================

CXX      = g++
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

.PHONY: debug
debug: CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -g -O0 -pthread
debug: clean $(TARGET)

# ASan finds memory errors (overflows, use-after-free, leaks); UBSan traps
# undefined behavior (signed overflow, bad shifts, misaligned access) as it
# executes. This program is UB-free, so the sanitized run must be clean.
.PHONY: sanitize
sanitize:
	$(CXX) -std=c++17 -Wall -Wextra -pedantic -g -O1 -pthread \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		-o $(TARGET)-sanitized $(SRCS)
	./$(TARGET)-sanitized > /dev/null && echo "sanitized run: CLEAN"

.PHONY: clean
clean:
	rm -f $(TARGET) $(TARGET)-sanitized
