# Embedded Rust Course Examples

A comprehensive set of examples for a zero-to-hero course in **Rust for
microcontrollers**. All examples are implemented in a single file,
`src/main.rs`, organized into fifteen chapters — from a first line of Rust to
the patterns that define real firmware:

1. **Rust Fundamentals** — explicit-width integers, mutability, shadowing, expressions
2. **Ownership & Borrowing** — moves, shared vs exclusive borrows, deterministic `Drop` (RAII clock guards)
3. **Enums, Option, Result** — typed errors, the `?` operator, exhaustive matching
4. **Structs, impl & Traits** — methods, default trait methods, `dyn` dispatch
5. **Bits & Registers** — masks, multi-bit fields, read-modify-write, volatile MMIO (the course's single `unsafe` block)
6. **Type-State GPIO** — pin modes as type parameters; driving an unconfigured pin is a *compile* error
7. **Generic Drivers** — `embedded-hal`-style traits: one `Led` driver runs on a GPIO pin and a relay board unchanged
8. **Heapless Data Structures** — a const-generic ring buffer; no allocator, capacity known at link time
9. **Interrupts & Shared State** — atomics and critical sections, with a real second thread playing the ISR
10. **State Machines** — a button debouncer and a traffic-light controller
11. **Protocols** — a byte-at-a-time UART frame parser with checksums and resync, and bit-banged SPI
12. **Fixed-Point Math** — Q16.16 arithmetic and integer ADC pipelines for FPU-less chips
13. **A Sensor Driver** — a TMP102 temperature sensor driver, generic over an I2C bus trait
14. **Task Scheduling** — the super-loop and a drift-free tick scheduler
15. **Capstone** — a deterministic greenhouse controller combining all of the above

Every function is documented in [DOCUMENTATION.md](DOCUMENTATION.md), with
explanations of the concepts, the real-hardware equivalents, and pitfalls.

## The simulation trick

Real embedded Rust runs on `no_std` targets (`thumbv7em-none-eabihf`,
`riscv32imac-unknown-none-elf`, ...) and needs hardware or QEMU. This course
instead **simulates the microcontroller in ordinary Rust**, so every chapter
runs on any host with a stock toolchain — but every pattern shown (typestate
pins, HAL traits, atomics in ISRs, register masks, heapless buffers) is
written exactly the way real firmware writes it. Comments mark every place
the simulation and real hardware differ, and `println!` stands in for
`defmt`/RTT logging. The course has **zero dependencies** on purpose: it
builds from scratch what `embedded-hal`, `heapless`, and `fixed` provide, so
students understand the ecosystem crates before adopting them.

## Building and running

Requires a stock Rust toolchain (`rustc`/`cargo`, edition 2021) and `make`.

```bash
make          # build the `main` binary (release profile)
./main        # run all chapters in order
make run      # build and run in one step
make debug    # dev profile: overflow checks on, debug symbols
make clean    # remove build artifacts
```

The build compiles warning-free.

## Layout

| File | Purpose |
|---|---|
| `src/main.rs` | All example implementations, organized by chapter |
| `DOCUMENTATION.md` | Written explanation of every function |
| `Cargo.toml` | Package manifest (no dependencies) |
| `Makefile` | Wraps cargo; builds the `main` binary |
