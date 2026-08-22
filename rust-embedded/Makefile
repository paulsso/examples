# ============================================================================
# Makefile for the embedded Rust course examples.
#
# Rust's native build tool is cargo; this Makefile wraps it so the course
# keeps the same interface as its C and C++ siblings.
#
# Targets:
#   make          - build the `main` binary (default, release profile)
#   make run      - build and run it
#   make debug    - build the dev profile (overflow checks ON — Rust's debug
#                   builds panic on integer overflow, catching arithmetic
#                   bugs that release builds wrap silently)
#   make clean    - remove build artifacts
# ============================================================================

CARGO   = cargo
TARGET  = main

.PHONY: all
all: $(TARGET)

# Release profile: optimized, the timings and sizes the course discusses.
$(TARGET): src/main.rs Cargo.toml
	$(CARGO) build --release
	cp target/release/main $(TARGET)

.PHONY: run
run: $(TARGET)
	./$(TARGET)

# Dev profile: debug symbols and overflow checks, for use with gdb/lldb.
.PHONY: debug
debug:
	$(CARGO) build
	cp target/debug/main $(TARGET)

.PHONY: clean
clean:
	$(CARGO) clean
	rm -f $(TARGET)
