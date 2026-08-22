#!/usr/bin/env python3
# ============================================================================
#  main.py — Algorithms & Their Use Cases (Python implementation)
# ============================================================================
#
#  One of FOUR implementations of the same twelve algorithms (C, C++, Rust,
#  Python), engineered to print BYTE-IDENTICAL output after the first line —
#  `make verify` diffs them. Same algorithms as main.c; the idiomatic
#  differences are Python's: lists and slices everywhere, tuples for pairs,
#  and 64-bit modular arithmetic done explicitly with a mask (Python ints
#  are unbounded — the other three languages get wrapping for free).
#  The ALGORITHMS do not change.
# ============================================================================

MASK64 = (1 << 64) - 1


def fmt_array(a):
    """Formats "[a, b, c]" exactly like the other three languages."""
    return "[" + ", ".join(str(v) for v in a) + "]"


# =============================== 1. binary search ===========================

def binary_search_idx(a, target):
    lo, hi = 0, len(a)
    while lo < hi:
        mid = lo + (hi - lo) // 2
        if a[mid] == target:
            return mid
        if a[mid] < target:
            lo = mid + 1
        else:
            hi = mid
    return -1


# ================================= 2. quicksort =============================

def quicksort(a, lo, hi):
    if lo >= hi:
        return
    pivot = a[hi]
    i = lo
    for j in range(lo, hi):
        if a[j] < pivot:
            a[i], a[j] = a[j], a[i]
            i += 1
    a[i], a[hi] = a[hi], a[i]
    quicksort(a, lo, i - 1)
    quicksort(a, i + 1, hi)


# =========================== 3. breadth-first search ========================

def bfs_hops(adj, start):
    dist = [-1] * len(adj)
    dist[start] = 0
    queue = [start]
    head = 0
    while head < len(queue):
        node = queue[head]
        head += 1
        for nxt in adj[node]:
            if dist[nxt] == -1:
                dist[nxt] = dist[node] + 1
                queue.append(nxt)
    return dist


# ================================= 4. dijkstra ==============================

def dijkstra(w, start):
    inf = 1 << 29
    n = len(w)
    dist = [inf] * n
    visited = [False] * n
    dist[start] = 0

    for _ in range(n):
        u = -1
        for i in range(n):
            if not visited[i] and (u == -1 or dist[i] < dist[u]):
                u = i
        if u == -1 or dist[u] == inf:
            break
        visited[u] = True
        for v in range(n):
            if w[u][v] > 0 and dist[u] + w[u][v] < dist[v]:
                dist[v] = dist[u] + w[u][v]
    return dist


# =============================== 5. edit distance ===========================

def edit_distance(a, b):
    m, n = len(a), len(b)
    dp = [[0] * (n + 1) for _ in range(m + 1)]
    for i in range(m + 1):
        dp[i][0] = i
    for j in range(n + 1):
        dp[0][j] = j
    for i in range(1, m + 1):
        for j in range(1, n + 1):
            cost = 0 if a[i - 1] == b[j - 1] else 1
            dp[i][j] = min(dp[i - 1][j] + 1,
                           dp[i][j - 1] + 1,
                           dp[i - 1][j - 1] + cost)
    return dp[m][n]


# ================================ 7. union-find =============================

class UnionFind:
    def __init__(self, n):
        self.parent = list(range(n))

    def find(self, x):
        while self.parent[x] != x:
            self.parent[x] = self.parent[self.parent[x]]  # path halving
            x = self.parent[x]
        return x

    def unite(self, a, b):
        self.parent[self.find(a)] = self.find(b)

    def connected(self, a, b):
        return self.find(a) == self.find(b)


# ============================= 8. topological sort ==========================

def topo_sort(adj):
    n = len(adj)
    indeg = [0] * n
    for neighbors in adj:
        for v in neighbors:
            indeg[v] += 1
    queue = [u for u in range(n) if indeg[u] == 0]
    order = []
    head = 0
    while head < len(queue):
        u = queue[head]
        head += 1
        order.append(u)
        for v in adj[u]:
            indeg[v] -= 1
            if indeg[v] == 0:
                queue.append(v)
    return order


# ================================== 9. kadane ===============================

def kadane(a):
    best_here = a[0]
    best = a[0]
    for x in a[1:]:
        best_here = max(x, best_here + x)
        best = max(best, best_here)
    return best


# ================================ 10. rabin-karp ============================

def rabin_karp(text, pat):
    d, q = 256, 101
    n, m = len(text), len(pat)
    matches = []

    high = 1
    for _ in range(m - 1):
        high = (high * d) % q

    ph = th = 0
    for i in range(m):
        ph = (ph * d + ord(pat[i])) % q
        th = (th * d + ord(text[i])) % q

    for i in range(n - m + 1):
        if ph == th and text[i:i + m] == pat:
            matches.append(i)
        if i + m < n:
            th = (th + q * d - ord(text[i]) * high % q) % q
            th = (th * d + ord(text[i + m])) % q
    return matches


# ============================ 11. reservoir sampling ========================

class Lcg:
    def __init__(self, seed):
        self.state = seed

    def next(self):
        self.state = (self.state * 6364136223846793005 + 1442695040888963407) & MASK64
        return self.state


def reservoir_sample(stream_len, k, rng):
    out = list(range(k))
    for i in range(k, stream_len):
        j = rng.next() % (i + 1)
        if j < k:
            out[j] = i
    return out


# =============================== 12. hyperloglog ============================

def fmix64(h):
    # FNV-1a's high bits avalanche poorly on similar keys; the murmur3
    # finalizer fixes the bit distribution HLL depends on.
    h ^= h >> 33
    h = (h * 0xff51afd7ed558ccd) & MASK64
    h ^= h >> 33
    h = (h * 0xc4ceb9fe1a85ec53) & MASK64
    h ^= h >> 33
    return h


def fnv1a64(s):
    h = 14695981039346656037
    for c in s.encode():
        h ^= c
        h = (h * 1099511628211) & MASK64
    return fmix64(h)


class HyperLogLog:
    M = 256

    def __init__(self):
        self.reg = [0] * self.M

    def add(self, item):
        h = fnv1a64(item)
        idx = h & (self.M - 1)
        w = h >> 8

        rho = 1
        mask = 1 << 55
        while mask != 0 and (w & mask) == 0:
            rho += 1
            mask >>= 1
        if rho > self.reg[idx]:
            self.reg[idx] = rho

    def estimate(self):
        total = 0.0
        for r in self.reg:
            total += 1.0 / (1 << r)
        alpha = 0.7213 / (1.0 + 1.079 / 256.0)
        e = alpha * 256.0 * 256.0 / total
        return int(e + 0.5)


# ===================================== main =================================

def main():
    print("language: Python")

    # 1
    print("\n== 1. binary search ==")
    sorted_data = [2, 5, 8, 12, 16, 23, 38, 56, 72, 91]
    print("sorted: " + fmt_array(sorted_data))
    print("index of 23 -> {}".format(binary_search_idx(sorted_data, 23)))
    print("index of 4 -> {}".format(binary_search_idx(sorted_data, 4)))

    # 2
    print("\n== 2. quicksort ==")
    unsorted = [42, 7, 19, 3, 88, 27, 1, 65]
    print("before: " + fmt_array(unsorted))
    quicksort(unsorted, 0, len(unsorted) - 1)
    print("after:  " + fmt_array(unsorted))

    # 3
    print("\n== 3. breadth-first search ==")
    bfs_adj = [[1, 2], [0, 3], [0, 3, 6], [1, 2, 4], [3, 5], [4], [2]]
    print("hops from node 0: " + fmt_array(bfs_hops(bfs_adj, 0)))

    # 4
    print("\n== 4. dijkstra shortest paths ==")
    weights = [
        [0, 7, 9, 0, 0, 14],
        [7, 0, 10, 15, 0, 0],
        [9, 10, 0, 11, 0, 2],
        [0, 15, 11, 0, 6, 0],
        [0, 0, 0, 6, 0, 9],
        [14, 0, 2, 0, 9, 0],
    ]
    print("cheapest cost from node 0: " + fmt_array(dijkstra(weights, 0)))

    # 5
    print("\n== 5. edit distance (dynamic programming) ==")
    print("kitten -> sitting: {}".format(edit_distance("kitten", "sitting")))
    print("sunday -> saturday: {}".format(edit_distance("sunday", "saturday")))

    # 6
    print("\n== 6. interval scheduling (greedy) ==")
    meetings = [(0, 6), (1, 4), (3, 5), (3, 8), (4, 7), (5, 9), (6, 10), (8, 11)]
    meetings.sort(key=lambda m: m[1])
    picked = []
    busy_until = -1
    for start, end in meetings:
        if start >= busy_until:
            picked.append((start, end))
            busy_until = end
    picked_str = ", ".join("({},{})".format(s, e) for s, e in picked)
    print("picked {} meetings: [{}]".format(len(picked), picked_str))

    # 7
    print("\n== 7. union-find (disjoint sets) ==")
    uf = UnionFind(8)
    uf.unite(0, 1)
    uf.unite(1, 2)
    uf.unite(3, 4)
    print("connected(0,2) -> " + ("true" if uf.connected(0, 2) else "false"))
    print("connected(0,3) -> " + ("true" if uf.connected(0, 3) else "false"))
    uf.unite(2, 3)
    print("after union(2,3): connected(0,4) -> " +
          ("true" if uf.connected(0, 4) else "false"))

    # 8
    print("\n== 8. topological sort (kahn) ==")
    topo_adj = [[], [], [3], [1], [0, 1], [2, 0]]
    print("build order: " + fmt_array(topo_sort(topo_adj)))

    # 9
    print("\n== 9. kadane max subarray ==")
    gains = [-2, 1, -3, 4, -1, 2, 1, -5, 4]
    print("daily changes: " + fmt_array(gains))
    print("best run sum: {}".format(kadane(gains)))

    # 10
    print("\n== 10. rabin-karp substring search ==")
    print("\"abra\" in \"abracadabra\" at: " +
          fmt_array(rabin_karp("abracadabra", "abra")))

    # 11
    print("\n== 11. reservoir sampling (streaming) ==")
    rng = Lcg(42)
    print("uniform sample of 5 from a 1000-item stream: " +
          fmt_array(reservoir_sample(1000, 5, rng)))

    # 12
    print("\n== 12. hyperloglog (probabilistic counting) ==")
    hll = HyperLogLog()
    for i in range(10000):
        hll.add("user-{}".format(i % 1000))
    print("stream: 10000 items, 1000 distinct")
    print("estimated distinct (256 bytes of state): {}".format(hll.estimate()))

    print("\nall 12 algorithms demonstrated.")


if __name__ == "__main__":
    main()
