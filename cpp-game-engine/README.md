# C++ Game Engine Course Examples

A comprehensive set of examples for a course in modern C++ (C++17) built
around one concrete goal: developing a **fast, cross-platform 2D game engine
from scratch**. All examples are implemented in a single file, `main.cpp`,
organized into fifteen chapters — each one a real engine subsystem:

1. **From C to C++** — references, overloading, default arguments, namespaces, `auto`
2. **Classes & RAII** — resource handles (textures) that free themselves
3. **Game Math** — `Vec2` with operator overloading, lerp/clamp, `Mat3` 2D transforms
4. **Templates & the STL** — a generic `Grid<T>` (tilemaps), `sort`/`unordered_map`/algorithms
5. **Polymorphism** — abstract interfaces, `override`/`final`, virtual dispatch
6. **Move Semantics & Smart Pointers** — the rule of five, `unique_ptr`/`shared_ptr`/`weak_ptr`
7. **Memory & Performance** — arena allocator, object pool, AoS vs SoA benchmark
8. **Entity Component System** — entities as IDs, components as data, systems as functions
9. **Game Loop & Timing** — `std::chrono` clock, fixed timestep with accumulator, render interpolation
10. **Events & Input** — event bus (observer pattern), deferred dispatch, key-to-action mapping
11. **Collision & Physics** — AABB and circle tests, penetration resolution, semi-implicit Euler
12. **Renderer Abstraction** — `IRenderer` interface with a console (ASCII framebuffer) backend, flipbook animation
13. **Pathfinding & AI** — A* on a grid, finite state machine
14. **Concurrency** — a job system (thread pool), parallel sums, atomics
15. **Mini Game** — "Asteroid Dodge": input, update, collision, and rendering combined into a deterministic playable simulation

Every function is documented in [DOCUMENTATION.md](DOCUMENTATION.md), with
explanations of the concepts, engine-design rationale, and pitfalls it
teaches.

## Why a console renderer?

Cross-platform engines never let game code call platform APIs directly —
everything draws through an abstract interface with a backend per platform.
The course's reference backend renders to an ASCII framebuffer in the
terminal: it needs zero dependencies, runs anywhere (including headless CI),
and proves the abstraction. Swapping in an SDL/OpenGL/Metal backend later
requires no changes to game code — that is the point.

## Building and running

Requires `g++` (or any C++17 compiler) and `make`.

```bash
make          # build the `main` binary
./main        # run all chapters in order
make run      # build and run in one step
make debug    # rebuild with -g -O0 for debugging with gdb
make clean    # remove the binary
```

The build uses `-std=c++17 -Wall -Wextra -pedantic -pthread` and compiles
warning-free.

## Layout

| File | Purpose |
|---|---|
| `main.cpp` | All example implementations, organized by chapter |
| `DOCUMENTATION.md` | Written explanation of every function |
| `Makefile` | Builds the `main` binary |
