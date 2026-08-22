# Algorithms & Their Use Cases

A **conceptual, language-agnostic course** on algorithms: the problem each
solves, the central insight, the complexity, the real-world systems that run
it, and when *not* to use it. The course text is
[DOCUMENTATION.md](DOCUMENTATION.md).

Twelve algorithms — **ten classics and two novel** streaming/probabilistic
ones — each implemented **four times**: in C, C++, Rust, and Python.

## The thesis, made executable

All four programs print **byte-identical output** after their first line:

```bash
make verify
# ...
# VERIFIED: outputs are byte-identical across C, C++, Rust and Python
```

Even the "random" reservoir sample and the probabilistic HyperLogLog
estimate agree to the byte, because the four implementations share the same
seeded generator, the same hash, and the same order of operations. An
algorithm is an idea, not code — languages only change the idioms, and
reading the four files side by side is half the course.

## The algorithms

| # | Algorithm | Family | Flagship use case |
|---|---|---|---|
| 1 | Binary search | divide & conquer | database indexes, `git bisect` |
| 2 | Quicksort | divide & conquer | `qsort`, `std::sort`, `sort_unstable` |
| 3 | Breadth-first search | graph | social distance, crawlers, GC marking |
| 4 | Dijkstra | graph / greedy | GPS routing, OSPF/IS-IS |
| 5 | Edit distance | dynamic programming | spell check, `diff`, DNA alignment |
| 6 | Interval scheduling | greedy | meeting rooms, resource booking |
| 7 | Union-find | amortized structure | Kruskal's MST, image segmentation |
| 8 | Topological sort (Kahn) | graph | build systems, package managers |
| 9 | Kadane | dynamic programming | best trading window, signal analysis |
| 10 | Rabin-Karp | hashing | plagiarism detection, `rsync` |
| 11 | **Reservoir sampling** | streaming *(novel)* | log sampling at scale |
| 12 | **HyperLogLog** | probabilistic *(novel)* | Redis `PFCOUNT`, unique visitors |

## Building and running

Requires `gcc`, `g++`, `rustc`, `python3`, and `make`.

```bash
make          # build the C, C++ and Rust binaries
make run      # run all four implementations in sequence
make verify   # diff the four outputs — must be byte-identical
make clean    # remove binaries and captured outputs
```

All compiled builds are warning-free (`-Wall -Wextra -pedantic` for C/C++).

## Layout

| File | Purpose |
|---|---|
| `DOCUMENTATION.md` | **The course text** — concepts, use cases, trade-offs |
| `main.c` | C implementation (fixed arrays, explicit memory) |
| `main.cpp` | C++ implementation (STL containers and algorithms) |
| `main.rs` | Rust implementation (slices, explicit wrapping arithmetic) |
| `main.py` | Python implementation (executable pseudocode) |
| `Makefile` | Builds everything; `verify` proves cross-language identity |
