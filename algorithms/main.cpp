/*
 * ============================================================================
 *  main.cpp — Algorithms & Their Use Cases (C++ implementation)
 * ============================================================================
 *
 *  One of FOUR implementations of the same twelve algorithms (C, C++, Rust,
 *  Python), engineered to print BYTE-IDENTICAL output after the first line —
 *  `make verify` diffs them. Same algorithms as main.c; the interesting
 *  differences are idiomatic: std::vector instead of fixed arrays,
 *  std::sort with a lambda instead of qsort, range-for instead of index
 *  loops. The ALGORITHMS do not change.
 * ============================================================================
 */

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

/* Prints "[a, b, c]" exactly like the other three languages. */
static void print_array(const std::vector<int>& a)
{
    std::cout << "[";
    for (std::size_t i = 0; i < a.size(); i++) {
        std::cout << a[i] << (i + 1 < a.size() ? ", " : "");
    }
    std::cout << "]";
}

/* ============================ 1. binary search =========================== */

static int binary_search_idx(const std::vector<int>& a, int target)
{
    std::size_t lo = 0, hi = a.size();
    while (lo < hi) {
        std::size_t mid = lo + (hi - lo) / 2;
        if (a[mid] == target) return static_cast<int>(mid);
        if (a[mid] < target) lo = mid + 1;
        else hi = mid;
    }
    return -1;
}

/* ============================ 2. quicksort =============================== */

static void quicksort(std::vector<int>& a, int lo, int hi)
{
    if (lo >= hi) return;
    int pivot = a[hi];
    int i = lo;
    for (int j = lo; j < hi; j++) {
        if (a[j] < pivot) {
            std::swap(a[i], a[j]);
            i++;
        }
    }
    std::swap(a[i], a[hi]);
    quicksort(a, lo, i - 1);
    quicksort(a, i + 1, hi);
}

/* ======================= 3. breadth-first search ========================= */

static std::vector<int> bfs_hops(const std::vector<std::vector<int>>& adj, int start)
{
    std::vector<int> dist(adj.size(), -1);
    std::vector<int> queue;
    std::size_t head = 0;

    dist[start] = 0;
    queue.push_back(start);
    while (head < queue.size()) {
        int node = queue[head++];
        for (int next : adj[node]) {
            if (dist[next] == -1) {
                dist[next] = dist[node] + 1;
                queue.push_back(next);
            }
        }
    }
    return dist;
}

/* ============================= 4. dijkstra =============================== */

static std::vector<int> dijkstra(const std::vector<std::vector<int>>& w, int start)
{
    const int INF = 1 << 29;
    const int n = static_cast<int>(w.size());
    std::vector<int> dist(n, INF);
    std::vector<bool> visited(n, false);
    dist[start] = 0;

    for (int round = 0; round < n; round++) {
        int u = -1;
        for (int i = 0; i < n; i++) {
            if (!visited[i] && (u == -1 || dist[i] < dist[u])) u = i;
        }
        if (u == -1 || dist[u] == INF) break;
        visited[u] = true;
        for (int v = 0; v < n; v++) {
            if (w[u][v] > 0 && dist[u] + w[u][v] < dist[v]) {
                dist[v] = dist[u] + w[u][v];
            }
        }
    }
    return dist;
}

/* =========================== 5. edit distance ============================ */

static int edit_distance(const std::string& a, const std::string& b)
{
    std::size_t m = a.size(), n = b.size();
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));

    for (std::size_t i = 0; i <= m; i++) dp[i][0] = static_cast<int>(i);
    for (std::size_t j = 0; j <= n; j++) dp[0][j] = static_cast<int>(j);

    for (std::size_t i = 1; i <= m; i++) {
        for (std::size_t j = 1; j <= n; j++) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            dp[i][j] = std::min({ dp[i - 1][j] + 1,
                                  dp[i][j - 1] + 1,
                                  dp[i - 1][j - 1] + cost });
        }
    }
    return dp[m][n];
}

/* ============================ 7. union-find ============================== */

struct UnionFind {
    std::vector<int> parent;

    explicit UnionFind(int n) : parent(n)
    {
        for (int i = 0; i < n; i++) parent[i] = i;
    }

    int find(int x)
    {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]]; // path halving
            x = parent[x];
        }
        return x;
    }

    void unite(int a, int b) { parent[find(a)] = find(b); }
    bool connected(int a, int b) { return find(a) == find(b); }
};

/* ========================= 8. topological sort =========================== */

static std::vector<int> topo_sort(const std::vector<std::vector<int>>& adj)
{
    const int n = static_cast<int>(adj.size());
    std::vector<int> indeg(n, 0);
    std::vector<int> queue;
    std::vector<int> order;
    std::size_t head = 0;

    for (int u = 0; u < n; u++) {
        for (int v : adj[u]) indeg[v]++;
    }
    for (int u = 0; u < n; u++) {
        if (indeg[u] == 0) queue.push_back(u);
    }
    while (head < queue.size()) {
        int u = queue[head++];
        order.push_back(u);
        for (int v : adj[u]) {
            if (--indeg[v] == 0) queue.push_back(v);
        }
    }
    return order;
}

/* =============================== 9. kadane =============================== */

static int kadane(const std::vector<int>& a)
{
    int best_here = a[0];
    int best = a[0];
    for (std::size_t i = 1; i < a.size(); i++) {
        best_here = std::max(a[i], best_here + a[i]);
        best = std::max(best, best_here);
    }
    return best;
}

/* ============================ 10. rabin-karp ============================= */

static std::vector<int> rabin_karp(const std::string& text, const std::string& pat)
{
    const std::uint64_t D = 256, Q = 101;
    const std::size_t n = text.size(), m = pat.size();
    std::vector<int> matches;

    std::uint64_t high = 1;
    for (std::size_t i = 0; i + 1 < m; i++) high = (high * D) % Q;

    std::uint64_t ph = 0, th = 0;
    for (std::size_t i = 0; i < m; i++) {
        ph = (ph * D + static_cast<unsigned char>(pat[i])) % Q;
        th = (th * D + static_cast<unsigned char>(text[i])) % Q;
    }

    for (std::size_t i = 0; i + m <= n; i++) {
        if (ph == th && text.compare(i, m, pat) == 0) {
            matches.push_back(static_cast<int>(i));
        }
        if (i + m < n) {
            th = (th + Q * D - static_cast<unsigned char>(text[i]) * high % Q) % Q;
            th = (th * D + static_cast<unsigned char>(text[i + m])) % Q;
        }
    }
    return matches;
}

/* ====================== 11. reservoir sampling =========================== */

static std::uint64_t g_lcg_state = 0;

static std::uint64_t lcg_next()
{
    g_lcg_state = g_lcg_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return g_lcg_state;
}

static std::vector<int> reservoir_sample(int stream_len, int k)
{
    std::vector<int> out(k);
    for (int i = 0; i < k; i++) out[i] = i;
    for (int i = k; i < stream_len; i++) {
        std::uint64_t j = lcg_next() % static_cast<std::uint64_t>(i + 1);
        if (j < static_cast<std::uint64_t>(k)) {
            out[j] = i;
        }
    }
    return out;
}

/* =========================== 12. hyperloglog ============================= */

static std::uint64_t fmix64(std::uint64_t h)
{
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
}

static std::uint64_t fnv1a64(const std::string& s)
{
    std::uint64_t h = 14695981039346656037ULL;
    for (char c : s) {
        h ^= static_cast<unsigned char>(c);
        h *= 1099511628211ULL;
    }
    return fmix64(h);
}

struct HyperLogLog {
    static const int M = 256;
    std::uint8_t reg[M] = {};

    void add(const std::string& item)
    {
        std::uint64_t h = fnv1a64(item);
        int idx = static_cast<int>(h & (M - 1));
        std::uint64_t w = h >> 8;

        int rho = 1;
        std::uint64_t mask = 1ULL << 55;
        while (mask != 0 && (w & mask) == 0) {
            rho++;
            mask >>= 1;
        }
        if (rho > reg[idx]) reg[idx] = static_cast<std::uint8_t>(rho);
    }

    long long estimate() const
    {
        double sum = 0.0;
        for (int j = 0; j < M; j++) {
            sum += 1.0 / static_cast<double>(1ULL << reg[j]);
        }
        double alpha = 0.7213 / (1.0 + 1.079 / 256.0);
        double e = alpha * 256.0 * 256.0 / sum;
        return static_cast<long long>(e + 0.5);
    }
};

/* ================================= main ================================== */

int main()
{
    std::cout << "language: C++\n";

    /* 1 */
    std::cout << "\n== 1. binary search ==\n";
    std::vector<int> sorted = { 2, 5, 8, 12, 16, 23, 38, 56, 72, 91 };
    std::cout << "sorted: ";
    print_array(sorted);
    std::cout << "\n";
    std::cout << "index of 23 -> " << binary_search_idx(sorted, 23) << "\n";
    std::cout << "index of 4 -> " << binary_search_idx(sorted, 4) << "\n";

    /* 2 */
    std::cout << "\n== 2. quicksort ==\n";
    std::vector<int> unsorted = { 42, 7, 19, 3, 88, 27, 1, 65 };
    std::cout << "before: ";
    print_array(unsorted);
    std::cout << "\n";
    quicksort(unsorted, 0, static_cast<int>(unsorted.size()) - 1);
    std::cout << "after:  ";
    print_array(unsorted);
    std::cout << "\n";

    /* 3 */
    std::cout << "\n== 3. breadth-first search ==\n";
    std::vector<std::vector<int>> bfs_adj = {
        { 1, 2 }, { 0, 3 }, { 0, 3, 6 }, { 1, 2, 4 }, { 3, 5 }, { 4 }, { 2 },
    };
    std::cout << "hops from node 0: ";
    print_array(bfs_hops(bfs_adj, 0));
    std::cout << "\n";

    /* 4 */
    std::cout << "\n== 4. dijkstra shortest paths ==\n";
    std::vector<std::vector<int>> weights = {
        { 0, 7, 9, 0, 0, 14 },
        { 7, 0, 10, 15, 0, 0 },
        { 9, 10, 0, 11, 0, 2 },
        { 0, 15, 11, 0, 6, 0 },
        { 0, 0, 0, 6, 0, 9 },
        { 14, 0, 2, 0, 9, 0 },
    };
    std::cout << "cheapest cost from node 0: ";
    print_array(dijkstra(weights, 0));
    std::cout << "\n";

    /* 5 */
    std::cout << "\n== 5. edit distance (dynamic programming) ==\n";
    std::cout << "kitten -> sitting: " << edit_distance("kitten", "sitting") << "\n";
    std::cout << "sunday -> saturday: " << edit_distance("sunday", "saturday") << "\n";

    /* 6 */
    std::cout << "\n== 6. interval scheduling (greedy) ==\n";
    std::vector<std::pair<int, int>> meetings = {
        { 0, 6 }, { 1, 4 }, { 3, 5 }, { 3, 8 },
        { 4, 7 }, { 5, 9 }, { 6, 10 }, { 8, 11 },
    };
    std::sort(meetings.begin(), meetings.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });
    std::vector<std::pair<int, int>> picked;
    int busy_until = -1;
    for (const auto& m : meetings) {
        if (m.first >= busy_until) {
            picked.push_back(m);
            busy_until = m.second;
        }
    }
    std::cout << "picked " << picked.size() << " meetings: [";
    for (std::size_t i = 0; i < picked.size(); i++) {
        std::cout << "(" << picked[i].first << "," << picked[i].second << ")"
                  << (i + 1 < picked.size() ? ", " : "");
    }
    std::cout << "]\n";

    /* 7 */
    std::cout << "\n== 7. union-find (disjoint sets) ==\n";
    UnionFind uf(8);
    uf.unite(0, 1);
    uf.unite(1, 2);
    uf.unite(3, 4);
    std::cout << "connected(0,2) -> " << (uf.connected(0, 2) ? "true" : "false") << "\n";
    std::cout << "connected(0,3) -> " << (uf.connected(0, 3) ? "true" : "false") << "\n";
    uf.unite(2, 3);
    std::cout << "after union(2,3): connected(0,4) -> "
              << (uf.connected(0, 4) ? "true" : "false") << "\n";

    /* 8 */
    std::cout << "\n== 8. topological sort (kahn) ==\n";
    std::vector<std::vector<int>> topo_adj = {
        {}, {}, { 3 }, { 1 }, { 0, 1 }, { 2, 0 },
    };
    std::cout << "build order: ";
    print_array(topo_sort(topo_adj));
    std::cout << "\n";

    /* 9 */
    std::cout << "\n== 9. kadane max subarray ==\n";
    std::vector<int> gains = { -2, 1, -3, 4, -1, 2, 1, -5, 4 };
    std::cout << "daily changes: ";
    print_array(gains);
    std::cout << "\n";
    std::cout << "best run sum: " << kadane(gains) << "\n";

    /* 10 */
    std::cout << "\n== 10. rabin-karp substring search ==\n";
    std::cout << "\"abra\" in \"abracadabra\" at: ";
    print_array(rabin_karp("abracadabra", "abra"));
    std::cout << "\n";

    /* 11 */
    std::cout << "\n== 11. reservoir sampling (streaming) ==\n";
    g_lcg_state = 42;
    std::cout << "uniform sample of 5 from a 1000-item stream: ";
    print_array(reservoir_sample(1000, 5));
    std::cout << "\n";

    /* 12 */
    std::cout << "\n== 12. hyperloglog (probabilistic counting) ==\n";
    HyperLogLog hll;
    for (int i = 0; i < 10000; i++) {
        hll.add("user-" + std::to_string(i % 1000));
    }
    std::cout << "stream: 10000 items, 1000 distinct\n";
    std::cout << "estimated distinct (256 bytes of state): " << hll.estimate() << "\n";

    std::cout << "\nall 12 algorithms demonstrated.\n";
    return 0;
}
