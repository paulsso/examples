// ============================================================================
//  main.rs — Rust for Microcontrollers: From Zero to Hero
// ============================================================================
//
//  This file is the backbone of a course on embedded Rust: taking a student
//  from their first line of Rust to the patterns that define real firmware —
//  registers, type-safe GPIO, portable drivers, interrupt-safe state,
//  heapless data structures, protocols, fixed-point math, and scheduling.
//
//  THE SIMULATION TRICK: real embedded Rust runs on `no_std` targets
//  (thumbv7em, riscv32...) and needs hardware or QEMU. This course instead
//  SIMULATES the microcontroller in ordinary Rust so every chapter runs on
//  any host — but every pattern shown (typestate pins, embedded-hal style
//  traits, atomics in ISRs, register masks) is written exactly the way real
//  firmware writes it. Where the simulation and real hardware differ, the
//  comments say so explicitly. `println!` stands in for `defmt`/RTT logging.
//
//  Chapters:
//    1.  Rust Fundamentals     — integer widths, mutability, expressions
//    2.  Ownership & Borrowing — the borrow checker as a firmware ally
//    3.  Enums & Errors        — Option, Result, exhaustive matching
//    4.  Structs & Traits      — methods, trait objects, default methods
//    5.  Bits & Registers      — masks, fields, volatile MMIO
//    6.  Type-State GPIO       — pin modes enforced at compile time
//    7.  Generic Drivers       — embedded-hal style traits, one driver
//                                for every chip
//    8.  Heapless Structures   — a const-generic ring buffer, no allocator
//    9.  Interrupts            — atomics and critical sections
//    10. State Machines        — debouncing, a traffic-light controller
//    11. Protocols             — UART frame parsing, bit-banged SPI
//    12. Fixed-Point Math      — Q16.16 arithmetic for FPU-less chips
//    13. A Sensor Driver       — an I2C temperature sensor, the driver-
//                                crate pattern
//    14. Task Scheduling       — the super-loop and a tick scheduler
//    15. Capstone              — a greenhouse controller combining it all
//
//  Build:  make          (produces the `main` binary via cargo)
//  Run:    ./main        (or `make run`)
// ============================================================================

use std::marker::PhantomData;
use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::Mutex;

/// Prints a banner separating each chapter's output.
fn chapter(title: &str) {
    println!("\n=============================================");
    println!(" {title}");
    println!("=============================================");
}

// ===========================================================================
// CHAPTER 1 — RUST FUNDAMENTALS
// ===========================================================================
// On a microcontroller every byte is visible: registers are exactly 32 bits,
// an ADC result is exactly 12, RAM is measured in KB. Rust's explicit-width
// integers (u8, u16, u32, i8...) match that world perfectly — there is no
// "int of platform-dependent size" ambiguity.

fn demo_fundamentals() {
    // Explicit integer widths. The `0b` prefix and `_` separators make
    // register values readable; suffixes like `3.3f32` pick float width.
    let status_register: u8 = 0b0001_0100;
    let adc_reading: u16 = 3102;
    let tick_count: u32 = 1_000_000;
    let temp_offset: i8 = -12;

    println!(
        "u8 status={status_register:#010b} ({} byte)  u16 adc={adc_reading} ({} bytes)",
        std::mem::size_of::<u8>(),
        std::mem::size_of::<u16>()
    );
    println!(
        "u32 ticks={tick_count} ({} bytes)  i8 offset={temp_offset} ({} byte)",
        std::mem::size_of::<u32>(),
        std::mem::size_of::<i8>()
    );

    // Variables are IMMUTABLE by default; `mut` is a visible opt-in.
    // Reading firmware, you instantly know what can change under you.
    let mut duty_cycle: u8 = 0;
    duty_cycle += 25;
    duty_cycle += 25;
    println!("duty_cycle after two bumps: {duty_cycle}%");

    // Shadowing: re-bind a name, even to a new type. Perfect for the
    // "raw reading -> engineering units" pipeline.
    let reading: u16 = 2048;
    let reading: f32 = reading as f32 * 3.3 / 4095.0;
    println!("shadowed: raw 2048 -> {reading:.3} volts");

    // `if` is an EXPRESSION — it produces a value. No ternary needed.
    let vbat_mv = 3600;
    let power_state = if vbat_mv > 3500 { "normal" } else { "low-power" };
    println!("vbat {vbat_mv} mV -> {power_state} mode");

    // `loop` with a break VALUE: retry until a condition, keep the result.
    // (Real firmware polls a status register exactly like this.)
    let mut attempts = 0;
    let calibration = loop {
        attempts += 1;
        if attempts >= 3 {
            break 42; // pretend the PLL locked and returned this value
        }
    };
    println!("calibration locked to {calibration} after {attempts} attempts");

    // Ranges and for-loops: no off-by-one C-style counters.
    print!("channels 0..4: ");
    for channel in 0..4 {
        print!("ch{channel} ");
    }
    println!();
}

// ===========================================================================
// CHAPTER 2 — OWNERSHIP & BORROWING
// ===========================================================================
// The borrow checker is not an obstacle — on a microcontroller it is the
// whole value proposition. Every value has exactly ONE owner; when the owner
// goes out of scope the value is dropped, deterministically. No garbage
// collector (no pauses, no heap requirement), no leaks, no double-frees,
// and no data races — all enforced before the code ever touches hardware.

/// A DMA buffer stand-in. NOT `Copy`: hardware may be writing into it, so
/// two owners would be a disaster. Rust makes duplication impossible unless
/// we explicitly opt in.
struct DmaBuffer {
    data: [u8; 4],
}

/// Shared borrow (&): read access, any number of readers at once.
fn checksum(buf: &DmaBuffer) -> u8 {
    buf.data.iter().fold(0, |acc, b| acc ^ b)
}

/// Exclusive borrow (&mut): write access, exactly one at a time.
/// While this borrow lives, NOBODY else can even read the buffer.
fn fill_pattern(buf: &mut DmaBuffer, seed: u8) {
    for (i, byte) in buf.data.iter_mut().enumerate() {
        *byte = seed.wrapping_add(i as u8);
    }
}

/// RAII guard for a peripheral clock — Chapter 2 of the C++ course, but the
/// compiler now also proves nobody uses the peripheral after the clock is
/// gone. Enabling in `new`, disabling in `Drop`.
struct ClockGuard {
    peripheral: &'static str,
}

impl ClockGuard {
    fn new(peripheral: &'static str) -> Self {
        println!("  [rcc] clock ENABLED for {peripheral}");
        ClockGuard { peripheral }
    }
}

impl Drop for ClockGuard {
    fn drop(&mut self) {
        println!("  [rcc] clock DISABLED for {}", self.peripheral);
    }
}

fn demo_ownership() {
    // MOVE: assignment transfers ownership. `first` is unusable afterwards —
    // uncommenting the marked line is a compile error, not a runtime bug.
    let first = DmaBuffer { data: [1, 2, 3, 4] };
    let second = first; // ownership MOVED
    // println!("{}", checksum(&first));   // ERROR: value borrowed after move
    println!("buffer moved; checksum via new owner: {:#04x}", checksum(&second));

    // BORROWS: many readers OR one writer, never both. The classic C bug —
    // an ISR reading a buffer while main() rewrites it — cannot compile.
    let mut buf = DmaBuffer { data: [0; 4] };
    fill_pattern(&mut buf, 0x10); // exclusive borrow starts and ends here
    let a = checksum(&buf); // now shared borrows are fine
    let b = checksum(&buf); // two readers simultaneously: allowed
    println!("filled buffer {:?}, checksums agree: {}", buf.data, a == b);

    // Deterministic drop: the clock turns off at the closing brace,
    // in reverse declaration order — never forgotten on any path.
    {
        let _uart_clock = ClockGuard::new("USART2");
        let _spi_clock = ClockGuard::new("SPI1");
        println!("  ...peripherals in use...");
    }
    println!("scope closed: both clocks off, zero cleanup code written");
}

// ===========================================================================
// CHAPTER 3 — ENUMS, OPTION, RESULT
// ===========================================================================
// Firmware in C signals errors with magic numbers (-1, 0xFF, NULL) that
// nothing forces callers to check. Rust encodes "might be absent" and
// "might fail" in the type system: Option<T> and Result<T, E> cannot be
// ignored without the compiler noticing.

/// Enums with DATA: each command carries exactly the fields it needs —
/// a tagged union (C course, Chapter 6) that the compiler checks for us.
enum Command {
    Reset,
    SetPwm { channel: u8, duty: u8 },
    ReadTemp,
}

/// Every error this ADC can produce, as a type. `#[derive(Debug)]` lets
/// `{:?}` print it, which is all a demo needs.
#[derive(Debug, PartialEq)]
enum AdcError {
    InvalidChannel,
    NotCalibrated,
}

/// Option<u8>: there may BE no free channel, and the type says so.
/// Returns the index of the first zero bit in a channel-usage mask.
fn find_free_channel(used_mask: u8) -> Option<u8> {
    (0u8..8).find(|&bit| used_mask & (1 << bit) == 0)
}

/// Result: success carries the reading, failure carries a typed reason.
fn read_adc(channel: u8, calibrated: bool) -> Result<u16, AdcError> {
    if channel > 7 {
        return Err(AdcError::InvalidChannel);
    }
    if !calibrated {
        return Err(AdcError::NotCalibrated);
    }
    Ok(1000 + channel as u16 * 100) // deterministic fake reading
}

/// The `?` operator: on Err, return it to OUR caller immediately; on Ok,
/// unwrap the value and continue. Error handling without exceptions and
/// without C's `if (ret != OK) goto cleanup;` pyramids.
fn average_two_channels(a: u8, b: u8) -> Result<u16, AdcError> {
    let first = read_adc(a, true)?;
    let second = read_adc(b, true)?;
    Ok((first + second) / 2)
}

/// Exhaustive matching: forget a Command variant here and the code does
/// not compile. Add a variant next year and the compiler lists every
/// match that must be updated — priceless in a 100k-line codebase.
fn execute(cmd: &Command) {
    match cmd {
        Command::Reset => println!("  -> resetting MCU"),
        Command::SetPwm { channel, duty } => {
            println!("  -> PWM channel {channel} set to {duty}%")
        }
        Command::ReadTemp => println!("  -> temperature queued for read"),
    }
}

fn demo_enums_and_errors() {
    // Option in action.
    match find_free_channel(0b0000_0111) {
        Some(ch) => println!("first free channel: {ch}"),
        None => println!("no free channels"),
    }
    println!(
        "fully used mask -> {:?} (the absence is a VALUE, not a crash)",
        find_free_channel(0xFF)
    );

    // Result in action, all paths handled.
    match read_adc(3, true) {
        Ok(v) => println!("adc channel 3 read: {v}"),
        Err(e) => println!("adc failed: {e:?}"),
    }
    println!("adc channel 9 -> {:?}", read_adc(9, true));
    println!("uncalibrated  -> {:?}", read_adc(3, false));
    println!("average_two_channels(1, 2) = {:?}", average_two_channels(1, 2));
    println!("average with bad channel   = {:?}", average_two_channels(1, 12));

    // Enum commands, exhaustively dispatched.
    let script = [
        Command::SetPwm { channel: 2, duty: 75 },
        Command::ReadTemp,
        Command::Reset,
    ];
    for cmd in &script {
        execute(cmd);
    }
}

// ===========================================================================
// CHAPTER 4 — STRUCTS, IMPL & TRAITS
// ===========================================================================
// Structs group data; `impl` blocks attach behavior; traits declare shared
// capabilities. Traits are Rust's interfaces — and (Chapter 7) the entire
// embedded ecosystem is built on them.

/// A PWM output channel. The struct owns its state; methods enforce the
/// invariant (duty never exceeds 100) — encapsulation without keywords,
/// because fields are private to the module by default.
struct PwmChannel {
    channel: u8,
    duty_percent: u8,
}

impl PwmChannel {
    /// Associated function (no self): the constructor convention.
    fn new(channel: u8) -> Self {
        PwmChannel { channel, duty_percent: 0 }
    }

    /// &mut self: mutates, requires exclusive access.
    fn set_duty(&mut self, percent: u8) {
        self.duty_percent = percent.min(100); // clamp: invariant held
    }

    /// &self: read-only accessor.
    fn duty(&self) -> u8 {
        self.duty_percent
    }
}

/// The Sensor trait: anything that can be read. `unit` has a DEFAULT
/// implementation that implementors may override — like C++ virtual
/// functions with a body, but no inheritance hierarchy required.
trait Sensor {
    fn name(&self) -> &'static str;
    fn read(&mut self) -> i32;
    fn unit(&self) -> &'static str {
        "raw"
    }
}

/// Two unrelated sensor types implementing the same trait.
struct OnboardTemp {
    last: i32,
}

impl Sensor for OnboardTemp {
    fn name(&self) -> &'static str {
        "onboard-temp"
    }
    fn read(&mut self) -> i32 {
        self.last += 1; // pretend it warms slightly each read
        self.last
    }
    fn unit(&self) -> &'static str {
        "m°C" // overrides the default
    }
}

struct Potentiometer {
    position: i32,
}

impl Sensor for Potentiometer {
    fn name(&self) -> &'static str {
        "potentiometer"
    }
    fn read(&mut self) -> i32 {
        self.position
    }
    // no unit(): inherits the "raw" default
}

fn demo_structs_and_traits() {
    let mut fan = PwmChannel::new(1);
    fan.set_duty(130); // clamped
    println!("PWM channel {} clamped to {}%", fan.channel, fan.duty());

    // DYNAMIC dispatch: &mut dyn Sensor picks the method at runtime via a
    // vtable — flexible, costs one indirection. Embedded code usually
    // prefers generics (static dispatch, Chapter 7), but dyn is ideal for
    // heterogeneous lists like this sensor bank.
    let mut temp = OnboardTemp { last: 23_000 };
    let mut pot = Potentiometer { position: 512 };
    let sensors: [&mut dyn Sensor; 2] = [&mut temp, &mut pot];
    for sensor in sensors {
        println!("  {} = {} {}", sensor.name(), sensor.read(), sensor.unit());
    }
}

// ===========================================================================
// CHAPTER 5 — BITS & REGISTERS
// ===========================================================================
// A microcontroller IS its registers: configuration means setting exact
// bits at exact addresses from the datasheet. These helpers are the same
// mask arithmetic as the C course's Chapter 11 — now applied to a
// simulated peripheral, plus the one thing C never taught: `volatile`
// access wrapped so that only ONE line of the codebase is `unsafe`.

fn set_bits(reg: u32, mask: u32) -> u32 {
    reg | mask
}

fn clear_bits(reg: u32, mask: u32) -> u32 {
    reg & !mask
}

fn toggle_bits(reg: u32, mask: u32) -> u32 {
    reg ^ mask
}

fn bit_is_set(reg: u32, bit: u32) -> bool {
    (reg >> bit) & 1 == 1
}

/// Extracts a multi-bit FIELD: shift it down, mask off the width.
/// Datasheets describe registers as packed fields ("bits 7:4 = prescaler");
/// this is how firmware reads them.
fn read_field(reg: u32, shift: u32, width: u32) -> u32 {
    (reg >> shift) & ((1 << width) - 1)
}

/// Writes a field without disturbing its neighbors: clear the field's
/// bits, then OR in the new value. Read-modify-write, the fundamental
/// register operation.
fn write_field(reg: u32, shift: u32, width: u32, value: u32) -> u32 {
    let mask = ((1u32 << width) - 1) << shift;
    (reg & !mask) | ((value << shift) & mask)
}

/// A simulated UART register block. On real hardware this struct is not
/// created — it is CAST from the datasheet's base address (e.g. STM32
/// USART2 at 0x4000_4400), which is what PACs (peripheral access crates)
/// generate from the vendor's SVD file.
struct UartRegisters {
    cr1: u32, // control register: bit 0 = enable, bits 7:4 = prescaler
    sr: u32,  // status register:  bit 5 = RX-not-empty
}

fn demo_bits_and_registers() {
    let mut reg: u32 = 0b0000_0000;
    reg = set_bits(reg, 0b0010_0010);
    println!("set bits 1,5    : {reg:#010b}");
    reg = clear_bits(reg, 0b0000_0010);
    println!("clear bit 1     : {reg:#010b}");
    reg = toggle_bits(reg, 0b1000_0000);
    println!("toggle bit 7    : {reg:#010b}");
    println!("bit 5 set? {}   bit 6 set? {}", bit_is_set(reg, 5), bit_is_set(reg, 6));

    // Field access on the simulated UART.
    let mut uart = UartRegisters { cr1: 0, sr: 0b0010_0000 };
    uart.cr1 = write_field(uart.cr1, 4, 4, 0b1010); // prescaler field = 10
    uart.cr1 = set_bits(uart.cr1, 1); // enable bit
    println!(
        "uart.cr1 = {:#010b}: prescaler field (7:4) reads back {}",
        uart.cr1,
        read_field(uart.cr1, 4, 4)
    );
    println!("uart.sr RXNE (bit 5): {}", bit_is_set(uart.sr, 5));

    // VOLATILE: the compiler may cache, reorder, or delete "unobservable"
    // memory accesses — but hardware registers change behind its back, so
    // MMIO must use volatile reads/writes. This is the course's only
    // `unsafe` block, and real firmware hides it inside the PAC exactly
    // like this. (Here the address is a stack variable; on hardware it
    // would come from the datasheet.)
    let mut simulated_mmio: u32 = 0;
    unsafe {
        std::ptr::write_volatile(&mut simulated_mmio, 0x55);
        let value = std::ptr::read_volatile(&simulated_mmio);
        println!("volatile round-trip through 'MMIO': {value:#04x}");
    }
    println!("(one unsafe block, wrapped once, audited once — the Rust deal)");
}

// ===========================================================================
// CHAPTER 6 — TYPE-STATE GPIO
// ===========================================================================
// The flagship embedded-Rust pattern. A pin's MODE (input/output) becomes a
// TYPE PARAMETER: driving a pin that is not configured as an output is a
// COMPILE ERROR, not a field-return. The zero-sized marker types and
// PhantomData vanish at compile time — the safety is free.

/// Marker types for pin modes. They carry no data (zero bytes!) — they
/// exist only to distinguish Pin<Floating> from Pin<Output> in the type
/// system.
struct Floating;
struct Output;

/// A GPIO pin, generic over its mode. `level` simulates the physical pin
/// state; real HALs write the GPIO peripheral's registers instead.
struct Pin<Mode> {
    number: u8,
    level: bool,
    _mode: PhantomData<Mode>,
}

impl Pin<Floating> {
    /// Pins come out of reset floating.
    fn new(number: u8) -> Self {
        Pin { number, level: false, _mode: PhantomData }
    }

    /// Mode change CONSUMES the pin (`self`, not `&self`) and returns a
    /// different type. The old floating handle is moved away — code that
    /// kept it around cannot compile. Ownership as a hardware invariant.
    fn into_output(self) -> Pin<Output> {
        println!("  pin {} configured as push-pull output", self.number);
        Pin { number: self.number, level: false, _mode: PhantomData }
    }
}

impl Pin<Output> {
    fn set_high(&mut self) {
        self.level = true;
    }

    fn set_low(&mut self) {
        self.level = false;
    }

    fn is_set_high(&self) -> bool {
        self.level
    }
}

fn demo_typestate_gpio() {
    let pa5 = Pin::<Floating>::new(5);
    // pa5.set_high();   // COMPILE ERROR: no such method on Pin<Floating>

    let mut pa5 = pa5.into_output(); // consumes the floating pin
    pa5.set_high();
    println!("  pin {} driven {}", pa5.number, if pa5.is_set_high() { "HIGH" } else { "LOW" });
    pa5.set_low();
    println!("  pin {} driven {}", pa5.number, if pa5.is_set_high() { "HIGH" } else { "LOW" });

    // The marker costs nothing: Pin<Output> is exactly its data fields.
    println!(
        "size_of::<Pin<Output>>() = {} bytes — the mode exists only at compile time",
        std::mem::size_of::<Pin<Output>>()
    );
}

// ===========================================================================
// CHAPTER 7 — GENERIC DRIVERS (THE EMBEDDED-HAL PATTERN)
// ===========================================================================
// The crown jewel of the ecosystem. Traits define WHAT hardware can do
// (set a pin high); each chip's HAL implements them; driver crates are
// written once against the traits and run on every chip. This is how one
// `Led` driver below drives both a GPIO pin and a relay board unchanged.

/// The contract (mirroring the real `embedded_hal::digital::OutputPin`,
/// simplified: the real one returns Result for fallible expanders).
trait OutputPin {
    fn set_high(&mut self);
    fn set_low(&mut self);
}

/// Chapter 6's pin satisfies the contract.
impl OutputPin for Pin<Output> {
    fn set_high(&mut self) {
        Pin::set_high(self);
    }
    fn set_low(&mut self) {
        Pin::set_low(self);
    }
}

/// A totally different "pin": a channel on an external relay board.
struct RelayChannel {
    id: u8,
    energized: bool,
}

impl OutputPin for RelayChannel {
    fn set_high(&mut self) {
        self.energized = true;
        println!("    [relay {}] energized (clack!)", self.id);
    }
    fn set_low(&mut self) {
        self.energized = false;
        println!("    [relay {}] released", self.id);
    }
}

/// THE driver: generic over any P that implements OutputPin. Written once;
/// works on an STM32 pin, an ESP32 pin, an I2C GPIO expander, our relay...
/// Generics compile to STATIC dispatch (monomorphization): a dedicated,
/// fully-inlined copy per pin type. No vtables on the hot path.
struct Led<P: OutputPin> {
    pin: P,
    is_on: bool,
}

impl<P: OutputPin> Led<P> {
    fn new(pin: P) -> Self {
        Led { pin, is_on: false }
    }

    fn on(&mut self) {
        self.pin.set_high();
        self.is_on = true;
    }

    fn off(&mut self) {
        self.pin.set_low();
        self.is_on = false;
    }

    fn toggle(&mut self) {
        if self.is_on {
            self.off();
        } else {
            self.on();
        }
    }
}

/// A generic function over the same trait: blinks any LED. `.` = short
/// blink, `-` = long. (Real firmware would delay between edges.)
fn morse_blink<P: OutputPin>(led: &mut Led<P>, pattern: &str) {
    print!("    blinking \"{pattern}\": ");
    for symbol in pattern.chars() {
        led.on();
        print!("{}", if symbol == '-' { "LONG " } else { "short " });
        led.off();
    }
    println!();
}

fn demo_generic_drivers() {
    // The SAME Led driver over two unrelated pin types.
    println!("  Led over a GPIO pin:");
    let gpio = Pin::<Floating>::new(13).into_output();
    let mut status_led = Led::new(gpio);
    status_led.toggle();
    println!("    gpio LED is {}", if status_led.is_on { "ON" } else { "OFF" });
    morse_blink(&mut status_led, ".-.");

    println!("  Led over a relay channel (same driver, zero changes):");
    let mut pump_indicator = Led::new(RelayChannel { id: 2, energized: false });
    pump_indicator.on();
    pump_indicator.off();
    println!(
        "    relay coil energized: {} (driver state and hardware state agree)",
        pump_indicator.pin.energized
    );
}

// ===========================================================================
// CHAPTER 8 — HEAPLESS DATA STRUCTURES
// ===========================================================================
// Most firmware runs with NO heap: allocation is nondeterministic, can
// fragment, and can fail at 3 a.m. in the field. Instead, containers have
// fixed capacities baked in at compile time and live in .bss/stack.
// Const generics make them ergonomic; the `heapless` crate is the
// production version of what we build here.

/// A ring (circular) buffer of bytes — THE structure for UART receive
/// queues: the ISR pushes bytes, the main loop pops them. Capacity N is a
/// compile-time constant; the buffer is a plain array, no allocator.
struct RingBuffer<const N: usize> {
    data: [u8; N],
    head: usize, // index of the oldest byte
    len: usize,  // number of stored bytes
}

/// Overflow is a TYPED, mandatory-to-handle event, not silent heap growth.
#[derive(Debug, PartialEq)]
struct BufferFull;

impl<const N: usize> RingBuffer<N> {
    fn new() -> Self {
        RingBuffer { data: [0; N], head: 0, len: 0 }
    }

    /// push: append at the tail. The modulo wrap reuses the array forever
    /// without shifting (the C course's queue, now generic and safe).
    fn push(&mut self, byte: u8) -> Result<(), BufferFull> {
        if self.len == N {
            return Err(BufferFull); // the firmware DECIDES: drop? overwrite?
        }
        self.data[(self.head + self.len) % N] = byte;
        self.len += 1;
        Ok(())
    }

    /// pop: remove the oldest byte. Option encodes emptiness.
    fn pop(&mut self) -> Option<u8> {
        if self.len == 0 {
            return None;
        }
        let byte = self.data[self.head];
        self.head = (self.head + 1) % N;
        self.len -= 1;
        Some(byte)
    }

    fn len(&self) -> usize {
        self.len
    }
}

fn demo_heapless() {
    // A 4-byte "UART RX queue".
    let mut rx: RingBuffer<4> = RingBuffer::new();

    for byte in [0x10u8, 0x20, 0x30, 0x40] {
        rx.push(byte).unwrap(); // capacity is known, so this cannot fail here
    }
    println!("pushed 4 bytes, len = {}", rx.len());
    println!("5th push -> {:?} (explicit policy, not a hidden malloc)", rx.push(0x50));

    print!("draining in FIFO order: ");
    while let Some(byte) = rx.pop() {
        print!("{byte:#04x} ");
    }
    println!();
    println!("pop when empty -> {:?}", rx.pop());
    println!(
        "the whole buffer is {} bytes of .bss — RAM budget known at link time",
        std::mem::size_of::<RingBuffer<4>>()
    );
}

// ===========================================================================
// CHAPTER 9 — INTERRUPTS & SHARED STATE
// ===========================================================================
// An interrupt handler preempts main() between ANY two instructions and
// shares memory with it — in C, the eternal source of heisenbugs. Rust
// refuses to compile racy access to shared statics: you must use atomics
// (lock-free, ISR-safe) or a critical section. Here a spawned thread plays
// the role of the hardware timer so the race conditions are REAL.

/// Shared tick counter. On Cortex-M the SysTick ISR increments this;
/// a plain `static mut u32` would be undefined behavior — and Rust simply
/// won't let you write that without `unsafe`.
static TICK_COUNT: AtomicU32 = AtomicU32::new(0);

/// The "interrupt handler". On real hardware this fn carries the
/// `#[interrupt]` attribute and the vector table points at it; nothing
/// about its BODY would change.
fn systick_handler() {
    TICK_COUNT.fetch_add(1, Ordering::Relaxed);
}

/// Configuration shared between main and ISRs that is too big for an
/// atomic. On Cortex-M this would be
/// `Mutex<RefCell<Config>>` from the `cortex_m` crate, locked by disabling
/// interrupts (`interrupt::free`). std's Mutex plays that role here; the
/// PATTERN — all shared state behind a lock the type system enforces — is
/// identical.
struct Config {
    sample_period_ms: u32,
    gain: u32,
}

static CONFIG: Mutex<Config> = Mutex::new(Config { sample_period_ms: 100, gain: 1 });

fn demo_interrupts() {
    // Fire the "ISR" from another thread of execution — a genuinely
    // concurrent writer, just like hardware.
    let timer = std::thread::spawn(|| {
        for _ in 0..50 {
            systick_handler();
            std::thread::sleep(std::time::Duration::from_micros(200));
        }
    });

    // main() polls the shared counter while the ISR fires — allowed,
    // because atomics are safe from both sides.
    let mut last_seen = 0;
    while last_seen < 25 {
        last_seen = TICK_COUNT.load(Ordering::Relaxed);
    }
    println!("main observed tick {last_seen} while the 'ISR' was still firing");

    timer.join().unwrap();
    println!(
        "after ISR finished: TICK_COUNT = {} (exactly 50 — no lost updates)",
        TICK_COUNT.load(Ordering::Relaxed)
    );

    // Critical section: lock, mutate, unlock at scope end.
    {
        let mut cfg = CONFIG.lock().unwrap();
        cfg.sample_period_ms = 50;
        cfg.gain = 4;
    } // lock released here — deterministic, RAII again
    let cfg = CONFIG.lock().unwrap();
    println!(
        "config updated inside critical section: period={} ms, gain=x{}",
        cfg.sample_period_ms, cfg.gain
    );
    println!("(a plain `static mut` version of this REFUSES to compile — that's the point)");
}

// ===========================================================================
// CHAPTER 10 — STATE MACHINES
// ===========================================================================
// Firmware is state machines all the way down. Rust enums make the states
// explicit and `match` makes transitions exhaustive — add a state and the
// compiler finds every transition you forgot.

/// Debouncer: a physical button "bounces" (rapid on/off chatter) for a few
/// milliseconds when pressed. The fix: only accept a new level after it
/// has been stable for `threshold` consecutive samples.
struct Debouncer {
    stable: bool,   // the accepted, debounced level
    candidate: bool, // the level we are considering switching to
    count: u8,      // how many consecutive samples agreed with candidate
    threshold: u8,
}

impl Debouncer {
    fn new(threshold: u8) -> Self {
        Debouncer { stable: false, candidate: false, count: 0, threshold }
    }

    /// Feed one raw sample; returns Some(new_level) only on an accepted
    /// edge. Called from a timer tick at a fixed rate.
    fn update(&mut self, raw: bool) -> Option<bool> {
        if raw == self.stable {
            self.count = 0; // nothing changing
            return None;
        }
        if raw == self.candidate {
            self.count += 1;
        } else {
            self.candidate = raw;
            self.count = 1;
        }
        if self.count >= self.threshold {
            self.stable = raw;
            self.count = 0;
            return Some(raw); // a REAL edge, chatter filtered out
        }
        None
    }
}

/// A traffic light: the canonical explicit state machine. Each state knows
/// its successor and its duration — the entire behavior in two matches.
#[derive(Debug, Clone, Copy, PartialEq)]
enum Light {
    Red,
    Green,
    Yellow,
}

impl Light {
    fn next(self) -> Light {
        match self {
            Light::Red => Light::Green,
            Light::Green => Light::Yellow,
            Light::Yellow => Light::Red,
        }
    }

    fn duration_ticks(self) -> u32 {
        match self {
            Light::Red => 4,
            Light::Green => 3,
            Light::Yellow => 1,
        }
    }
}

fn demo_state_machines() {
    // Debouncing a noisy press: raw samples bounce, one edge comes out.
    let noisy_samples = [
        false, true, false, true, true, true, true, // bouncy press
        true, true, false, true, false, false, false, false, // bouncy release
    ];
    let mut button = Debouncer::new(3);
    print!("debouncer edges: ");
    for (i, &raw) in noisy_samples.iter().enumerate() {
        if let Some(level) = button.update(raw) {
            print!("[sample {i}: {}] ", if level { "PRESSED" } else { "RELEASED" });
        }
    }
    println!("(15 noisy samples -> 2 clean edges)");

    // The traffic light, advanced by ticks.
    let mut light = Light::Red;
    let mut remaining = light.duration_ticks();
    print!("traffic light over 16 ticks: ");
    for _ in 0..16 {
        print!("{} ", match light {
            Light::Red => "R",
            Light::Green => "G",
            Light::Yellow => "Y",
        });
        remaining -= 1;
        if remaining == 0 {
            light = light.next();
            remaining = light.duration_ticks();
        }
    }
    println!();
}

// ===========================================================================
// CHAPTER 11 — PROTOCOLS
// ===========================================================================
// Bytes arrive one at a time, with noise, at any alignment. Firmware
// reassembles them into frames with a byte-at-a-time state machine, and
// speaks to bare chips by bit-banging clock and data lines directly.

/// Our toy UART protocol:  [0x7E][len][payload x len][checksum]
/// where checksum = XOR of the payload bytes. 0x7E marks a frame start.
const FRAME_START: u8 = 0x7E;
const MAX_PAYLOAD: usize = 16;

/// Parser states — one enum variant per protocol phase. The `expected`
/// and `received` counters live INSIDE the variant that needs them.
enum ParserState {
    Idle,                       // hunting for 0x7E
    Length,                     // next byte is the payload length
    Payload { remaining: u8 },  // collecting payload bytes
    Checksum,                   // next byte must equal the XOR
}

struct FrameParser {
    state: ParserState,
    payload: [u8; MAX_PAYLOAD],
    payload_len: usize,
}

impl FrameParser {
    fn new() -> Self {
        FrameParser { state: ParserState::Idle, payload: [0; MAX_PAYLOAD], payload_len: 0 }
    }

    /// Consumes ONE byte, advances the state machine, and returns a
    /// completed valid frame (copied out) when the checksum matches.
    /// Garbage and bad checksums quietly resynchronize to Idle — a parser
    /// that panics on line noise is a parser that bricks devices.
    fn push_byte(&mut self, byte: u8) -> Option<([u8; MAX_PAYLOAD], usize)> {
        match self.state {
            ParserState::Idle => {
                if byte == FRAME_START {
                    self.state = ParserState::Length;
                }
            }
            ParserState::Length => {
                if byte as usize > MAX_PAYLOAD || byte == 0 {
                    self.state = ParserState::Idle; // absurd length: resync
                } else {
                    self.payload_len = 0;
                    self.state = ParserState::Payload { remaining: byte };
                }
            }
            ParserState::Payload { remaining } => {
                self.payload[self.payload_len] = byte;
                self.payload_len += 1;
                self.state = if remaining == 1 {
                    ParserState::Checksum
                } else {
                    ParserState::Payload { remaining: remaining - 1 }
                };
            }
            ParserState::Checksum => {
                self.state = ParserState::Idle;
                let expected: u8 =
                    self.payload[..self.payload_len].iter().fold(0, |acc, b| acc ^ b);
                if byte == expected {
                    return Some((self.payload, self.payload_len));
                }
                // wrong checksum: frame dropped, parser already resynced
            }
        }
        None
    }
}

/// Bit-banged SPI, mode 0: for chips (or pins) without a hardware SPI
/// block, firmware wiggles the clock and data lines in software. MSB
/// first: put a bit on MOSI, raise the clock (the device samples on this
/// rising edge), lower the clock, repeat x8.
struct ShiftRegisterDevice {
    received: u8,
}

impl ShiftRegisterDevice {
    /// The simulated device: on each rising clock edge it shifts MOSI in.
    fn on_rising_edge(&mut self, mosi: bool) {
        self.received = (self.received << 1) | (mosi as u8);
    }
}

fn spi_transfer_byte(byte: u8, device: &mut ShiftRegisterDevice) {
    for bit_index in (0..8).rev() {
        let mosi = (byte >> bit_index) & 1 == 1; // set data line
        device.on_rising_edge(mosi); // raise clock: device samples
        // (clock falls here; mode 0 devices ignore the falling edge)
    }
}

fn demo_protocols() {
    let mut parser = FrameParser::new();

    // A realistic byte stream: noise, a good frame, a corrupted frame,
    // and a good frame split across two "reads".
    let stream: Vec<u8> = vec![
        0x00, 0xFF, // line noise
        0x7E, 3, b'a', b'b', b'c', b'a' ^ b'b' ^ b'c', // valid frame
        0x7E, 2, b'x', b'y', 0x00, // BAD checksum -> dropped
        0x7E, 2, b'o', // frame interrupted mid-payload...
    ];
    let stream_rest: Vec<u8> = vec![b'k', b'o' ^ b'k']; // ...finished later

    let mut frames = 0;
    for &byte in stream.iter().chain(stream_rest.iter()) {
        if let Some((payload, len)) = parser.push_byte(byte) {
            frames += 1;
            let text: String = payload[..len].iter().map(|&b| b as char).collect();
            println!("  frame {frames} received: \"{text}\" ({len} bytes)");
        }
    }
    println!("  2 valid frames recovered; noise and the bad checksum were skipped");

    // Bit-banging.
    let mut device = ShiftRegisterDevice { received: 0 };
    spi_transfer_byte(0b1011_0010, &mut device);
    println!(
        "  bit-banged SPI sent 0b1011_0010, device latched {:#010b} -> {}",
        device.received,
        if device.received == 0b1011_0010 { "match" } else { "MISMATCH" }
    );
}

// ===========================================================================
// CHAPTER 12 — FIXED-POINT MATH
// ===========================================================================
// Many MCUs (Cortex-M0/M3, most 8-bit parts) have NO floating-point
// hardware: every f32 operation becomes a slow library call. Firmware
// instead scales integers: Q16.16 keeps 16 bits of integer and 16 bits of
// fraction in an i32. Fast, deterministic, and exact for the operations
// that matter.

/// Q16.16 fixed-point number. The newtype wrapper means the type system
/// stops you from adding a raw i32 to a fixed-point value by accident.
#[derive(Clone, Copy)]
struct Fix(i32);

impl Fix {
    const SHIFT: u32 = 16;
    const ONE: i32 = 1 << Self::SHIFT;

    /// From an integer: shift left — 5 becomes 5.0 (raw 327680).
    fn from_int(v: i32) -> Fix {
        Fix(v << Self::SHIFT)
    }

    /// From a ratio: (num/den) with rounding to nearest. How constants
    /// like 0.61 enter fixed-point code without any float in sight.
    fn from_ratio(num: i32, den: i32) -> Fix {
        Fix(((num as i64 * Self::ONE as i64 + (den as i64 / 2)) / den as i64) as i32)
    }

    /// Multiply: the product of two Q16.16 numbers has 32 fraction bits,
    /// so compute in i64 and shift back down. Skipping the i64 widening
    /// is THE classic fixed-point overflow bug.
    fn mul(self, other: Fix) -> Fix {
        Fix(((self.0 as i64 * other.0 as i64) >> Self::SHIFT) as i32)
    }

    fn add(self, other: Fix) -> Fix {
        Fix(self.0 + other.0)
    }

    /// For display only: thousandths, computed in integer math.
    fn to_millis(self) -> i64 {
        (self.0 as i64 * 1000) >> Self::SHIFT
    }
}

/// ADC-to-millivolts in pure integers: (raw * vref_mv) / full_scale.
/// Widen to i64 BEFORE multiplying — 4095 * 3300 already exceeds u16.
fn adc_to_millivolts(raw: u16, vref_mv: u32) -> u32 {
    (raw as u64 * vref_mv as u64 / 4095) as u32
}

/// A TMP36-style analog sensor: 500 mV offset, 10 mV per °C.
/// millicelsius = (mV - 500) * 100. All integer, no FPU, no rounding drift.
fn tmp36_millicelsius(mv: u32) -> i32 {
    (mv as i32 - 500) * 100
}

fn demo_fixed_point() {
    // 2.5 * 3.25 = 8.125 without a single float operation.
    let a = Fix::from_ratio(5, 2); // 2.5
    let b = Fix::from_ratio(13, 4); // 3.25
    let product = a.mul(b);
    println!(
        "Q16.16: 2.500 * 3.250 = {}.{:03} (raw i32 = {})",
        product.to_millis() / 1000,
        product.to_millis() % 1000,
        product.0
    );

    let sum = Fix::from_int(1).add(Fix::from_ratio(1, 3));
    println!("Q16.16: 1 + 1/3     = {} millis (1333 expected, 1/3 is inexact in ANY binary format)", sum.to_millis());

    // The full ADC pipeline, integer end to end.
    let raw: u16 = 900;
    let mv = adc_to_millivolts(raw, 3300);
    let t = tmp36_millicelsius(mv);
    println!("ADC raw {raw} -> {mv} mV -> {}.{:02} °C, in pure integer math", t / 1000, (t % 1000) / 10);

    // Same computation in f32, for comparison. On a Cortex-M0 this line
    // costs hundreds of cycles in soft-float calls; the line above costs a
    // handful.
    let t_float = (raw as f32 * 3300.0 / 4095.0 - 500.0) / 10.0;
    println!("f32 cross-check: {t_float:.2} °C (agrees; costs ~100x more on an M0)");
}

// ===========================================================================
// CHAPTER 13 — A SENSOR DRIVER (THE DRIVER-CRATE PATTERN)
// ===========================================================================
// How the ecosystem's hundreds of driver crates work: the driver owns the
// bus (or a handle to it), is generic over the bus TRAIT, and translates
// register reads into engineering units. One driver, every chip, every
// board — because nothing in it names concrete hardware.

/// Errors a bus transaction can produce.
#[derive(Debug, PartialEq)]
enum BusError {
    Nack, // no device acknowledged the address
}

/// The I2C contract, mirroring `embedded_hal::i2c` in simplified form:
/// one combined write-then-read transaction (how register reads work:
/// write the register index, read the contents).
trait I2cBus {
    fn write_read(&mut self, address: u8, tx: &[u8], rx: &mut [u8]) -> Result<(), BusError>;
}

/// A simulated TMP102 temperature sensor living at address 0x48.
/// Register 0x00 holds the temperature: 12 bits, left-justified across
/// two bytes, 0.0625 °C per LSB — straight from the real datasheet.
struct FakeI2cBus {
    /// The sensor's current temperature in raw 12-bit form.
    tmp102_raw: u16,
}

impl I2cBus for FakeI2cBus {
    fn write_read(&mut self, address: u8, tx: &[u8], rx: &mut [u8]) -> Result<(), BusError> {
        if address != 0x48 {
            return Err(BusError::Nack); // nobody home at that address
        }
        if tx.len() == 1 && tx[0] == 0x00 && rx.len() == 2 {
            rx[0] = (self.tmp102_raw >> 4) as u8; // MSB: bits 11..4
            rx[1] = ((self.tmp102_raw & 0x0F) << 4) as u8; // LSB: bits 3..0, left-justified
        }
        Ok(())
    }
}

/// The driver. Generic over ANY bus implementing I2cBus — the fake one
/// here, a real STM32 I2C peripheral tomorrow, a Raspberry Pi's i2cdev in
/// an integration test. The driver code never changes.
struct Tmp102<B: I2cBus> {
    bus: B,
    address: u8,
}

impl<B: I2cBus> Tmp102<B> {
    fn new(bus: B, address: u8) -> Self {
        Tmp102 { bus, address }
    }

    /// Reads the temperature register and converts to millicelsius.
    /// raw * 0.0625 °C = raw * 62.5 m°C = raw * 625 / 10 — integer math
    /// (Chapter 12), and `?` propagates bus errors (Chapter 3).
    fn read_millicelsius(&mut self) -> Result<i32, BusError> {
        let mut rx = [0u8; 2];
        self.bus.write_read(self.address, &[0x00], &mut rx)?;
        let raw = ((rx[0] as u16) << 4) | ((rx[1] as u16) >> 4);
        Ok(raw as i32 * 625 / 10)
    }
}

fn demo_sensor_driver()
{
    // 25.0 °C = 400 LSBs of 0.0625 °C.
    let bus = FakeI2cBus { tmp102_raw: 400 };
    let mut sensor = Tmp102::new(bus, 0x48);

    match sensor.read_millicelsius() {
        Ok(mc) => println!("TMP102 reads {}.{:03} °C", mc / 1000, mc % 1000),
        Err(e) => println!("read failed: {e:?}"),
    }

    // Wrong address: the error is a value, handled where it occurs.
    let bad_bus = FakeI2cBus { tmp102_raw: 400 };
    let mut ghost = Tmp102::new(bad_bus, 0x13);
    println!("driver at wrong address 0x13 -> {:?}", ghost.read_millicelsius());
    println!("the driver is generic over I2cBus: fake bus today, real silicon tomorrow");
}

// ===========================================================================
// CHAPTER 14 — TASK SCHEDULING
// ===========================================================================
// Most shipped firmware is not an RTOS — it is a SUPER-LOOP: wake on tick,
// run whatever is due, sleep. A tick-based cooperative scheduler is 30
// lines and covers a huge share of products. (When you outgrow it, look at
// RTIC and Embassy — interrupt-driven and async schedulers for Rust.)

/// One periodic task: a name, a period, when it next runs, and a plain
/// function pointer (`fn(u32)` — no heap, no closures needed).
struct Task {
    name: &'static str,
    period_ticks: u32,
    next_run: u32,
    run: fn(u32),
}

/// A fixed-capacity scheduler — const generics again: the task table's
/// size is part of the type, in .bss, known at link time.
struct Scheduler<const N: usize> {
    tasks: [Task; N],
}

impl<const N: usize> Scheduler<N> {
    fn new(tasks: [Task; N]) -> Self {
        Scheduler { tasks }
    }

    /// One tick of the super-loop: run everything that is due.
    /// `next_run += period` (rather than `next_run = now + period`)
    /// prevents drift when a task runs late — a subtle, load-bearing
    /// detail in real schedulers.
    fn tick(&mut self, now: u32) {
        for task in self.tasks.iter_mut() {
            if now >= task.next_run {
                (task.run)(now);
                task.next_run += task.period_ticks;
            }
        }
    }
}

fn heartbeat_task(now: u32) {
    println!("  [tick {now:2}] heartbeat: LED toggled");
}

fn sensor_task(now: u32) {
    println!("  [tick {now:2}] sensors: readings taken");
}

fn telemetry_task(now: u32) {
    println!("  [tick {now:2}] telemetry: packet sent");
}

fn demo_scheduler() {
    let mut scheduler = Scheduler::new([
        Task { name: "heartbeat", period_ticks: 2, next_run: 0, run: heartbeat_task },
        Task { name: "sensors", period_ticks: 5, next_run: 0, run: sensor_task },
        Task { name: "telemetry", period_ticks: 6, next_run: 3, run: telemetry_task },
    ]);

    for task in scheduler.tasks.iter() {
        println!("task '{}' every {} ticks (first at {})", task.name, task.period_ticks, task.next_run);
    }
    for now in 0..12 {
        scheduler.tick(now);
        // real firmware: wait-for-interrupt (WFI) here — the MCU sleeps
        // between ticks and the SysTick interrupt wakes it. Milliamps
        // become microamps.
    }
}

// ===========================================================================
// CHAPTER 15 — CAPSTONE: A GREENHOUSE CONTROLLER
// ===========================================================================
// Everything at once, in one deterministic simulation:
//   - the TMP102 driver over the fake I2C bus     (Ch 13, 7)
//   - integer ADC conversion for soil moisture    (Ch 12)
//   - a debounced override button                 (Ch 10)
//   - heater + pump driven through OutputPin      (Ch 6, 7)
//   - an event log in a heapless ring buffer      (Ch 8)
//   - a tick-driven control loop                  (Ch 14)
//   - Result/Option handling throughout           (Ch 3)

/// Event codes for the log — one byte each, because the log is a byte
/// ring buffer exactly like a real black-box event log in RAM.
const EV_HEATER_ON: u8 = 1;
const EV_HEATER_OFF: u8 = 2;
const EV_PUMP_ON: u8 = 3;
const EV_PUMP_OFF: u8 = 4;
const EV_BUTTON: u8 = 5;

fn event_name(code: u8) -> &'static str {
    match code {
        EV_HEATER_ON => "heater-on",
        EV_HEATER_OFF => "heater-off",
        EV_PUMP_ON => "pump-on",
        EV_PUMP_OFF => "pump-off",
        EV_BUTTON => "button-press",
        _ => "?",
    }
}

fn run_capstone() {
    // Hardware: sensor on the bus, actuators behind the OutputPin trait.
    let bus = FakeI2cBus { tmp102_raw: 288 }; // 18.0 °C to start
    let mut thermometer = Tmp102::new(bus, 0x48);
    let mut heater = Led::new(Pin::<Floating>::new(6).into_output());
    let mut pump = Led::new(RelayChannel { id: 1, energized: false });
    let mut button = Debouncer::new(2);
    let mut log: RingBuffer<16> = RingBuffer::new();

    // Scripted inputs make the run reproducible (same trick as the C++
    // course's mini game): soil readings drain, the button bounces once.
    let soil_raw: [u16; 10] = [2600, 2500, 2400, 1500, 1400, 1500, 2900, 3000, 3100, 3200];
    let button_raw: [bool; 10] =
        [false, false, true, true, false, false, false, false, false, false];

    const SETPOINT_MC: i32 = 21_000; // 21 °C
    const SOIL_DRY_MV: u32 = 1300; // below this: water the plants
    let mut pump_override = false;

    println!("setpoint {}.0 °C; soil dry threshold {} mV\n", SETPOINT_MC / 1000, SOIL_DRY_MV);

    for tick in 0..10u32 {
        // --- SENSE ------------------------------------------------------
        let temp_mc = thermometer.read_millicelsius().unwrap_or(SETPOINT_MC);
        let soil_mv = adc_to_millivolts(soil_raw[tick as usize], 3300);

        // --- INPUT ------------------------------------------------------
        if button.update(button_raw[tick as usize]) == Some(true) {
            pump_override = !pump_override;
            let _ = log.push(EV_BUTTON);
        }

        // --- CONTROL ----------------------------------------------------
        // Heater: bang-bang control around the setpoint.
        let want_heat = temp_mc < SETPOINT_MC;
        if want_heat != heater.is_on {
            if want_heat {
                heater.on();
                let _ = log.push(EV_HEATER_ON);
            } else {
                heater.off();
                let _ = log.push(EV_HEATER_OFF);
            }
        }

        // Pump: on when soil is dry, or forced by the override button.
        let want_pump = pump_override || soil_mv < SOIL_DRY_MV;
        if want_pump != pump.is_on {
            if want_pump {
                pump.on();
                let _ = log.push(EV_PUMP_ON);
            } else {
                pump.off();
                let _ = log.push(EV_PUMP_OFF);
            }
        }

        // --- REPORT -----------------------------------------------------
        println!(
            "tick {tick}: temp {:2}.{:03} °C  soil {:4} mV  heater {}  pump {}{}",
            temp_mc / 1000,
            temp_mc % 1000,
            soil_mv,
            if heater.is_on { "ON " } else { "off" },
            if pump.is_on { "ON " } else { "off" },
            if pump_override { "  [override]" } else { "" }
        );

        // --- PLANT PHYSICS (the simulated world) -------------------------
        // The heater warms the greenhouse by 0.75 °C per tick, otherwise
        // it cools by 0.25 °C. (12 raw LSB = 0.75 °C at 0.0625 °C/LSB.)
        if heater.is_on {
            thermometer.bus.tmp102_raw += 12;
        } else {
            thermometer.bus.tmp102_raw -= 4;
        }
    }

    print!("\nevent log (from the ring buffer): ");
    while let Some(code) = log.pop() {
        print!("{} ", event_name(code));
    }
    println!();
    println!("deterministic run: scripted inputs + integer math = same output every time");
}

// ===========================================================================
// MAIN — runs every chapter in order
// ===========================================================================

fn main() {
    println!("embedded Rust course — simulated MCU, real patterns");

    chapter("1. Rust fundamentals");
    demo_fundamentals();

    chapter("2. Ownership & borrowing");
    demo_ownership();

    chapter("3. Enums, Option, Result");
    demo_enums_and_errors();

    chapter("4. Structs, impl & traits");
    demo_structs_and_traits();

    chapter("5. Bits & registers");
    demo_bits_and_registers();

    chapter("6. Type-state GPIO");
    demo_typestate_gpio();

    chapter("7. Generic drivers (embedded-hal pattern)");
    demo_generic_drivers();

    chapter("8. Heapless data structures");
    demo_heapless();

    chapter("9. Interrupts & shared state");
    demo_interrupts();

    chapter("10. State machines");
    demo_state_machines();

    chapter("11. Protocols: UART framing & bit-banged SPI");
    demo_protocols();

    chapter("12. Fixed-point math");
    demo_fixed_point();

    chapter("13. A sensor driver (TMP102 over I2C)");
    demo_sensor_driver();

    chapter("14. Task scheduling");
    demo_scheduler();

    chapter("15. Capstone: greenhouse controller");
    run_capstone();

    println!("\nAll chapters completed successfully.");
}
