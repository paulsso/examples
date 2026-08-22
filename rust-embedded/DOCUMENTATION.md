# Embedded Rust Course — Documentation

This document explains every function, struct, and trait implemented in
`src/main.rs`. The code is organized as fifteen **chapters** that take a
student from zero Rust to the patterns that define real microcontroller
firmware. Running the program (`make run`) walks through all of them in
order.

Build and run:

```bash
make        # build the `main` binary (release profile)
./main      # run it (or `make run`)
make debug  # dev profile: overflow checks on, debug symbols
make clean  # remove build artifacts
```

---

## Table of Contents

1. [Chapter 1 — Rust Fundamentals](#chapter-1--rust-fundamentals)
2. [Chapter 2 — Ownership & Borrowing](#chapter-2--ownership--borrowing)
3. [Chapter 3 — Enums, Option, Result](#chapter-3--enums-option-result)
4. [Chapter 4 — Structs, impl & Traits](#chapter-4--structs-impl--traits)
5. [Chapter 5 — Bits & Registers](#chapter-5--bits--registers)
6. [Chapter 6 — Type-State GPIO](#chapter-6--type-state-gpio)
7. [Chapter 7 — Generic Drivers](#chapter-7--generic-drivers-the-embedded-hal-pattern)
8. [Chapter 8 — Heapless Data Structures](#chapter-8--heapless-data-structures)
9. [Chapter 9 — Interrupts & Shared State](#chapter-9--interrupts--shared-state)
10. [Chapter 10 — State Machines](#chapter-10--state-machines)
11. [Chapter 11 — Protocols](#chapter-11--protocols)
12. [Chapter 12 — Fixed-Point Math](#chapter-12--fixed-point-math)
13. [Chapter 13 — A Sensor Driver](#chapter-13--a-sensor-driver)
14. [Chapter 14 — Task Scheduling](#chapter-14--task-scheduling)
15. [Chapter 15 — Capstone](#chapter-15--capstone-a-greenhouse-controller)

---

## The simulation approach

Real embedded Rust is `no_std`: no heap, no OS, no `println!`; binaries are
cross-compiled for targets like `thumbv7em-none-eabihf` and flashed to a
board. To make the course runnable on any host with a stock toolchain, the
*hardware* is simulated — a fake I2C sensor, pins that flip a `bool`, a
thread standing in for a timer interrupt — while the *patterns* (typestate,
HAL traits, atomics, register masks, heapless buffers) are written exactly as
real firmware writes them. Comments in the code mark each divergence.
`println!` stands in for `defmt`/RTT debug logging.

The course intentionally has **zero dependencies**: it builds from scratch
the essence of `embedded-hal` (Chapter 7), `heapless` (Chapter 8), and
`fixed` (Chapter 12), so students understand what those crates do before
adopting them.

### `chapter(title: &str)`

Prints a banner separating each chapter's output — same convention as the C
and C++ courses.

---

## Chapter 1 — Rust Fundamentals

### `demo_fundamentals()`

The ground floor, with an embedded accent:

- **Explicit-width integers** — `u8`, `u16`, `u32`, `i8` map exactly onto
  registers, ADC results and RAM budgets; there is no platform-dependent
  `int`. Binary literals (`0b0001_0100`) with `_` separators make register
  values readable. `std::mem::size_of` confirms the sizes.
- **Immutability by default** — `let` bindings cannot change; `mut` is a
  visible opt-in, so reading firmware you know at a glance what can mutate.
- **Shadowing** — re-binding a name, even to a new type: the natural shape
  of the raw-reading → engineering-units pipeline.
- **`if` as an expression** — produces a value directly; no ternary
  operator needed.
- **`loop` with a break value** — poll until a condition and keep the
  result, exactly like waiting on a PLL-locked status bit.
- **Ranges** — `0..4` iterates without C's off-by-one-prone counters.

---

## Chapter 2 — Ownership & Borrowing

The borrow checker is the value proposition for firmware: every value has
exactly one owner, dropped deterministically at scope end — **no garbage
collector** (no pauses, no heap requirement), no leaks, no double-frees, no
data races, all proven at compile time.

### `struct DmaBuffer`

A stand-in for a buffer hardware may be writing into. Deliberately **not
`Copy`**: duplicating it would be a correctness disaster, and Rust makes
duplication impossible unless explicitly opted into. Assignment **moves**
ownership; using the old binding afterwards is a compile error (shown
commented out in the demo).

### `checksum(buf: &DmaBuffer) -> u8`

Takes a **shared borrow** (`&`) — read-only access, any number of
simultaneous readers. Implemented with an iterator `fold` (XOR accumulate),
introducing iterator style early.

### `fill_pattern(buf: &mut DmaBuffer, seed: u8)`

Takes an **exclusive borrow** (`&mut`) — write access, exactly one at a
time, and *nobody* may read while it lives. The C-classic bug of an ISR
reading a buffer that `main()` is rewriting cannot compile. Uses
`wrapping_add` to state the overflow policy explicitly.

### `struct ClockGuard` (+ `Drop`)

RAII from the C++ course, now borrow-checked: `new` "enables" a peripheral
clock, the `Drop` implementation disables it at scope end — in reverse
declaration order, on every exit path, with zero cleanup code at call sites.

### `demo_ownership()`

Shows the move, both borrow kinds, and deterministic drop in sequence.

---

## Chapter 3 — Enums, Option, Result

C firmware signals errors with magic values (`-1`, `0xFF`, `NULL`) that
nothing forces callers to check. Rust encodes absence and failure in types
the compiler refuses to let you ignore.

### `enum Command`

An enum whose variants **carry data** (`SetPwm { channel, duty }`) — the C
course's tagged union, with the tag handling built into the language.

### `enum AdcError`

Every failure mode as a variant. `#[derive(Debug, PartialEq)]` provides
printing and comparison for free.

### `find_free_channel(used_mask: u8) -> Option<u8>`

Returns the first zero bit in a channel-usage mask — or `None`. The
possible absence is part of the signature; iterator `find` expresses the
scan without an explicit loop.

### `read_adc(channel: u8, calibrated: bool) -> Result<u16, AdcError>`

Success carries the reading; failure carries a typed reason. Callers must
`match` (or `?`) — silently dropping an error requires visible effort.

### `average_two_channels(a: u8, b: u8) -> Result<u16, AdcError>`

The **`?` operator**: each `read_adc(..)?` either unwraps the Ok value or
returns the error to *this function's* caller immediately. Error
propagation without exceptions and without C's `goto cleanup` pyramids.

### `execute(cmd: &Command)`

**Exhaustive matching**: omit a variant and compilation fails. Add a
variant next year and the compiler lists every `match` that must be
updated — invaluable in large firmware.

---

## Chapter 4 — Structs, impl & Traits

### `struct PwmChannel` + `new` / `set_duty` / `duty`

Data plus an `impl` block of methods. `new` is the constructor convention
(an associated function, no `self`); `set_duty(&mut self)` clamps to 100 so
the invariant always holds; `duty(&self)` is a read-only accessor.
Receivers (`&self` vs `&mut self`) encode read vs write access in the
signature itself.

### `trait Sensor`

Rust's interface construct: `name`, `read`, and `unit` — the last with a
**default implementation** that implementors may override (like a virtual
function with a body, but no inheritance hierarchy).

### `struct OnboardTemp` / `struct Potentiometer`

Two unrelated types implementing `Sensor`. `OnboardTemp` overrides `unit`
("m°C"); `Potentiometer` inherits the "raw" default.

### `demo_structs_and_traits()`

Iterates `[&mut dyn Sensor; 2]` — **dynamic dispatch** through a vtable,
ideal for heterogeneous lists. The chapter contrasts it with the static
dispatch (generics) that embedded code usually prefers, setting up
Chapter 7.

---

## Chapter 5 — Bits & Registers

A microcontroller *is* its registers. This chapter ports the C course's bit
toolbox and adds the two things C never made safe: field access and
volatile MMIO.

### `set_bits` / `clear_bits` / `toggle_bits` / `bit_is_set`

The four fundamental mask operations (`|`, `& !`, `^`, `>> … & 1`) as pure
functions on `u32`.

### `read_field(reg, shift, width)` / `write_field(reg, shift, width, value)`

Datasheets describe registers as packed fields ("bits 7:4 = prescaler").
`read_field` shifts down and masks; `write_field` performs the fundamental
**read-modify-write**: clear the field's bits, OR in the new value, leave
the neighbors untouched.

### `struct UartRegisters`

A simulated register block. On real hardware this struct is never
constructed — it is *cast from the datasheet's base address*, which is
exactly what PACs (peripheral access crates, generated from vendor SVD
files) do.

### `demo_bits_and_registers()` — the volatile lesson

The compiler may cache, reorder, or delete memory accesses it believes are
unobservable — but hardware registers change behind its back. MMIO must use
`std::ptr::write_volatile` / `read_volatile`, which require `unsafe`. This
is the **course's single `unsafe` block**, and the point is made explicitly:
real firmware wraps volatile access once, inside the PAC, and everything
above it is safe Rust. One block to audit instead of a whole codebase.

---

## Chapter 6 — Type-State GPIO

The flagship embedded-Rust pattern: a pin's configuration becomes a **type
parameter**, so using a pin wrongly is a compile error.

### `struct Floating` / `struct Output`

Zero-sized **marker types** — no data, no methods. They exist only to
distinguish `Pin<Floating>` from `Pin<Output>` in the type system.

### `struct Pin<Mode>`

Generic over its mode; `PhantomData<Mode>` tells the compiler the parameter
is intentional without storing anything. The `level: bool` field simulates
the electrical state (real HALs write GPIO registers instead).

### `Pin::<Floating>::new(number)` / `into_output(self)`

Pins come out of reset floating. `into_output` **consumes `self`** (takes
it by value) and returns a different type — the old floating handle is
moved away, so code that kept it cannot compile. Ownership as a hardware
invariant: a pin cannot be two modes at once.

### `Pin<Output>::set_high` / `set_low` / `is_set_high`

Exist *only* on `Pin<Output>`. Calling `set_high` on a floating pin is not
a runtime error code — it is "no such method", at compile time (shown
commented out in the demo).

The demo prints `size_of::<Pin<Output>>()` to prove the safety is free:
the marker occupies zero bytes; mode checking happens entirely at compile
time.

---

## Chapter 7 — Generic Drivers (the embedded-hal pattern)

The crown jewel of the Rust embedded ecosystem: traits define *what
hardware can do*, each chip's HAL implements them, and driver crates are
written once against the traits — portable across every chip.

### `trait OutputPin`

The contract (`set_high`/`set_low`), mirroring
`embedded_hal::digital::OutputPin` in simplified form (the real one returns
`Result` to accommodate fallible pins such as I2C GPIO expanders).

### `impl OutputPin for Pin<Output>`

Chapter 6's typestate pin satisfies the contract by delegating to its
inherent methods.

### `struct RelayChannel` + `impl OutputPin`

A completely different "pin" — a channel on an external relay board — that
also satisfies the contract. That's the whole trick.

### `struct Led<P: OutputPin>` + `new` / `on` / `off` / `toggle`

**The driver**: generic over any `P` implementing `OutputPin`. Written
once; drives an STM32 pin, an ESP32 pin, a GPIO expander, or the relay
without modification. Generics compile via **monomorphization** to static
dispatch — a dedicated, fully-inlined copy per pin type, no vtables on the
hot path.

### `morse_blink<P: OutputPin>(led, pattern)`

A generic *function* over the same bound, blinking any LED with a
dot/dash pattern — driver-agnostic application code.

### `demo_generic_drivers()`

Runs the identical `Led` driver over both pin types, and reads the relay's
coil state back to show driver state and "hardware" state agree.

---

## Chapter 8 — Heapless Data Structures

Most firmware runs with **no heap**: allocation is nondeterministic, can
fragment, and can fail in the field. Containers instead have fixed
capacities baked into their types, living in `.bss` or on the stack —
the RAM budget is known at link time.

### `struct RingBuffer<const N: usize>`

A circular byte buffer — *the* structure for UART receive queues (ISR
pushes, main loop pops). **Const generics** make the capacity part of the
type: `RingBuffer<4>` and `RingBuffer<64>` are different types, both backed
by plain arrays, no allocator anywhere. This is the C course's queue,
generic and safe; the production version is the `heapless` crate.

- **`new()`** — zeroed array, empty state.
- **`push(byte) -> Result<(), BufferFull>`** — appends at
  `(head + len) % N`; the modulo wrap reuses the array forever without
  shifting. When full it returns `Err(BufferFull)` — a typed, must-handle
  event. The firmware *decides* the overflow policy (drop? overwrite?
  assert?) instead of a hidden `malloc` growing the heap.
- **`pop() -> Option<u8>`** — removes the oldest byte; `None` when empty.
- **`len()`** — bytes currently stored.

### `struct BufferFull`

A unit error type: overflow as a value, not a silent behavior.

---

## Chapter 9 — Interrupts & Shared State

An interrupt handler preempts `main()` between any two instructions and
shares memory with it — in C, the eternal source of heisenbugs. Rust
refuses to compile racy access to shared statics: the options are atomics
or critical sections, and the chapter demonstrates both **with a real
second thread playing the hardware timer**, so the concurrency is genuine.

### `static TICK_COUNT: AtomicU32`

The shared counter. A plain `static mut u32` would be undefined behavior —
and Rust will not compile it without `unsafe`. Atomics are safe from both
sides and lock-free (ISR-safe: an ISR must never block on a lock).

### `systick_handler()`

The "interrupt handler": one `fetch_add(1, Ordering::Relaxed)`. On a
Cortex-M this function would carry the `#[interrupt]` attribute and be
placed in the vector table — the *body* would not change. `Relaxed`
ordering suffices for a pure counter (no other memory is synchronized by
it), a first look at memory orderings.

### `struct Config` + `static CONFIG: Mutex<Config>`

State too big for an atomic goes behind a lock. On Cortex-M this is
`cortex_m::interrupt::Mutex<RefCell<Config>>`, locked by disabling
interrupts (`interrupt::free`); std's `Mutex` plays that role here. The
pattern is identical: shared state is *inaccessible* except through the
lock — enforced by the type system, not by convention or code review.

### `demo_interrupts()`

Spawns the timer thread firing `systick_handler` 50 times while `main`
concurrently polls `TICK_COUNT` (safe from both sides), joins, and verifies
the count is **exactly 50** — no lost updates. Then mutates `CONFIG` inside
a scoped lock (released at the brace — RAII again).

---

## Chapter 10 — State Machines

Firmware is state machines all the way down; Rust enums make states
explicit and `match` makes transitions exhaustive.

### `struct Debouncer` + `new(threshold)` / `update(raw) -> Option<bool>`

Physical buttons "bounce" — rapid on/off chatter for milliseconds after a
press. The debouncer accepts a new level only after `threshold` consecutive
samples agree: `update` is called at a fixed tick rate, tracks a candidate
level and a run count, and returns `Some(new_level)` only on an accepted
edge. The demo feeds 15 noisy samples (a bouncy press and a bouncy release)
and gets exactly 2 clean edges out.

### `enum Light` + `next()` / `duration_ticks()`

The traffic light: the canonical explicit FSM. Each state knows its
successor and its dwell time — the complete behavior in two exhaustive
`match` expressions. Adding a `LeftArrow` state would surface every spot
that needs updating as compile errors.

---

## Chapter 11 — Protocols

Bytes arrive one at a time, with noise, at arbitrary alignment. Firmware
reassembles frames with byte-at-a-time state machines and speaks to bare
chips by bit-banging lines directly.

### The toy UART protocol

`[0x7E][len][payload × len][checksum]`, checksum = XOR of payload bytes.

### `enum ParserState`

One variant per protocol phase — and the `Payload` variant *carries its own
counter* (`remaining`), so state-specific data lives inside the state.
Illegal combinations (a payload counter while idle) are unrepresentable.

### `struct FrameParser` + `new()` / `push_byte(byte) -> Option<([u8; 16], usize)>`

Consumes one byte, advances the machine, and returns a completed frame
(payload copied out, plus its length) only when the checksum matches.
Everything else — line noise, absurd lengths, bad checksums, truncated
frames — quietly **resynchronizes to Idle**: a parser that panics on noise
is a parser that bricks devices. The demo stream contains garbage, a valid
frame, a corrupted frame, and a frame split across two "reads"; exactly the
two valid frames come out.

### `struct ShiftRegisterDevice` + `on_rising_edge(mosi)`

The simulated SPI peripheral: shifts the MOSI bit in on each rising clock
edge, like a real shift register.

### `spi_transfer_byte(byte, device)`

**Bit-banged SPI, mode 0**: for each of 8 bits MSB-first, put the bit on
the data line and raise the clock (the device samples on that edge). This
is how firmware talks to chips when no hardware SPI block is free — and
the clearest possible demonstration of what an SPI peripheral does.

---

## Chapter 12 — Fixed-Point Math

Many MCUs (Cortex-M0/M3, 8-bit parts) have no FPU: every `f32` operation
becomes a slow soft-float library call. Firmware scales integers instead.

### `struct Fix(i32)` — Q16.16

16 integer bits, 16 fraction bits in an `i32`. The **newtype wrapper**
means the type system prevents accidentally mixing raw integers and
fixed-point values.

- **`from_int(v)`** — shift left by 16: `5` becomes 5.0 (raw 327680).
- **`from_ratio(num, den)`** — `(num/den)` with round-to-nearest: how
  constants like 2.5 enter fixed-point code with no float in sight.
- **`mul(other)`** — the product of two Q16.16 values has 32 fraction
  bits, so widen to `i64`, multiply, shift back down. Skipping the
  widening is *the* classic fixed-point overflow bug (and the debug build,
  with overflow checks on, would catch it).
- **`add(other)`** — same format, plain integer addition.
- **`to_millis()`** — thousandths for display, still in integer math.

### `adc_to_millivolts(raw: u16, vref_mv: u32) -> u32`

The integer ADC pipeline: `raw * vref / 4095`, widened to `u64` *before*
multiplying (4095 × 3300 already overflows `u16` arithmetic).

### `tmp36_millicelsius(mv: u32) -> i32`

A TMP36 analog temperature sensor: 500 mV offset, 10 mV/°C, so
`(mV − 500) × 100` millicelsius. Exact, deterministic, FPU-free.

### `demo_fixed_point()`

Computes 2.5 × 3.25 = 8.125 with no float operation, shows 1 + 1/3 (inexact
in *any* binary format — a good discussion point), runs the ADC pipeline in
integers, and cross-checks against `f32` (which agrees to quantization, and
costs ~100× more cycles on an M0).

---

## Chapter 13 — A Sensor Driver

How the ecosystem's hundreds of driver crates work: a driver owns (a handle
to) the bus, is generic over the bus **trait**, and translates register
traffic into engineering units. Nothing in it names concrete hardware.

### `enum BusError`

Bus failure modes; `Nack` = no device acknowledged the address.

### `trait I2cBus`

The bus contract, mirroring `embedded_hal::i2c` in simplified form: one
combined `write_read` transaction — write the register index, read its
contents — which is how register-based I2C devices are actually accessed.

### `struct FakeI2cBus` + `impl I2cBus`

A simulated **TMP102** temperature sensor at address 0x48, implemented from
the real datasheet: register 0x00 holds the temperature as 12 bits,
left-justified across two bytes, 0.0625 °C per LSB. Wrong address →
`Err(Nack)`, like real silicon staying silent.

### `struct Tmp102<B: I2cBus>` + `new(bus, address)` / `read_millicelsius()`

**The driver.** Generic over any bus implementing `I2cBus`: the fake bus
today, an STM32 I2C peripheral tomorrow, a Linux `i2cdev` in an integration
test — the driver code never changes. `read_millicelsius` performs the
write-read, reassembles the 12-bit raw value
(`(msb << 4) | (lsb >> 4)`), and converts with integer math
(`raw × 625 / 10`, Chapter 12). Bus errors propagate with `?` (Chapter 3).

### `demo_sensor_driver()`

Reads 25.000 °C from the fake sensor, then shows a driver pointed at the
wrong address receiving `Err(Nack)` as a handled value.

---

## Chapter 14 — Task Scheduling

Most shipped firmware is not an RTOS — it is a **super-loop**: wake on a
timer tick, run whatever is due, sleep. A cooperative tick scheduler is ~30
lines and covers a huge share of real products. (Beyond it: RTIC and
Embassy, Rust's interrupt-driven and async schedulers.)

### `struct Task`

A name, a period, the next due tick, and a plain **function pointer**
(`fn(u32)`) — no heap, no closures required.

### `struct Scheduler<const N: usize>` + `new(tasks)` / `tick(now)`

Const generics again: the task table's size is part of the type, in `.bss`,
known at link time. `tick` runs every task whose time has come. One subtle,
load-bearing detail: rescheduling uses `next_run += period` rather than
`next_run = now + period` — the former **prevents drift** when a task runs
late; the latter accumulates lag forever.

### `heartbeat_task` / `sensor_task` / `telemetry_task`

Three periodic tasks (every 2, 5, and 6 ticks, telemetry phase-shifted to
start at tick 3) printing their tick — the demo runs 12 ticks so the
interleaving is visible. The comment marks where real firmware executes
WFI (wait-for-interrupt) to sleep between ticks: milliamps become
microamps.

---

## Chapter 15 — Capstone: A Greenhouse Controller

Everything combined in one deterministic control loop, ten ticks long:

| Piece | Chapter |
|---|---|
| TMP102 driver over the fake I2C bus | 13, 7 |
| Integer ADC conversion for soil moisture | 12 |
| Debounced manual-override button | 10 |
| Heater (GPIO pin) + pump (relay) behind `OutputPin` | 6, 7 |
| Event log in a heapless ring buffer | 8 |
| Tick-driven sense → input → control → report loop | 14 |
| `Result`/`Option` handling throughout | 3 |

### Event constants + `event_name(code)`

One-byte event codes (`EV_HEATER_ON`, …) logged into the ring buffer —
exactly like a black-box event log in RAM on a real device — with a
name-mapping function for the final printout.

### `run_capstone()`

Each tick: read the thermometer (with `unwrap_or` fallback), convert the
scripted soil ADC sample to millivolts, debounce the scripted button (a
press toggles pump override), run **bang-bang control** on the heater
around a 21 °C setpoint, drive the pump when soil is dry or overridden,
log every actuator edge, print a status line, then advance the simulated
world (the heater warms the greenhouse 0.75 °C per tick; otherwise it cools
0.25 °C).

The run shows the heater raising the temperature to the setpoint and then
oscillating around it (the signature of bang-bang control and a natural
segue to hysteresis/PID as follow-on topics), the button press engaging the
pump override through the debouncer, and the event log draining in FIFO
order at the end. Scripted inputs plus integer math make every run
byte-for-byte identical — determinism as a testing strategy.

---

## Where to go next (real hardware)

The course's patterns map directly onto the real ecosystem:

| Course construct | Production equivalent |
|---|---|
| `Pin<Mode>` typestate | every HAL crate's GPIO API (`stm32f4xx-hal`, `esp-hal`, ...) |
| `OutputPin` / `I2cBus` traits | `embedded-hal` |
| `RingBuffer<N>` | `heapless::spsc::Queue` |
| `Fix` | the `fixed` crate |
| `UartRegisters` simulation | PACs generated by `svd2rust` |
| `#[interrupt]` comment | `cortex-m-rt` interrupt attributes |
| `Mutex<Config>` critical section | `cortex_m::interrupt::Mutex<RefCell<T>>` / the `critical-section` crate |
| the tick scheduler | RTIC (interrupt-driven) or Embassy (async) |
| `println!` | `defmt` + RTT |

## Suggested Course Progression

| Stage | Chapters | Exercises to assign |
|---|---|---|
| Beginner | 1–4 | A `Percent(u8)` newtype with clamped constructor; a `Direction` enum with `opposite()` |
| Intermediate | 5–8 | `write_field` for 16-bit registers; `Pin<Input>` with pull-up typestate; make `RingBuffer` generic over `T: Copy` |
| Advanced | 9–12 | An overwrite-oldest ring buffer variant; hysteresis for the debouncer; CRC-8 instead of XOR; Q8.8 format |
| Expert | 13–15 | A driver for a second fake sensor (e.g. an SPI accelerometer); priority-aware scheduler; port a chapter to a real board with `embedded-hal` |
