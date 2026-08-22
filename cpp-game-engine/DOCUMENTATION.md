# C++ Game Engine Course — Documentation

This document explains every function and class implemented in `main.cpp`.
The code is organized as fifteen **chapters** that take a student from
beginner C++ to the advanced techniques used in real game engines — with one
running goal: a **fast, cross-platform 2D game engine built from scratch**.
Each chapter implements an actual engine subsystem; running the program
(`make run`) walks through all of them in order.

Build and run:

```bash
make        # builds the `main` binary
./main      # run it (or `make run`)
make debug  # rebuild with -g -O0 for use with gdb
make clean  # remove the binary
```

The build uses `g++ -std=c++17 -Wall -Wextra -pedantic -O2 -pthread` and
compiles warning-free.

---

## Table of Contents

1. [Chapter 1 — From C to C++](#chapter-1--from-c-to-c)
2. [Chapter 2 — Classes & RAII](#chapter-2--classes--raii)
3. [Chapter 3 — Game Math](#chapter-3--game-math)
4. [Chapter 4 — Templates & the STL](#chapter-4--templates--the-stl)
5. [Chapter 5 — Polymorphism & Virtual Dispatch](#chapter-5--polymorphism--virtual-dispatch)
6. [Chapter 6 — Move Semantics & Smart Pointers](#chapter-6--move-semantics--smart-pointers)
7. [Chapter 7 — Memory & Performance](#chapter-7--memory--performance)
8. [Chapter 8 — Entity Component System](#chapter-8--entity-component-system)
9. [Chapter 9 — Game Loop & Timing](#chapter-9--game-loop--timing)
10. [Chapter 10 — Events & Input](#chapter-10--events--input)
11. [Chapter 11 — Collision & Physics](#chapter-11--collision--physics)
12. [Chapter 12 — Renderer Abstraction](#chapter-12--renderer-abstraction)
13. [Chapter 13 — Pathfinding & AI](#chapter-13--pathfinding--ai)
14. [Chapter 14 — Concurrency: A Job System](#chapter-14--concurrency-a-job-system)
15. [Chapter 15 — Mini Game](#chapter-15--mini-game)

---

## Design principles the course teaches

Three themes run through every chapter:

- **Fast**: no heap allocation on the hot path (Chapter 7's allocators),
  data laid out for the CPU cache (AoS vs SoA, ECS), work spread across
  cores (Chapter 14), and measurements instead of guesses (`std::chrono`).
- **Cross-platform**: game code never touches a platform API directly.
  Everything goes through abstract interfaces (Chapter 12's `IRenderer`);
  the console backend proves the abstraction with zero dependencies, and a
  future SDL/OpenGL/Metal backend slots in without touching game code.
- **From scratch**: every subsystem — math, allocators, ECS, event bus,
  collision, renderer, pathfinding, thread pool — is implemented in this
  file, not imported.

### `chapter(const std::string& title)`

Prints a banner separating each demo's output. Where the C course used a
macro, C++ prefers a function: type-checked, debuggable, and scoped.

---

## Chapter 1 — From C to C++

### `apply_damage(int& hp, int amount)`

Takes its parameter by **reference** (`int&`) — an alias for the caller's
variable. References replace C's out-pointers: no null possibility, no `*`
or `&` noise at the call site, same machine code underneath.

### `describe(int)` / `describe(float)` / `describe(const std::string&)`

Function **overloading**: three functions share one name and the compiler
selects by argument type at each call site. In C these needed three names
(`describe_int`, `describe_float`, ...). Note the string overload takes
`const std::string&` — pass-by-const-reference avoids copying, the default
idiom for anything bigger than a pointer.

### `spawn_enemy(const std::string& name, int hp = 100, int level = 1)`

**Default arguments**: trailing parameters may be omitted by callers. Engine
APIs use this for sensible spawn defaults.

### `namespace units`

Namespaces prevent name collisions between modules and third-party
libraries. `constexpr` values (`PIXELS_PER_METER`, `GRAVITY`) are computed
at compile time and replace C's `#define` constants — typed, scoped, and
debugger-visible.

### `demo_cpp_basics()`

Also shows `auto` type deduction, range-based `for`, `std::vector` and
`std::string` — containers that manage their own memory, eliminating the
`malloc`/`strcpy`/`free` choreography of the C course.

---

## Chapter 2 — Classes & RAII

**RAII (Resource Acquisition Is Initialization)** is the single most
important C++ idiom: acquire a resource in a constructor, release it in the
destructor. Cleanup then happens automatically, in deterministic order, on
every exit path. Textures, sounds, files and GPU buffers all become
self-cleaning classes.

### `class TextureHandle`

Simulates owning a GPU texture:

- **Constructor** — "uploads" the texture and logs it. `explicit` prevents
  accidental implicit conversions from `std::string`. `std::move(name)`
  transfers the string into the member instead of copying it.
- **Destructor** — "frees" it. The demo's log output shows destructors
  firing at each closing brace, in reverse order of construction.
- **Deleted copy operations** — `= delete` makes copying a compile error:
  two handles freeing the same GPU texture would be a double-free, so the
  type itself forbids the mistake.
- **`static int next_id_`** — one counter shared by all instances, used to
  hand out unique ids.

### `class Player`

Encapsulation in a plain gameplay class: state (`hp_`) is private and can
only change through methods that enforce the rules — `take_damage` clamps at
0, `heal` clamps at max. **`const` methods** (`is_alive`, `hp`, `name`)
promise not to modify the object; the compiler enforces the promise, letting
them be called through const references.

### `demo_raii()`

Nested scopes show automatic, deterministic destruction; then the `Player`
demonstrates that invariants hold (130 damage cannot push hp below zero).

---

## Chapter 3 — Game Math

Every 2D engine sits on a small math library. **Operator overloading** makes
it read like math: `pos += vel * dt`.

### `struct Vec2`

A 2D vector with default member initializers (`Vec2{}` is the zero vector).
It is a plain struct with public members — math types are *value types* with
no invariant to protect, so getters would be noise.

- **`length()`** — magnitude, √(x² + y²).
- **`normalized()`** — same direction, length 1; returns the zero vector
  unchanged (guarding division by zero).

### Free operators: `+`, `-`, `*` (both orders), `+=`, `operator<<`

Defined as free functions so both operands are treated symmetrically.
`operator<<` teaches how to make any type printable with `std::cout` —
invaluable for debugging.

### `dot(Vec2 a, Vec2 b)` → `float`

The dot product, |a||b|cos θ. Sign answers "is the target in front of me?";
used constantly for facing checks, projections and lighting.

### `cross2d(Vec2 a, Vec2 b)` → `float`

The 2D cross product (the z of the 3D cross). Its **sign** says whether `b`
is clockwise or counter-clockwise of `a` — turning decisions and winding
tests.

### `lerp(Vec2 a, Vec2 b, float t)` → `Vec2`

Linear interpolation: `a + (b - a) * t`. The most-used function in game
programming — camera smoothing, blends, fades, and the render interpolation
of Chapter 9.

### `clampf(float v, float lo, float hi)` → `float`

Constrains a value to a range: keeps players in the world and volume in 0..1.
Also reused by the circle-vs-AABB test in Chapter 11.

### `deg_to_rad(float degrees)` → `float`

Trig functions take radians; designers think in degrees. Convert once, at
the boundary.

### `struct Mat3`

A 3×3 matrix for 2D transforms, defaulting to identity. Using 3×3 rather
than 2×2 puts points in **homogeneous coordinates** (a hidden trailing 1),
which makes *translation* expressible as a matrix — so translate, rotate and
scale all compose with one multiplication, exactly how real engines build
transform hierarchies.

- **`Mat3::translation(Vec2)` / `Mat3::rotation(float)` / `Mat3::scale(Vec2)`**
  — named constructors for the three fundamental transforms.
- **`operator*(const Mat3&)`** — matrix multiplication composes transforms.
  **Order matters**: `T * R * S` scales first, then rotates, then translates.
- **`transform_point(Vec2)`** — multiplies (x, y, 1) by the matrix; the demo
  verifies a point is scaled ×2, rotated 90°, then moved to (100, 50).

---

## Chapter 4 — Templates & the STL

Templates generate type-safe code per type at **compile time** — the
zero-cost alternative to the C course's `void *` generics.

### `my_clamp<T>(T value, T lo, T hi)` → `T`

A function template: one definition, instantiated for `int`, `float`, or any
comparable type. (`std::clamp` exists since C++17; writing it shows how
little magic is involved.)

### `class Grid<T>`

A 2D grid stored as **one flat `std::vector`** — a single allocation and
contiguous memory, unlike a vector-of-vectors. Element (x, y) lives at index
`y * width + x` (row-major, same layout as the C course's matrices).

- **`at(x, y)`** — element access, with a `const` overload so const grids
  are readable.
- **`in_bounds(x, y)`** — the bounds predicate used by clipping (Chapter 12)
  and A* neighbor checks (Chapter 13).

This one template is reused as tilemap, framebuffer, pathfinding map, and
path-overlay view — demonstrating why generic containers matter.

### `demo_templates_and_stl()`

The STL tour, in engine terms:

- `std::sort` with a **lambda** comparator ordering sprites by layer —
  literally what a 2D renderer does to its draw list every frame.
- `std::unordered_map<std::string, int>` — O(1) average lookup, the shape of
  every asset registry.
- `std::find_if` with a predicate lambda, and `std::accumulate` to total a
  damage log.

---

## Chapter 5 — Polymorphism & Virtual Dispatch

Virtual functions let code operate on a base-class **interface** while
derived classes supply behavior, selected at runtime through the vtable.
The engine-scale payoff arrives in Chapter 12, where a renderer interface
hides the platform.

### `class Enemy` (abstract base)

- **`virtual void attack() const = 0`** — *pure* virtual: `Enemy` cannot be
  instantiated; derived classes must implement it.
- **`virtual void update(float dt)`** — virtual with a default body
  (drift right); derived classes may override or inherit it.
- **`virtual ~Enemy() = default`** — the essential virtual destructor:
  deleting a derived object through a base pointer without one is undefined
  behavior.

### `class Goblin` / `class Dragon final`

- **`using Enemy::Enemy`** — inherits the base constructor.
- **`override`** — asks the compiler to verify the function really overrides
  a virtual; catches signature typos that would otherwise silently create a
  new, unrelated function.
- **`final`** on `Dragon` — forbids further derivation and enables
  devirtualization optimizations.
- `Dragon::update` overrides movement (flies up); `Goblin` inherits the
  default.

### `demo_polymorphism()`

Iterates both enemies through `Enemy*` pointers — each call dispatches to
the right override. Includes two teaching notes: storing polymorphic objects
*by value* would **slice** off the derived parts, and `dynamic_cast` is the
safe, nullptr-returning downcast (used sparingly; needing it often signals a
design smell).

---

## Chapter 6 — Move Semantics & Smart Pointers

Large resources are expensive to copy; move semantics **transfer ownership**
instead — the destination steals the internals, the source is left empty but
valid. Smart pointers then encode ownership in the type system so `delete`
disappears from engine code entirely.

### `class VertexBuffer` — the Rule of Five

Owns a heap array of floats (stand-in for GPU vertex data). Because it
manages a raw resource, it defines all five special member functions, each
logging so the demo output shows exactly when copies and moves happen:

| Member | Cost | What it does |
|---|---|---|
| destructor | — | `delete[]` the buffer (skips logging when moved-from) |
| copy constructor | O(n) | allocates and duplicates every float |
| copy assignment | O(n) | copy-then-swap-style: allocate first, then replace (self-assignment safe, exception safe) |
| move constructor | O(1) | steals the pointer, nulls the source |
| move assignment | O(1) | frees own buffer, steals, nulls the source |

The move operations are **`noexcept`** — this matters practically:
`std::vector` only moves elements during reallocation if the move cannot
throw, otherwise it copies.

### `Texture` / `Sprite` and the shared-asset demo

`std::shared_ptr<Texture>` reference-counts an asset shared by two sprites:
`use_count()` shows 3 owners; the texture dies exactly when the last owner
releases it. `std::weak_ptr` observes without owning — after all owners
reset, `expired()` reports the texture is gone. This is asset lifetime
management in miniature.

### `demo_move_and_smart_pointers()`

Also demonstrates `std::unique_ptr` + `std::make_unique` for exclusive
ownership (the Chapter 2 `TextureHandle` freed automatically, no `delete`),
and `std::move` converting an lvalue into an rvalue so the move constructor
is chosen.

---

## Chapter 7 — Memory & Performance

"Fast" is an engineering requirement. General-purpose `new`/`malloc` is slow
and fragments memory, so engines allocate from custom allocators on the hot
path — and lay data out for the CPU cache, which is roughly two orders of
magnitude faster than main memory.

### `class ArenaAllocator`

The simplest, fastest allocator: one big block and a **bump pointer**.

- **`allocate(size, alignment)`** — rounds the offset up to the required
  alignment with the classic bit trick
  `(offset + align - 1) & ~(align - 1)`, hands out the slot, bumps the
  offset. Returns `nullptr` when exhausted.
- **`alloc_array<T>(n)`** — typed wrapper deriving size and alignment from
  `T`.
- **`reset()`** — frees *everything* in O(1) by zeroing the offset. There is
  deliberately no per-object free: arenas suit per-frame scratch data —
  allocate all frame, reset once at frame end.
- Copying is deleted (the arena owns raw memory, same reasoning as
  `TextureHandle`).

### `class ObjectPool<T, Capacity>`

Pre-allocates `Capacity` objects in a `std::array` and recycles them through
a **free list** of slot indices:

- **`acquire()`** — O(1): pops a free index, returns a pointer to that slot;
  `nullptr` when exhausted (running out of bullets is handled, not fatal).
- **`release(T*)`** — O(1): pointer arithmetic (`object - slots_.data()`)
  recovers the slot index, which is pushed back onto the free list.
- **`available()`** — free slots remaining.

Bullets, particles, audio voices — anything spawned and destroyed constantly
— live in pools in production engines, so the hot path never touches the
heap.

### AoS vs SoA — `ParticleAoS` / `ParticlesSoA`

The data-layout lesson behind ECS. Array-of-Structs stores whole particles
contiguously, so updating only positions still drags every particle's unused
bytes (lifetime, size, color, flags) through the cache. Struct-of-Arrays
keeps each field in its own array, so the position loop touches only
position bytes. `demo_memory_and_performance()` times both layouts over
200,000 particles × 20 steps with `std::chrono::steady_clock` and prints
both timings plus checksums (which also stop the compiler from deleting the
loops). SoA typically wins ~2× here and grows with unused payload.

---

## Chapter 8 — Entity Component System

The architecture of modern engines (Unity's DOTS, Unreal's Mass, EnTT,
flecs). Instead of inheritance trees (`Enemy` → `FlyingEnemy` →
`FlyingFireEnemy`…):

- an **entity** is just an ID (`using Entity = std::uint32_t`),
- **components** are plain data structs attached to IDs — `TransformComp`,
  `VelocityComp`, `HealthComp` here (no methods, no virtuals),
- **systems** are functions that process every entity holding the components
  they need.

Composition replaces inheritance, and data is laid out for iteration.

### `class World`

Owns entities and component storage.

- **`create_entity()`** — an entity is born as a fresh ID, nothing more.
- **`destroy_entity(Entity)`** — removes the ID and erases its components
  from every table.
- **Component tables** — `std::unordered_map<Entity, T>` per component type.
  This teaching version favors clarity; production ECS packs components
  into dense arrays for the Chapter 7 SoA cache wins. The *API shape* is
  identical.

### `movement_system(World&, float dt)`

For every entity with both a velocity and a transform:
`position += velocity * dt`. It knows nothing about goblins or bullets —
*anything* with those two components moves. That is the whole idea.

### `poison_system(World&, int damage)`

Ticks damage on every entity with health. Adding a mechanic = adding a
system; nothing else changes.

### `reap_system(World&)` → `std::size_t`

Collects dead entities first, *then* destroys them — **deferred
destruction**, because destroying while iterating would invalidate the map
being walked. A real ECS lesson in miniature. Returns the number reaped.

### `demo_ecs()`

Builds a player (moves + health), an arrow (moves, no health), and a chest
(health, static) purely by attaching different components, then runs three
frames of all systems. The chest dies to poison on frame 2 and is reaped.

---

## Chapter 9 — Game Loop & Timing

The engine's heartbeat. Frames take variable real time, but physics must be
deterministic — solved by the **fixed timestep with accumulator** pattern
(Glenn Fiedler's "Fix Your Timestep!"), used by every serious engine.

### `class Clock`

Thin wrapper over `std::chrono::steady_clock` — *monotonic*, so it never
jumps when the OS clock changes (`system_clock` does; using it for frame
deltas is a real shipped-game bug).

- **`restart()`** — returns seconds since the previous call as `float`
  (via `std::chrono::duration<float>`): the frame delta.

### `demo_game_loop()`

First measures a real busy-loop with `Clock`, then runs the fixed-timestep
pattern over **synthetic frame times** (16, 34, 8, 25, 41 ms) so the output
is deterministic and inspectable:

```
accumulator += frame_time;
while (accumulator >= dt) { simulate(dt); accumulator -= dt; }
alpha = accumulator / dt;
render(lerp(previous_state, current_state, alpha));
```

- The 34 ms frame runs 3 physics steps to catch up; the 8 ms frame runs 0
  and just re-renders. Physics always sees exactly `dt` — deterministic
  regardless of frame rate.
- **`alpha`** measures how far into the *next* step the frame landed; the
  renderer draws `lerp(prev_state, state, alpha)` (Chapter 3's lerp),
  eliminating visual stutter without touching the simulation.

---

## Chapter 10 — Events & Input

Systems must communicate without depending on each other: audio should not
`#include` combat. The **event bus** decouples them.

### `enum class EventType` + `event_type_name`

Scoped enums (`enum class`) don't leak names or implicitly convert to int —
prefer them over C-style enums. The name function is the standard printing
pattern.

### `struct Event`

A small tagged record: a type plus two int payload fields whose meaning
depends on the type. Real engines use `std::variant` or per-type structs;
two ints keep the pattern in plain sight.

### `class EventBus`

- **`subscribe(EventType, Handler)`** — registers a callback for one event
  type. `Handler` is `std::function<void(const Event&)>`, whose *type
  erasure* accepts lambdas, free functions, or bound member functions
  uniformly — the **observer pattern**.
- **`publish(const Event&)`** — appends to a queue; *nothing runs yet*.
- **`dispatch()`** — drains the queue, invoking every subscriber of each
  event's type. Deferred delivery means callbacks never fire in the middle
  of another system's update — events are processed at one controlled point
  in the frame.

The demo has audio, UI and quest systems subscribing independently; combat
publishes without knowing any of them exist.

### Input mapping — `enum class Action` + `action_name`

Raw key codes never appear in gameplay code. Keys map to abstract **actions**
(`MoveLeft`, `Jump`, `Fire`) in a single `std::unordered_map<char, Action>`
table; gameplay reads actions. Rebinding keys or adding gamepad support then
touches exactly one place — a cornerstone of cross-platform input. The demo
translates a keystroke string and flags unbound keys.

---

## Chapter 11 — Collision & Physics

2D games mostly need two shapes — axis-aligned boxes and circles — plus
simple integration. That covers platformers, shooters, and most arcade
genres.

### `struct AABB` + `aabb_overlap(a, b)` → `bool`

Axis-aligned bounding box stored as min/max corners. Because the boxes never
rotate, overlap is four comparisons: they overlap unless one is entirely
left/right/above/below the other (separating-axis logic in its simplest
form).

### `aabb_penetration(a, b)` → `Vec2`

For overlapping boxes, computes the push distance along each of the four
directions and returns the smallest one as a vector that moves `a` out of
`b`. Resolving along the **least-penetration axis** is what makes characters
slide along walls instead of sticking to them.

### `struct CircleShape` + `circle_overlap(a, b)` → `bool`

Circles overlap when center distance < sum of radii — compared as **squared
distances** (`dot(d, d) < r * r`), avoiding the `sqrt` entirely. Free
performance, standard idiom.

### `circle_vs_aabb(c, box)` → `bool`

Clamp the circle's center to the box (Chapter 3's `clampf`, per axis) to get
the **closest point** on the box, then a point-in-circle test. One clamp
turns a hard-looking problem into an easy one.

### `struct Body` + `integrate(Body&, Vec2 gravity, float dt)`

**Semi-implicit Euler**: update velocity *first*, then position with the new
velocity. One line different from naive Euler, but noticeably more stable at
game timesteps — it's what most shipping 2D engines actually use.

### `demo_collision_and_physics()`

Resolves a player/wall overlap, tests bomb-blast circles against a crate and
a wall, then drops a ball under gravity with restitution 0.6 bouncing off
the floor.

---

## Chapter 12 — Renderer Abstraction

The cross-platform chapter. Game code **never calls a platform API
directly**: it draws through an abstract interface, and each platform gets a
backend (OpenGL, Metal, DirectX, console…). Here the "platform" is the
terminal — a backend with zero dependencies that runs anywhere, including
headless CI.

### `class IRenderer` (interface)

Pure-virtual drawing contract: `clear`, `draw_glyph`, `draw_text`,
`present`, `width`, `height`, plus the obligatory virtual destructor. This
is Chapter 5's polymorphism applied at engine scale: game code holds an
`IRenderer&` and cannot name the concrete backend.

### `class ConsoleRenderer final : public IRenderer`

The terminal backend, built on `Grid<char>` from Chapter 4 as an off-screen
character **framebuffer**:

- **`clear(char)`** — fills the buffer with a background glyph.
- **`draw_glyph(x, y, char)`** — writes one cell, silently **clipping**
  off-screen draws via `in_bounds` (renderers never crash on out-of-view
  geometry).
- **`draw_text(x, y, string)`** — a row of glyphs.
- **`present()`** — flushes the buffer to stdout in one pass, framed by a
  border. Draw calls mutate the off-screen buffer; only `present` shows it —
  **double buffering**, the same architecture as a GPU swap chain, in ASCII.

### `struct Animation`

Flipbook animation: cycles through glyph frames at `seconds_per_frame`,
driven by the frame **delta time** — never by frame count, or the game
animates faster on faster machines (the classic beginner mistake). The
`while` loop in `update` handles frames long enough to skip several
animation frames.

### `draw_scene(IRenderer&, int hero_x, char hero_glyph)`

Game-side drawing that only knows `IRenderer` — it would compile unchanged
against an OpenGL backend, which is the entire point of the abstraction.

### `demo_renderer()`

Renders three frames of a hero walking right while its glyph animates
through `| / - \`.

---

## Chapter 13 — Pathfinding & AI

Two workhorses: A* answers *where to go*; finite state machines decide
*what to do*.

### `struct GridPos`

Integer tile coordinates, deliberately distinct from the float `Vec2` of
physics — mixing the two coordinate spaces is a classic bug source.

### `find_path_astar(const Grid<int>& map, GridPos start, GridPos goal)` → `std::vector<GridPos>`

A* over a tile grid (0 = walkable, 1 = wall), 4-directional movement.

A* is Dijkstra plus a **heuristic**: it always expands the node with the
lowest `f = g + h`, where `g` is the true cost from the start and `h`
estimates the remaining cost. With an *admissible* heuristic (Manhattan
distance never overestimates on a 4-connected grid) the result is optimal.

Implementation details worth teaching:

- The open set is a min-heap: `std::priority_queue` over `(f, x, y)` tuples
  with `std::greater` flipping the default max-heap.
- Stale heap entries are skipped rather than updated (**lazy deletion**) —
  far simpler than a decrease-key operation and standard practice.
- `came_from` records each tile's predecessor; the path is reconstructed
  backwards from the goal, then reversed.
- Returns an empty vector when the goal is unreachable.

The demo builds a map with a wall and renders the found path onto a
`Grid<char>` view (`S`, `G`, `*`, `#`).

### `class Guard` — a finite state machine

States `Patrol` / `Chase` / `Attack` with transitions driven by distance to
the player:

- **`update(Vec2 player_pos)`** — one AI tick: compute distance, select the
  desired state by thresholds (>6 patrol, >1.5 chase, else attack), log the
  transition if the state changed, then run the current state's behavior
  (chase steps toward the player using Chapter 3's `normalized()`).

FSMs make AI explicit and debuggable: the guard is in exactly one state at
any moment and every transition is a visible event. Most game AI up to
mid-size titles is FSMs. `guard_state_name` is the enum-printing pattern
again.

### `demo_pathfinding_and_ai()`

Runs A* around the wall, then feeds the guard a scripted approach-engage-
flee sequence of player positions: Patrol → Chase → Attack → Patrol.

---

## Chapter 14 — Concurrency: A Job System

Modern CPUs have many cores; engines feed them through a **job system**:
worker threads consuming small tasks from a shared queue. Physics,
animation, particles and streaming all become jobs.

### `class ThreadPool`

~60 lines containing the heart of every production job system:

- **Constructor** — spawns N workers, each running `worker_loop`.
- **`submit(std::function<void()>)`** — pushes a job under the mutex and
  wakes one sleeping worker (`notify_one`).
- **`wait_idle()`** — blocks until the queue is empty **and** no job is
  running (`active_jobs_ == 0`). This is the *frame barrier*: "all jobs
  finished, safe to render".
- **`worker_loop()`** (private) — waits on the `work_available_` condition
  variable until stopped or work exists; pops a job and increments
  `active_jobs_` under the lock; **runs the job outside the lock** (holding
  it would serialize all workers); then decrements and notifies `all_done_`
  when everything is finished.
- **Destructor** — sets the stop flag under the lock, wakes all workers,
  joins every thread. Workers drain remaining jobs before exiting.
- Copying is deleted; threads and mutexes are not copyable resources.

Two condition variables separate the two directions of signaling: workers
wait on "work available"; `wait_idle` waits on "all done".

### `demo_job_system()`

- **Parallel sum** — a 1M-element array split into 4 chunks; each job writes
  its result to its *own slot* of a pre-sized vector, so no two jobs share
  memory: parallelism **without locks**. `wait_idle` is the barrier; the
  result is verified against a serial `std::accumulate`.
- **Atomics** — 8 jobs each increment `std::atomic<int>` 10,000 times;
  the count is exactly 80,000. A plain `int` here would be a data race
  (undefined behavior) and would lose updates in practice.
  `memory_order_relaxed` is sufficient for a pure counter — a first look at
  memory orderings.
- Ends with the practical rule: parallelize *big* work; tiny jobs cost more
  to schedule than they save.

---

## Chapter 15 — Mini Game

**"Asteroid Dodge"** — everything combined into a complete, playable game
loop: asteroids fall, the player dodges.

### `struct Asteroid`

Integer tile coordinates; this game snaps to the character grid.

### `run_mini_game()`

Each frame executes the canonical loop — **input → update → collision →
render**:

1. **Input** — a scripted keystroke string (`'a'` left, `'d'` right, `'.'`
   stay) is translated to movement, clamped to the field (Chapter 10's
   mapping idea; swap in real keyboard reads for interactivity).
2. **Update** — asteroids spawn on even frames at columns drawn from
   `std::mt19937` seeded with 42, and everything falls one row.
3. **Collision** — grid-exact: an asteroid on the player's tile is a hit;
   one that reaches the bottom row is dodged. Survivors are kept via the
   collect-then-swap pattern (`std::move` into the vector — Chapter 6).
4. **Render** — through `IRenderer` (Chapter 12), with a score line drawn
   as text.

The fixed seed plus scripted inputs make every run identical — the same
determinism that powers game replays and daily-challenge modes, and the
capstone lesson of the course: **deterministic simulation + abstracted
platform = a portable, testable engine**.

---

## Suggested Course Progression

| Stage | Chapters | Exercises to assign |
|---|---|---|
| Beginner | 1–4 | Add `Vec2::rotated(angle)`, a `Mat3` inverse for camera transforms, `Grid<T>::fill` |
| Intermediate | 5–9 | Add an `AudioHandle` RAII type, `vec_pop` for the pool, an `Interpolated<T>` helper for render states |
| Advanced | 10–13 | `std::variant`-based events, swept AABB collision, diagonal + weighted A*, hierarchical FSM |
| Expert | 14–15 | Work-stealing job queues, parallel movement system over the ECS, an SDL2 backend for `IRenderer`, real keyboard input for the mini game |
