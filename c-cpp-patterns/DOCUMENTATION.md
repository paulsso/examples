# Patterns & Anti-Patterns in C and C++ — Course Text

This is the course. Unlike the mechanics-focused courses in this repository,
the deliverable here is *judgment*: the ability to recognize recurring shapes
in C and C++ code, to understand **why the bad shapes keep appearing** (every
anti-pattern is a locally reasonable decision), and to know the established
fixes along with their limits.

Each chapter follows the same structure:

- **The anti-pattern** — what it looks like in the wild.
- **Why it emerges** — the honest, usually sympathetic origin story.
- **What it costs** — the failure mode, and when the bill arrives.
- **The pattern** — the fix, in C and in C++ where they differ.
- **Nuance** — when the "anti-pattern" is actually acceptable, and when the
  "pattern" can be overdone. This section is the reason the course exists.

`main.cpp` runs every chapter's anti-pattern and pattern side by side
(labelled `ANTI` / `GOOD`). Disasters that would be undefined behavior are
**simulated with logging counters** (a fake resource registry, a fake heap)
so they are visible in the output without ever being executed;
`make sanitize` verifies the program itself is UB-clean.

```bash
make            # build
./main          # run all chapters
make sanitize   # ASan + UBSan build and run — must be clean
```

---

## Table of Contents

1. [Ownership & Cleanup](#chapter-1--ownership--cleanup)
2. [Error Handling](#chapter-2--error-handling)
3. [Magic Values](#chapter-3--magic-values)
4. [Global State](#chapter-4--global-state)
5. [Buffers & Strings](#chapter-5--buffers--strings)
6. [Undefined Behavior](#chapter-6--undefined-behavior)
7. [Function Shape](#chapter-7--function-shape)
8. [The Preprocessor](#chapter-8--the-preprocessor)
9. [Inheritance Abuse](#chapter-9--inheritance-abuse)
10. [Copies & Ownership](#chapter-10--copies--ownership)
11. [Concurrency](#chapter-11--concurrency)
12. [Design for Change](#chapter-12--design-for-change)

---

## Chapter 1 — Ownership & Cleanup

### The anti-pattern: leak-on-early-return

`leaky_load` acquires two resources, hits a validation failure, and returns
— past the cleanup. With manual resource management, *every* return
statement is a separate cleanup site, and the set of return statements only
grows over time.

### Why it emerges

The function was correct when written: one acquisition, one return. Then a
second resource was added, then an early validation check, each change
individually reasonable. Manual cleanup requires **global** knowledge of the
function to make a **local** edit — that's the trap.

### What it costs

Leaks concentrate in error paths, which are precisely the paths tests
exercise least. The program works for months, then falls over under the
exact conditions (resource exhaustion, failing dependencies) where it most
needs to behave. The demo makes this visible: the handle counter reads 2
after the "failed" call.

### The patterns

- **C: single-exit `goto cleanup`.** All cleanup lives at one labelled
  block at the bottom, in reverse acquisition order; every failure jumps to
  it. This is the house style of the Linux kernel and most serious C
  codebases. Note the inversion: **goto — famously "harmful" — is the
  pattern here.** Judgement over dogma.
- **C++: RAII** (`ScopedHandle`). The destructor releases the resource, so
  there is no cleanup path to forget — every return, every exception, every
  early exit added years later is covered. Copying is deleted because two
  owners of one handle is the Chapter 10 disaster.

### Nuance

`goto` cleanup is only disciplined when jumps go strictly forward/downward
to one label. RAII's cost is that cleanup becomes invisible — a reader must
know the types to know what a closing brace does; name RAII types loudly
(`ScopedHandle`, `lock_guard`). If a destructor can meaningfully *fail*
(e.g. a buffered writer whose final flush hits a full disk), RAII alone
can't report it — offer an explicit `close()` that returns a status, with
the destructor as the last-resort backstop.

---

## Chapter 2 — Error Handling

### The anti-patterns: sentinel soup and ignored returns

`find_user_bad` returns −1 for failure; `count_sessions_bad` returns 0 —
which is also a legitimate count. Every function invents its own
convention, the compiler checks none of them, and `ignored_write` shows the
end state: a return value that reports data loss, silently dropped at the
call site.

### Why it emerges

In-band sentinels are free: no new types, no out-parameters, and −1 *is*
unambiguous for the first function that uses it. The convention decays one
function at a time, and by then consistency would require touching every
caller.

### What it costs

Callers must memorize per-function conventions; the 0-vs-failure ambiguity
turns into wrong branches; ignored returns turn detectable failures
(short writes, partial reads) into silent corruption discovered much later.

### The patterns

- **C: one status enum per module + out-parameters** (`Status`,
  `find_user_good`). Uniform and boring — the success/failure channel is
  fully separated from the data channel. Attributes like
  `__attribute__((warn_unused_result))` make ignoring the status a warning.
- **C++: a `Result<T, E>` type** that physically couples the value to its
  validity — an unchecked "value" cannot exist. The course implements a
  20-line teaching version; the production versions are `std::optional`
  (absence without a reason), `std::expected` (C++23), and exceptions.

### Nuance

Exceptions are a legitimate pattern, not an anti-pattern — but they are a
*global* architectural decision (exception safety everywhere, RAII
mandatory) rather than a local one, and much of the industry (embedded,
games, kernels) builds with `-fno-exceptions`. What is *always* an
anti-pattern is a mixed economy: some functions throwing, some returning
codes, some doing both. errno deserves mention: global, easily clobbered,
and only meaningful immediately after a failing call — historical baggage
to interoperate with, not to imitate.

---

## Chapter 3 — Magic Values

### The anti-patterns: magic numbers and the boolean trap

`create_window_bad(1280, 720, true, false, true)` — the demo runs it, and
the call site tells the reader nothing. Magic numbers are the same disease
in numeric form: `if (state == 3)`.

### Why it emerges

Literals are always available; adding a name requires deciding where the
name lives. Booleans creep in one flag at a time — the first `bool` is
readable, the third makes the signature write-only.

### What it costs

Reading code means keeping a decoder ring in your head. Transposed
arguments of the same type (`bool`, `bool`, `bool` or `int width, int
height`) compile silently. The extreme version killed a spacecraft: the
Mars Climate Orbiter was lost because one team's pound-seconds met another
team's newton-seconds in an untyped interface.

### The patterns

- **Named constants** (`constexpr`, enums) — the name is the documentation.
- **Options structs** (`WindowOptions`) with named, defaulted fields:
  self-documenting call sites, and adding an option no longer breaks or
  reorders every caller. (C99+ gets the same effect with designated
  initializers: `.fullscreen = true`.)
- **Strong types / the newtype idiom** (`Meters`, `Feet`): wrapping a
  double in a one-field struct is free at runtime, and `runway + altitude`
  with mixed units becomes a *compile error*. The demo forces the
  conversion through `to_meters`.

### Nuance

Not every literal is magic: `0`, `1`, and obvious identities (`% 2` for
parity) need no names, and naming them (`const int TWO = 2;`) is its own
anti-pattern — noise that buries the real constants. Strong types earn
their keep at *unit and identity boundaries* (meters/feet, user-id vs
order-id); wrapping every `int` in the program is ceremony without payoff.

---

## Chapter 4 — Global State

### The anti-pattern: action at a distance

`module_a_tune_for_slow_network()` silently changes what `module_b_send()`
does. Nothing in B's code, signature, or call site hints at the coupling —
the demo prints B behaving differently after a call it knows nothing about.

### Why it emerges

Globals are the path of least resistance for making data visible to code
that didn't receive it — threading a parameter through five call layers is
real work today, and the global costs nothing until later.

### What it costs

Functions can no longer be understood (or tested) in isolation; behavior
depends on invocation order; every global is shared state under concurrency
(Chapter 11); and C++ adds the **static initialization order fiasco** —
globals in different translation units initialize in unspecified order, so
one global depending on another may read garbage before `main`.

### The patterns

- **Explicit context** (`NetConfig`, passed as a parameter): the dependency
  appears in the signature, callers control it, tests construct any variant
  they want.
- **When one instance is genuinely required** (a logger, a registry): the
  **construct-on-first-use** function-local static (`global_logger()`),
  which fixes the initialization-order fiasco and is thread-safe since
  C++11. This is the least-bad singleton.

### Nuance

Immutable constants at namespace scope are fine — the problems above are
about *mutable* globals. The singleton's true cost is not the single
instance but the **globally reachable mutable state** and the hidden
dependency edges it invites; passing the logger explicitly to the few
places that need it is often less code than people fear. Dependency
injection here means "take it as a parameter", not a framework.

---

## Chapter 5 — Buffers & Strings

### The anti-patterns: the CVE generators

`gets` reads without any bound (removed in C11 — too dangerous to keep).
`strcpy`/`sprintf`/`strcat` write without one. And the "safe-looking" fix,
`strncpy`, has its own trap: when the source fills the destination, it
writes **no terminator**, and the next `printf("%s")` walks off the end.
These stay in `// UB:` comments — they are not safely runnable.

### Why it emerges

The original C library API predates the security era; the names everyone
learns first are the broken ones; and `strncpy`'s actual purpose (fixed-
width fields in old Unix structures, deliberately unterminated) is unknown
to almost everyone who reaches for it.

### What it costs

Stack smashing, heap corruption, and several decades of remote-code-
execution advisories. Buffer overflows remain at the top of the CWE most-
dangerous-weakness lists.

### The patterns

- **C: size-carrying, always-terminating, truncation-reporting APIs.** The
  course implements `bounded_copy` (an `strlcpy`): copies what fits,
  *unconditionally* terminates, returns the full source length so
  `returned >= dst_size` detects truncation — the demo shows the truncated
  copy being detected rather than silent. `snprintf` follows the same
  contract for formatting.
- **Non-owning views:** pass `(pointer, length)` pairs instead of assuming
  termination — standardized in C++17 as `std::string_view` (demoed
  viewing a prefix without copying).
- **C++: `std::string`**, which removes the entire category — growth,
  termination and lifetime are the type's problem.

### Nuance

Truncation-on-overflow is *detectable* but still a policy decision: a
truncated file path is a different file, and a truncated command is a
different command — sometimes the right behavior on overflow is to fail
the operation, not to truncate. And `string_view`'s non-ownership cuts both
ways: it is a borrow (Chapter 10's language), and returning one that
outlives the string it views is a dangling reference in new clothes.

---

## Chapter 6 — Undefined Behavior

### The concept — UB is a contract, not a crash

Undefined behavior does not mean "the program crashes"; it means **the
compiler is licensed to assume it never happens** and optimizes under that
assumption. `if (x + 1 < x)` as a signed-overflow check can be deleted —
overflow is UB, therefore the compiler may assume it cannot occur,
therefore the test is always false. Code with UB doesn't have a bug *at*
the UB site; it has no defined meaning at all, and the symptom appears
wherever the optimizer took the assumption.

The bestiary (all in `// UB:` comments): signed overflow, out-of-bounds
access, dangling pointers/references, uninitialized reads, strict-aliasing
violations, unsequenced modification (`i = i++` — caught live in the C
course's development, fittingly), data races.

### Why it exists

The standard leaves these undefined so implementations don't pay for
checks on every operation — it is the source of much of C/C++'s speed. The
anti-pattern is not UB's existence; it is *writing code that commits it
while expecting a particular behavior* ("it wraps on my machine").

### The patterns (all runnable in the demo)

- **Overflow pre-checks** (`safe_add`): test against `INT_MAX`/`INT_MIN`
  *before* the operation, using arithmetic that cannot itself overflow.
- **Unsigned for modular arithmetic**: unsigned wraparound is fully
  defined — `UINT_MAX + 1 == 0` by the standard, ideal for tick counters
  and hashes.
- **`memcpy` for type punning** (`float_bits`): reinterpreting bytes via
  pointer casts breaks strict aliasing; `memcpy` has defined semantics and
  compiles to the same single instruction. (C++20 adds `std::bit_cast`.)
- **Initialize everything**: `int counters[4] = {}` — value-initialization
  as a habit eliminates uninitialized reads.
- **Tooling as part of the pattern**: `-Wall -Wextra -pedantic` at compile
  time; AddressSanitizer and UBSanitizer at test time (`make sanitize`
  runs this very program under both — clean, which is the proof that the
  ANTI demos are simulations).

### Nuance

Compilers offer dialects with defined behavior (`-fwrapv` makes signed
overflow wrap) — legitimate as an explicit, documented engineering
decision, dangerous as an unstated assumption, because the code is then
correct only under that flag. Defensive checks also have a cost pattern of
their own: sprinkling `if (ptr != NULL)` where null is impossible doesn't
make code safer, it makes the *actual* contracts illegible. Decide where
null is legal, document it, and check at those boundaries.

---

## Chapter 7 — Function Shape

### The anti-patterns: arrow code and the god function

`validate_order_arrow` nests four conditions deep; the success path hides
at maximum indentation and each `else` sits a screen away from its `if`.
Scaled up, this becomes the god function: hundreds of lines, a dozen
locals, doing validation, business logic, I/O and formatting at once.

### Why it emerges

Code grows by accretion: each new requirement is one more nested `if`
inside the existing structure, and no single addition justifies a
restructuring. Deep nesting is what "just add it here" compounds into.

### What it costs

Comprehension: the reader must carry the whole condition stack in their
head. Testing: N nested conditions produce paths that combinatorially
explode. Modification: inserting a new rule means finding the right level
of an indentation pyramid.

### The patterns

- **Guard clauses** (`validate_order_guards`): reject early, one condition
  per line, success path flat at the bottom. The demo runs both versions on
  identical inputs — same behavior, different legibility.
- **Table-driven logic** (`HTTP_TABLE`): when branching selects among *data*
  (code → message, state → handler), store the mapping as data and look it
  up. Adding an entry is a row, not a branch; the C course's function-
  pointer dispatch table is the same pattern one level up.
- **Decomposition**: one function, one job; the god function becomes a
  short coordinator calling named steps.

### Nuance

Single-exit ("one return per function") was good advice *for C with manual
cleanup* (Chapter 1) and is obsolete as a blanket rule in C++ — guard
clauses are the readable form when cleanup is automatic. Decomposition can
overshoot: fifty three-line functions, each called once, can be harder to
follow than one coherent thirty-line function. The unit of extraction is a
*nameable concept*, not a line count.

---

## Chapter 8 — The Preprocessor

### The anti-patterns: macros doing a function's job

`TWICE(fake_random())` calls the RNG **twice** — the demo counts it. With
`i++` as the argument, double evaluation graduates to unsequenced
modification (UB, Chapter 6). `#define` constants add their own problems:
no type, no scope, invisible to debuggers, and silent expansion anywhere
the token appears.

### Why it emerges

For decades the preprocessor *was* the only tool for generic code,
inlining, and named constants in C. The habits are inherited, and the
macros in old headers still teach them.

### What it costs

Call-site behavior that depends on argument side effects; name collisions
across the entire program (`#define min` breaking `<algorithm>` is a
classic); error messages pointing at expansions rather than definitions.

### The patterns

- **`inline`/ordinary functions** (`twice_fn`): evaluate arguments exactly
  once, obey types and scope, and — the demo shows — the RNG counter reads
  1, not 2.
- **`constexpr`** (`MAX_CLIENTS`, `half_of`): compile-time computation with
  real semantics; everything `#define` offered, plus a type and namespace.
  In C, `enum { MAX_CLIENTS = 64 }` is the classic scoped integer constant.
- **The legitimate X-macro** (`PACKET_KINDS`): define a list once, expand
  it into an enum *and* a matching name table. The two can never drift
  apart — which is exactly the bug hand-maintained parallel lists always
  eventually grow. Some jobs still genuinely need token pasting and
  stringification; this is the disciplined form.

### Nuance

The preprocessor is not deprecated: include guards, conditional
compilation for platforms, feature detection, and X-macro-style code
generation remain legitimate and irreplaceable in C. The rule of judgment:
**reach for the preprocessor when you need text manipulation, not when you
need a function or a constant.** When writing an unavoidable function-like
macro, parenthesize every argument and the whole body, and name it in
SHOUTING_CASE so call sites know to be careful.

---

## Chapter 9 — Inheritance Abuse

### The anti-patterns: is-a everything, and slicing

Inheriting to *reuse code* rather than to satisfy an interface produces
hierarchies where a subclass inherits methods that make no sense for it —
the classic Duck problem: `RobotDuck`, `RubberDuck`, and combinatorial
subclasses for every behavior mix. And passing polymorphic objects **by
value** triggers slicing: `Animal a = dog;` copies only the `Animal` part.
The demo runs it — the sliced object says `"..."` while a reference to the
same dog says `"woof!"`.

### Why it emerges

Inheritance is taught as *the* OO mechanism, and it delivers instant
gratification: subclass, inherit everything, override one method. Slicing
happens because value semantics is C++'s default and nothing at the
assignment site looks wrong.

### What it costs

Deep hierarchies fossilize: a change to a base class ripples into every
descendant, and cross-cutting behaviors (the robot duck that squeaks)
have no home. Slicing silently discards state and behavior; the related
lifetime trap — deleting a derived object through a base pointer whose
destructor isn't `virtual` — is UB (noted, not executed).

### The patterns

- **Polymorphism through references/pointers only**; polymorphic base
  classes get `virtual` destructors, and (a good default) are made
  non-copyable to turn slicing into a compile error.
- **Small pure interfaces** (`QuackBehavior`) rather than fat base classes
  with data — closer to what Chapter 7 of the C++ course called the
  embedded-hal idea: contracts, not taxonomies.
- **Composition over inheritance** (`Duck` *has a* voice): behavior as a
  swappable component. The demo swaps a duck's voice at runtime — no new
  subclass invented.

### Nuance

Interface inheritance (pure virtual bases — Chapter 12 of the C++ course's
`IRenderer`) is the legitimate, load-bearing use of the mechanism;
"composition over inheritance" targets *implementation* inheritance.
Guidance, not dogma: shallow hierarchies (one interface, N
implementations) are excellent; it's depth and data-in-base that rot.
`final` and `override` document intent and let the compiler check it.

---

## Chapter 10 — Copies & Ownership

### The anti-pattern: shallow copy of an owning type

`ShallowBuffer` owns a heap block and keeps the compiler-generated copy
operations. Copying it duplicates the *handle*, not the resource — two
owners, one block, and both destructors free it. The demo simulates the
heap with logging: `DOUBLE FREE of block 42!` appears in the output instead
of corrupting a real allocator. This is the **rule of three/five**
violated: if a class needs a custom destructor, it almost certainly needs
custom (or deleted) copy/move operations too.

### Why it emerges

The compiler writes the copy operations silently, and they are *correct*
until the first raw owning handle appears as a member. Nothing warns at
the moment the invariant breaks.

### What it costs

Double frees and use-after-frees — heap corruption with symptoms far from
causes. Also the quieter cost: accidental deep copies of large objects
passed by value, invisible until profiled.

### The patterns

- **Rule of zero first** (`Document`): own resources through members that
  already manage themselves (`std::string`, `std::vector`, smart
  pointers). Zero special member functions written; all six correct by
  construction. The demo copies a `Document` and mutates the copy —
  independent, automatically.
- **Rule of five when truly managing a resource** — written out in full in
  the C++ course's `VertexBuffer`; this course adds the judgment: that
  code belongs in *one* low-level wrapper type, not scattered across the
  codebase.
- **Measure copies instead of guessing** (`Tracked`): an instrumented type
  counting its own copies and moves. The demo shows pass-by-value = 1
  copy, pass-by-`const&` = 0, and `push_back(std::move(t))` = 0 copies +
  1 move — with the note that the move constructor being `noexcept` is
  what lets `std::vector` use it during reallocation.

### Nuance

Value semantics is a C++ *strength* — copies of value types are what make
code local and reasoning easy. The anti-pattern is unintended copies of
*owning* or *large* types, not copying per se. Passing small structs
(Chapter 3's `Meters`) by value is ideal. And `shared_ptr` is not "the
safe default": it is shared *ownership* with atomic refcount traffic and
unclear lifetime responsibility — `unique_ptr` is the default; `shared_ptr`
is a design statement.

---

## Chapter 11 — Concurrency

### The anti-pattern: the read-modify-write hole

A true data race (two threads on a plain `int`) is UB and cannot be
demonstrated legally — so the demo runs its well-defined cousin:
`counter.store(counter.load() + 1)` from four threads. Every access is
atomic, there is **no UB**, and yet the run loses tens of thousands of
updates (printed live), because *check-then-act spans a gap*: another
thread writes between the load and the store. This is the arithmetic of
the data-race bug, reproduced safely — and it generalizes: `if
(!map.contains(k)) map.insert(k)` has the same hole with a lock held for
each half.

### Why it emerges

`x++` looks atomic in source. The mental model of "one line = one step"
fails exactly here, and (on x86 especially) the racy version *usually*
passes tests — the failure needs contention and timing.

### What it costs

Lost updates, corrupted invariants, and — for the plain-`int` version —
undefined behavior, meaning the optimizer may do anything at all with the
code around it. Race bugs are load-dependent: rare in test, constant in
production.

### The patterns

- **Atomic read-modify-write** (`fetch_add`): one indivisible operation;
  the demo's count is exact. The unit of atomicity must cover the whole
  *decision*, not each memory access.
- **`std::mutex` + `lock_guard`** for anything larger than one variable —
  which is Chapter 1's RAII applied to locks: unlock cannot be forgotten
  on any path.
- **Structural patterns** (in the text, beyond the demo): immutable data
  needs no synchronization; thread confinement (one owner per datum,
  Chapter 8/14 of the Rust course made this a compiler guarantee); message
  passing over shared mutation.

### Nuance

The historical anti-patterns worth naming: **`volatile` is not a
concurrency tool in C/C++** (it suppresses optimization, provides no
atomicity or ordering — its legitimate use is MMIO, as in the embedded
course), and **pre-C++11 double-checked locking** was famously broken
until the C++11 memory model plus atomics made correct forms expressible
(construct-on-first-use statics, Chapter 4, are the simple fix). Fine-
grained locking and relaxed memory orderings are real tools with real
costs; the default is a coarse mutex and `fetch_add`, refined only under
measurement (Chapter 12's rule).

---

## Chapter 12 — Design for Change

### The anti-patterns: hidden dependencies and premature pessimization

`session_expired_bad` calls the wall clock *inside* — its result depends on
when the test suite runs, so no stable assertion exists. And on the
performance side, the inverse of the famous warning: **premature
pessimization** — habitually writing the slower version when the fast one
costs nothing, e.g. growing a vector through repeated reallocation when
`reserve()` is one line (the demo times both).

### Why it emerges

Calling `time()`/`rand()` directly is the obvious way to get time and
randomness; the test-time consequence is invisible while writing. The
pessimizations come from the opposite misreading of "don't optimize
prematurely" — as permission never to think about cost at all.

### What it costs

Untestable code calcifies: what can't be tested doesn't get refactored.
Pessimization costs quietly and diffusely — no single hot spot, just a
program slower everywhere than it needed to be.

### The patterns

- **Inject dependencies** (`session_expired_good(started, now)`): time,
  randomness, and I/O enter as parameters or interfaces. Production passes
  the real clock; tests pass any moment they like — the demo asserts both
  sides of the 30-minute boundary deterministically. (The Rust course's
  seeded RNG and the C++ course's scripted inputs are this same pattern.)
- **Sensible defaults are not premature optimization**: `reserve()` when
  the size is known, pass-by-`const&` for big objects (measured in
  Chapter 10), moving instead of copying. These are habits, not tuning.
- **`const` as an enforced contract**: a `const&` parameter or `const`
  method is documentation the compiler checks — the API says "I only
  read", and callers can rely on it.
- **Measure before real optimization** — the C++ course's AoS/SoA
  benchmark is the model: a claim, an experiment, numbers.

### Nuance

Knuth's line — "premature optimization is the root of all evil" — comes
from a paper *about* optimizing, and continues "...yet we should not pass
up our opportunities in that critical 3%". Both halves matter: default to
clarity, adopt free efficiencies as habits, and earn complex optimizations
with a profiler. Likewise dependency injection: a parameter or a small
interface is the pattern; a framework of factories building factories is
the same idea overdosed.

---

## Closing: a working vocabulary

| Anti-pattern | Chapter | Pattern that answers it |
|---|---|---|
| Leak on early return | 1 | `goto` cleanup (C), RAII (C++) |
| Sentinel soup, ignored returns | 2 | Status enums, `Result`/`expected` |
| Magic numbers, boolean trap | 3 | Named constants, options structs, strong types |
| Mutable globals, casual singletons | 4 | Explicit context, construct-on-first-use |
| Unbounded string ops | 5 | Size-carrying APIs, `string`/`string_view` |
| "It wraps on my machine" | 6 | Pre-checks, `memcpy` punning, sanitizers |
| Arrow code, god functions | 7 | Guard clauses, tables, decomposition |
| Function-like macros, `#define` constants | 8 | `inline`, `constexpr`, X-macros where earned |
| Hierarchy-as-reuse, slicing | 9 | Interfaces, composition, references |
| Shallow copies of owners | 10 | Rule of zero, rule of five, measurement |
| Check-then-act across a gap | 11 | Atomic RMW, `lock_guard`, immutability |
| Hidden clock/RNG, casual waste | 12 | Injection, sensible defaults, profiling |

## Suggested Course Progression

| Stage | Chapters | Exercises to assign |
|---|---|---|
| Foundations | 1–3 | Refactor a provided leaky C function to goto-cleanup, then to RAII; convert a boolean-trap API to an options struct |
| Structure | 4–7 | Remove a global from a small program by threading context; flatten a nested validator with guard clauses; convert a switch to a table |
| Language edges | 8–10 | Replace a macro utility header with `constexpr`; find the rule-of-three violation in a provided class; instrument and count copies in a hot loop |
| Systems | 11–12 | Fix a check-then-act bug three ways (mutex, atomic RMW, redesign); make an untestable time-dependent function deterministic |
