# Algorithms & Their Use Cases — Course Text

This is a **conceptual, language-agnostic course** about algorithms: what
problem each one solves, the intuition behind it, its complexity, where it
runs in real systems, and — just as important — when *not* to use it.

Every algorithm is implemented **four times** — in C (`main.c`), C++
(`main.cpp`), Rust (`main.rs`), and Python (`main.py`) — and all four
programs print **byte-identical output** after their first line.
`make verify` diffs them. That is the course's thesis made executable:

> An algorithm is an idea, not code. Languages change the *idioms* —
> pointer arithmetic vs iterators vs slices vs list comprehensions — but
> binary search halves the same interval in all of them.

Twelve algorithms are covered: **ten classics** (1–10) that every working
programmer meets, and **two novel ones** (11–12) from the modern
streaming/probabilistic family — algorithms that trade exactness for the
ability to process data too large to hold.

```bash
make          # build the C, C++ and Rust binaries
make run      # run all four implementations
make verify   # prove the outputs are byte-identical
```

---

## Table of Contents

- [How to read an algorithm](#how-to-read-an-algorithm)
- [1. Binary Search](#1-binary-search)
- [2. Quicksort](#2-quicksort)
- [3. Breadth-First Search](#3-breadth-first-search)
- [4. Dijkstra's Algorithm](#4-dijkstras-algorithm)
- [5. Edit Distance](#5-edit-distance-dynamic-programming)
- [6. Interval Scheduling](#6-interval-scheduling-greedy)
- [7. Union-Find](#7-union-find-disjoint-sets)
- [8. Topological Sort](#8-topological-sort-kahns-algorithm)
- [9. Kadane's Algorithm](#9-kadanes-algorithm-maximum-subarray)
- [10. Rabin-Karp](#10-rabin-karp-substring-search)
- [11. Reservoir Sampling](#11-reservoir-sampling-novel-1)
- [12. HyperLogLog](#12-hyperloglog-novel-2)
- [The same algorithm in four languages](#the-same-algorithm-in-four-languages)

---

## How to read an algorithm

Four questions organize every chapter, and they are the four questions to
ask about any algorithm you meet:

1. **What problem does it solve?** Stated without code — usually a sentence.
2. **What is the insight?** Every named algorithm has one central idea; the
   rest is bookkeeping.
3. **What does it cost?** Time and space in big-O — but also the constants,
   preconditions, and failure modes big-O hides.
4. **Where does it run in the real world?** Algorithms earn names by being
   useful; each has systems that depend on it daily — and situations where
   it's the wrong choice.

Complexity notation in one paragraph: **O(f(n))** bounds how the cost grows
with input size n. O(1) constant, O(log n) halving, O(n) one pass,
O(n log n) sort-like, O(n²) all-pairs, O(2ⁿ) hopeless. Two hidden caveats
matter in practice: constants (an O(n) pass that misses cache can lose to
an O(n log n) that doesn't — see the C++ course's AoS/SoA chapter), and
preconditions (binary search's O(log n) is purchased with the cost of
keeping data sorted).

---

## 1. Binary Search

**Problem.** Find an item in *sorted* data.

**Insight.** One comparison against the middle discards half the
candidates. Repeat: 1,000,000 items need at most 20 comparisons.

**Cost.** O(log n) time, O(1) space. Precondition: the data is sorted —
keeping it sorted is where the real price lives.

**Use cases.** Database index lookups (B-trees are "binary search made
cache-friendly"); `git bisect` finding the commit that broke the build in
log₂(history) steps; numeric root-finding (bisection); "binary search the
answer" — finding the smallest capacity/speed/size that satisfies a
constraint, used constantly in capacity planning.

**When not.** Unsorted or frequently-mutated data (every insert pays
O(n) to stay sorted — use a hash table or tree); tiny arrays where a linear
scan is simpler and just as fast.

**Implementation notes.** The half-open interval `[lo, hi)` and the
overflow-safe midpoint `lo + (hi - lo) / 2` — the naive `(lo + hi) / 2`
overflow was a real bug in production libraries for decades (covered in
the C course too). All four implementations use the identical loop.

---

## 2. Quicksort

**Problem.** Order n items.

**Insight.** Pick a pivot; partition everything smaller to its left,
larger to its right — the pivot is now in its FINAL position. Recurse on
the two sides. The work is one O(n) pass per level, O(log n) levels on
average.

**Cost.** O(n log n) average, O(n²) worst case (sorted input with a bad
pivot rule), O(log n) stack. In-place, cache-friendly, small constants —
which is why it wins in practice despite the worst case.

**Use cases.** The backbone of `qsort`, and (as introsort: quicksort +
heapsort fallback + insertion sort for small runs) of C++'s `std::sort`
and Rust's `sort_unstable`. Its partition step alone is **quickselect** —
finding the k-th smallest element (medians, percentiles) in O(n) average.

**When not.** When stability matters (equal keys must keep their order —
use merge sort/timsort, which is Python's `sorted`); when adversarial
input is possible and worst-case guarantees are required (heapsort or
merge sort); when data doesn't fit in memory (external merge sort).

**Implementation notes.** Lomuto partition with last-element pivot —
chosen for clarity and cross-language determinism, not production use
(real implementations use median-of-three or randomized pivots exactly to
dodge the O(n²) input).

---

## 3. Breadth-First Search

**Problem.** Explore a graph from a start node; find the minimum number of
*hops* to every reachable node.

**Insight.** A FIFO queue processes all nodes at distance d before any
node at distance d+1 — so the first time a node is reached IS its
shortest hop count. Swap the queue for a stack and you get DFS: the data
structure is the algorithm.

**Cost.** O(V + E) time, O(V) space.

**Use cases.** Shortest routes when every edge costs the same: degrees of
separation in social graphs, web crawlers (fetch by depth), chip/PCB
routing, garbage collector reachability (mark phase), puzzle solvers
(fewest moves), broadcast in networks.

**When not.** Weighted edges (that's Dijkstra, next chapter); very deep
searches where the frontier explodes (memory is O(width) — iterative
deepening or bidirectional search help).

**Implementation notes.** The demo graph's hop counts `[0, 1, 1, 2, 3, 4, 2]`
are identical in all four languages because adjacency lists are iterated
in a fixed order — determinism is a *choice* in graph code.

---

## 4. Dijkstra's Algorithm

**Problem.** Cheapest paths from a source in a graph with non-negative
edge weights.

**Insight.** Greedy that works: always settle the unsettled node with the
smallest known cost. Because weights can't be negative, no later route can
undercut it — its cost is final. (This is BFS generalized: BFS is
Dijkstra where every edge costs 1.)

**Cost.** O(V²) with the array scan used here (fine for dense or small
graphs); O((V + E) log V) with a binary heap — the version real systems
run.

**Use cases.** GPS navigation (with A*, which is Dijkstra plus a heuristic
that says "prefer nodes toward the goal"); network routing protocols —
OSPF and IS-IS, which route much of the internet, are Dijkstra over link
costs; game pathfinding; dependency resolvers minimizing download cost.

**When not.** Negative edge weights (Bellman-Ford handles them and detects
negative cycles); huge graphs where a heuristic is available (A* explores
far less); all-pairs needs (Floyd-Warshall or repeated Dijkstra).

**Implementation notes.** The classic 6-node textbook graph; expected
output `[0, 7, 9, 20, 20, 11]`. Note the cost of reaching node 5: 11 via
node 2, beating the direct 14 edge — shortest paths are rarely the obvious
ones.

---

## 5. Edit Distance (dynamic programming)

**Problem.** The minimum number of single-character edits (insert, delete,
substitute) turning one string into another. `kitten → sitting` is 3.

**Insight.** Dynamic programming: the answer for prefixes `(i, j)` depends
only on three smaller prefix answers. Fill a table so every subproblem is
solved exactly once instead of exponentially many times — DP is
"recursion plus memory".

**Cost.** O(m·n) time and space (O(min(m,n)) space with the two-row
optimization).

**Use cases.** Spell checkers ("did you mean...?" = words within distance
1–2); `diff`, and version control's merge machinery (longest common
subsequence is the same DP shape); DNA/protein sequence alignment
(Needleman-Wunsch is weighted edit distance) — bioinformatics runs on
this table; fuzzy search in editors and search engines.

**When not.** Long documents at scale — O(m·n) explodes; real systems
filter candidates first (n-gram indexes, BK-trees) and run the DP on the
survivors only.

**Implementation notes.** The DP table's first row and column are the
base cases (distance from the empty string). All four languages fill the
table in the same order; `sunday → saturday: 3` is the classic second
test vector.

---

## 6. Interval Scheduling (greedy)

**Problem.** From overlapping meetings, attend the maximum number.

**Insight.** Sort by END time and always take the meeting that frees you
soonest — it leaves the most room for the rest. This greedy choice is
*provably* optimal (an exchange argument: any optimal solution can be
rewritten to start with the earliest-ending meeting).

**Cost.** O(n log n) for the sort; the selection pass is O(n).

**Use cases.** Meeting-room and resource booking; CPU/task scheduling
variants; bandwidth reservation; transmit-slot allocation in radio
protocols.

**When not — the bigger lesson.** Greedy is a *strategy*, not a guarantee.
Sort by start time or by duration and the same code produces suboptimal
schedules; add weights (maximize total VALUE, not count) and no sort order
works — weighted interval scheduling needs DP (Chapter 5's tool). Every
greedy algorithm needs a proof, not a vibe.

**Implementation notes.** The demo picks `[(1,4), (4,7), (8,11)]` — 3 of
8 meetings. Note `start >= busy_until`: touching intervals don't conflict.
The four languages sort with different tools (qsort / std::sort /
sort_by_key / list.sort) — same comparator, same result.

---

## 7. Union-Find (disjoint sets)

**Problem.** Dynamic connectivity: merge groups and answer "are these two
in the same group?" — many times, fast.

**Insight.** Each group is a tree; a member's *root* names the group.
Union = point one root at the other. Two tricks flatten the trees —
path compression (nodes re-point toward the root as find walks up) and
union by rank — making operations effectively O(1) (inverse Ackermann,
< 5 for any input that fits in the universe).

**Cost.** Near-O(1) amortized per operation, O(n) space.

**Use cases.** Kruskal's minimum-spanning-tree algorithm ("does this edge
connect two different components?"); network/percolation connectivity;
image segmentation (merge similar neighboring regions); compilers' type
unification; detecting cycles in an undirected graph as edges stream in.

**When not.** It only answers connectivity — not paths (BFS) or distances
(Dijkstra). And it merges; it cannot efficiently *split* (that needs much
heavier machinery).

**Implementation notes.** Path *halving* (each node re-points to its
grandparent) — one line, same asymptotics as full compression, identical
in all four languages. The demo's output is structure-independent (only
booleans), a deliberate choice: internal tree shapes may differ, answers
may not.

---

## 8. Topological Sort (Kahn's algorithm)

**Problem.** Order the nodes of a directed acyclic graph so every edge
points forward: dependencies before dependents.

**Insight.** Anything with no incoming edges can safely go first. Emit
it, delete its outgoing edges, repeat. If everything gets emitted, that's
a valid order; if not, the leftovers contain a cycle — the algorithm is
also the cycle *detector*.

**Cost.** O(V + E) time and space.

**Use cases.** Build systems (make, ninja, cargo, bazel: compile in
dependency order — and the independent nodes of each "level" are exactly
what can build in parallel); package managers resolving install order;
spreadsheet cell recalculation; task orchestration DAGs (Airflow);
course-prerequisite planning.

**When not.** Cyclic graphs have no topological order by definition — the
right output there is the cycle itself, as an error message (`error:
package A depends on B depends on A`).

**Implementation notes.** FIFO processing with fixed adjacency order
makes the output deterministic (`[4, 5, 2, 0, 3, 1]`) across all four
languages; in general many valid orders exist, which is worth teaching in
itself — "a" topological order, never "the".

---

## 9. Kadane's Algorithm (maximum subarray)

**Problem.** In a sequence of gains and losses, find the contiguous run
with the largest total.

**Insight.** Scan once, tracking the best run *ending here*: it's either
the previous best run extended, or a fresh start at the current element —
whichever is larger. DP distilled to two variables; no table needed.

**Cost.** O(n) time, O(1) space.

**Use cases.** The best buy/sell window in a price series (on the array of
daily *changes*); the loudest segment in an audio signal; hot-region
detection in profiling data; maximum-sum submatrix (2D version, built on
the 1D one); genomics' maximal-scoring subsequences.

**When not.** Non-contiguous selection (that's a different problem —
just take every positive element); constrained window sizes (sliding
window techniques); all-negative inputs need a defined convention (this
implementation returns the largest single element, a decision worth
making explicitly).

**Implementation notes.** `best_here = max(x, best_here + x)` — the whole
algorithm is that line. The demo's answer for
`[-2, 1, -3, 4, -1, 2, 1, -5, 4]` is 6 (the run `4, -1, 2, 1`).

---

## 10. Rabin-Karp (substring search)

**Problem.** Find every occurrence of a pattern inside a text.

**Insight.** Compare *hashes*, not strings. A **rolling hash** updates the
window's hash in O(1) as it slides one character — subtract the leaving
character's contribution, add the entering one's. Hash mismatch: definitely
not a match, skip. Hash match: verify char-by-char (collisions exist).

**Cost.** O(n + m) expected; O(n·m) worst case if collisions pile up
(mitigated by a decent modulus or double hashing).

**Use cases.** Plagiarism and duplicate detection (hash every window of
every document — this scales where naive comparison cannot); `rsync`'s
rolling checksum finding unchanged blocks at any alignment; content-
defined chunking in dedup/backup systems; multi-pattern search (hash all
patterns into a set, one scan of the text).

**When not.** Single-pattern search where guaranteed linear time matters —
KMP and Boyer-Moore have no bad case; tiny texts where `strstr` wins on
constants. Rabin-Karp's edge is *many* patterns or *many* windows.

**Implementation notes.** Base 256, modulus 101 (teaching-sized), the
`+ Q*D - ...` dance keeping modular arithmetic non-negative — identical
in all four languages because they all reduce mod Q the same way. The
verification step on hash match is not optional; skipping it is a
correctness bug, not an optimization.

---

## 11. Reservoir Sampling (novel #1)

**Problem.** Keep a uniform random sample of k items from a stream whose
length is unknown in advance — logs, clicks, sensor readings — in one
pass and O(k) memory.

**Insight.** Keep the first k items. When item i (0-indexed, i ≥ k)
arrives, accept it with probability k/(i+1); if accepted, it evicts a
random resident. Induction shows every item seen so far ends up in the
sample with probability exactly k/(i+1) — perfectly uniform at every
moment, no matter when the stream stops.

**Cost.** O(n) time, O(k) memory, one pass. No second look at the data —
that's the entire point.

**Use cases.** Sampling telemetry/logs at scale (keep 10k representative
events out of billions); A/B-test subject selection from a live stream;
database query planners sampling tables to estimate statistics;
training-data subsampling in one pass over a dataset too big to shuffle.

**When not.** Weighted sampling needs the A-Res/A-ExpJ variants; if the
data fits in memory, just shuffle and take k; if you need *distinct*
counts rather than a sample, that's the next chapter.

**Implementation notes.** The "randomness" is a tiny LCG implemented
identically in all four languages (wrapping 64-bit multiply-add; Python
masks to 64 bits explicitly) — so all four print the *same* "random"
sample. Seeded, reproducible randomness is itself a pattern: it makes
probabilistic code testable (the Rust course used the same trick).

---

## 12. HyperLogLog (novel #2)

**Problem.** How many *distinct* items are in a massive stream — unique
visitors, unique IPs, unique queries — without storing the items? An
exact answer needs a set: gigabytes for billions of items. HyperLogLog
answers within a few percent in **256 bytes**.

**Insight.** Hash every item. Rare hash prefixes imply many distinct
items: seeing a hash that starts with k zero bits suggests you've seen
about 2ᵏ distinct values. One maximum is a terrible estimator (one lucky
hash ruins it), so: use the low bits to route each item to one of m=256
registers, each register tracks its own maximum, and the harmonic mean of
2^(-register) combines them — outliers averaged away. Standard error is
1.04/√m ≈ 6.5% for m=256; grow m for more precision.

**Cost.** O(1) per item, m bytes total, mergeable: the register-wise MAX
of two HLLs is the HLL of the union — so shards can sketch independently
and merge for free.

**Use cases.** Redis's `PFCOUNT` (this exact structure); unique-visitor
counts in analytics (Google BigQuery's `APPROX_COUNT_DISTINCT`,
Druid, ClickHouse); network monitoring (distinct flows/IPs per minute on
a router); join-size estimation inside query optimizers; deduplication
estimates in storage systems.

**When not.** When exactness is a requirement (billing!); tiny
cardinalities where a plain set is free; and note the demo's own lesson —
the estimate (974 for a true 1000) is *close but wrong*, and knowing the
error bar is part of using the tool.

**Implementation notes.** Two practical details carry the chapter. First,
the hash must have good bit dispersion: raw FNV-1a's high bits avalanche
poorly on similar keys (`user-1`, `user-2`, ...), which skewed the
estimate to 775 in development — a murmur3-style finalizer (`fmix64`)
fixed it to 974. *Probabilistic algorithms are only as good as their
hashes.* Second, all floating-point is done in the same order in all four
languages, so even the estimate is byte-identical — `1/2^r` sums are
exact in IEEE-754 doubles.

---

## The same algorithm in four languages

Reading the four files side by side is the second half of the course. The
algorithms are line-for-line the same; the *idioms* are not:

| Concern | C | C++ | Rust | Python |
|---|---|---|---|---|
| Collections | fixed arrays + explicit lengths | `std::vector` | `Vec` / slices | lists |
| Sorting | `qsort` + comparator fn | `std::sort` + lambda | `sort_by_key` | `sort(key=...)` |
| Graph adjacency | `-1`-padded 2D arrays | `vector<vector<int>>` | `Vec<Vec<usize>>` | list of lists |
| Pairs | struct | `std::pair` | tuple | tuple |
| 64-bit wraparound | native unsigned | native unsigned | `wrapping_mul/add` (explicit!) | `& MASK64` (explicit!) |
| Out-of-bounds | undefined behavior | undefined behavior | panic (checked) | exception (checked) |
| Genericity | `void*` | templates | generics + traits | duck typing |

Observations worth teaching:

- **C makes memory visible**: every buffer is declared with its size, and
  the BFS queue is just an array with two indices. Nothing is hidden —
  and nothing is checked.
- **C++ and Rust read almost identically here** — vectors, sort-with-key,
  range-for — but differ at the edges: Rust *forces* the wrapping
  arithmetic to be explicit (`wrapping_mul`), turning a silent C behavior
  into a visible decision, and bounds-checks every index.
- **Python is executable pseudocode** — the edit-distance table is four
  readable lines — at ~10–100× the runtime, which is why its heavy lifting
  (`sorted`, dict, numpy) is implemented in C underneath.
- **Determinism is portable**: with fixed iteration orders, a shared LCG,
  and same-order floating-point, even the "random" and probabilistic
  chapters agree to the byte. When outputs *would* legitimately differ
  (hash-map iteration order, unstable sorts of equal keys), that's a
  property of the *implementation*, not the algorithm — and knowing the
  difference is the mark of an advanced student.

## Suggested Course Progression

| Stage | Chapters | Exercises to assign |
|---|---|---|
| Foundations | 1, 2, 9 | Implement lower-bound binary search ("first index ≥ x"); make quicksort use median-of-three; extend Kadane to also report the run's indices |
| Graphs | 3, 4, 7, 8 | Add DFS by swapping BFS's queue for a stack; heap-based Dijkstra; detect a cycle with Kahn's leftover nodes; Kruskal's MST from union-find |
| Techniques | 5, 6, 10 | Two-row edit distance; weighted interval scheduling (greedy fails — use DP); double-hash Rabin-Karp |
| Streaming | 11, 12 | Weighted reservoir sampling; measure HLL error vs register count (m = 16 … 4096); merge two HLL sketches and verify against the true union |
