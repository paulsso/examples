# Patterns & Anti-Patterns in C and C++

A **conceptual course** on the recurring good and bad ideas of C and C++
programming. Unlike the other courses in this repository — which teach
mechanics — this one teaches *judgment*: recognizing the shapes that code
grows into, understanding why the bad shapes keep appearing, and knowing the
established fixes (and their limits).

The course text is [DOCUMENTATION.md](DOCUMENTATION.md). For every chapter it
covers: what the anti-pattern looks like, **why it emerges** (they are all
locally reasonable decisions), what it costs, the pattern that fixes it, and
— crucially — **when the "anti-pattern" is actually acceptable**. Nuance is
the point: `goto` cleanup is a *pattern* in C, singletons are occasionally
right, and premature *pessimization* is as real as premature optimization.

`main.cpp` is the course's living illustration: each chapter **runs the
anti-pattern and the pattern side by side** (labelled `ANTI` / `GOOD` in the
output), so the difference is observable rather than asserted.

## Chapters

1. **Ownership & Cleanup** — leaks on early return; `goto` cleanup (the C pattern); RAII
2. **Error Handling** — sentinel soup, ignored returns; status enums; a `Result<T, E>` type
3. **Magic Values** — magic numbers, the boolean trap; options structs; strong types (Meters vs Feet)
4. **Global State** — action at a distance; explicit context; the least-bad singleton
5. **Buffers & Strings** — `gets`/`strcpy`/`strncpy` traps; an `strlcpy`-style bounded copy; `std::string`/`string_view`
6. **Undefined Behavior** — the bestiary; overflow pre-checks, `memcpy` punning, value-init, sanitizers
7. **Function Shape** — arrow code vs guard clauses; table-driven logic
8. **The Preprocessor** — double evaluation; `constexpr`/`inline`; the legitimate X-macro
9. **Inheritance Abuse** — object slicing; hierarchy-as-reuse; composition
10. **Copies & Ownership** — shallow-copy double free (simulated); rule of zero/five; measuring copies
11. **Concurrency** — the read-modify-write hole (demonstrated legally); `fetch_add`; `lock_guard`
12. **Design for Change** — hidden clock dependencies; premature pessimization; `const` as contract

## How the dangerous demos work

Several anti-patterns (leaks, double frees, data races) are undefined
behavior when executed for real. **This program never invokes UB.** Disasters
are *simulated with logging counters* — a fake resource registry counts
leaked handles, a fake heap reports the double free — so the failure appears
in the output while the program stays well-defined. Where even that is
impossible, the bad code appears in comments marked `// UB:`. The
`make sanitize` target proves the claim: an AddressSanitizer + UBSanitizer
build runs clean.

## Building and running

Requires `g++` (or any C++17 compiler) and `make`.

```bash
make            # build the `main` binary
./main          # run all chapters (ANTI vs GOOD, side by side)
make run        # build and run in one step
make debug      # rebuild with -g -O0 for debugging
make sanitize   # ASan+UBSan build and run — must be clean
make clean      # remove binaries
```

The build uses `-std=c++17 -Wall -Wextra -pedantic -pthread` and compiles
warning-free.

## Layout

| File | Purpose |
|---|---|
| `DOCUMENTATION.md` | **The course text** — concepts, causes, costs, nuance |
| `main.cpp` | Runnable side-by-side demonstrations |
| `Makefile` | Builds the `main` binary; includes a sanitizer target |
