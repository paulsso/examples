/*
 * ============================================================================
 *  main.c — Algorithms & Their Use Cases (C implementation)
 * ============================================================================
 *
 *  One of FOUR implementations of the same twelve algorithms (C, C++, Rust,
 *  Python). All four are engineered to print BYTE-IDENTICAL output after
 *  their first line — `make verify` diffs them to prove the course's thesis:
 *  algorithms are language-agnostic; only the idioms change.
 *
 *  The course text (concepts, intuition, complexity, real-world use cases)
 *  lives in DOCUMENTATION.md. Comments here focus on the C idioms.
 *
 *  Algorithms 1-10 are the classics; 11-12 are the "novel" pair: streaming /
 *  probabilistic algorithms for data too large to hold in memory.
 * ============================================================================
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * Shared output helper: prints "[a, b, c]" exactly like the other languages.
 * ------------------------------------------------------------------------ */
static void print_array(const int *a, int n)
{
    printf("[");
    for (int i = 0; i < n; i++) {
        printf("%d%s", a[i], (i + 1 < n) ? ", " : "");
    }
    printf("]");
}

/* ===========================================================================
 * 1. BINARY SEARCH — O(log n) lookup in sorted data
 * ===========================================================================
 * Half-open interval [lo, hi); mid computed overflow-safely.
 */
static int binary_search(const int *a, int n, int target)
{
    int lo = 0, hi = n;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (a[mid] == target) return mid;
        if (a[mid] < target) lo = mid + 1;
        else hi = mid;
    }
    return -1;
}

/* ===========================================================================
 * 2. QUICKSORT — O(n log n) average, in-place
 * ===========================================================================
 * Lomuto partition with the last element as pivot: everything < pivot is
 * swapped to the front, the pivot drops into its final slot, recurse on
 * both sides. (Deterministic pivot keeps all four languages identical.)
 */
static void quicksort(int *a, int lo, int hi)
{
    if (lo >= hi) return;
    int pivot = a[hi];
    int i = lo;
    for (int j = lo; j < hi; j++) {
        if (a[j] < pivot) {
            int t = a[i]; a[i] = a[j]; a[j] = t;
            i++;
        }
    }
    int t = a[i]; a[i] = a[hi]; a[hi] = t;
    quicksort(a, lo, i - 1);
    quicksort(a, i + 1, hi);
}

/* ===========================================================================
 * 3. BREADTH-FIRST SEARCH — layer-by-layer exploration, O(V + E)
 * ===========================================================================
 * The FIFO queue is what makes BFS find MINIMUM-hop paths: all nodes at
 * distance d are processed before any node at distance d+1.
 * Graph: adjacency lists in fixed rows, -1 padded (plain C, no containers).
 */
#define BFS_N 7

static const int BFS_ADJ[BFS_N][4] = {
    { 1, 2, -1, -1 },   /* 0 */
    { 0, 3, -1, -1 },   /* 1 */
    { 0, 3, 6, -1 },    /* 2 */
    { 1, 2, 4, -1 },    /* 3 */
    { 3, 5, -1, -1 },   /* 4 */
    { 4, -1, -1, -1 },  /* 5 */
    { 2, -1, -1, -1 },  /* 6 */
};

static void bfs_hops(int start, int *dist)
{
    int queue[BFS_N];
    int head = 0, tail = 0;

    for (int i = 0; i < BFS_N; i++) dist[i] = -1;
    dist[start] = 0;
    queue[tail++] = start;

    while (head < tail) {
        int node = queue[head++];
        for (int k = 0; k < 4 && BFS_ADJ[node][k] != -1; k++) {
            int next = BFS_ADJ[node][k];
            if (dist[next] == -1) {
                dist[next] = dist[node] + 1;
                queue[tail++] = next;
            }
        }
    }
}

/* ===========================================================================
 * 4. DIJKSTRA — cheapest paths in a weighted graph
 * ===========================================================================
 * Greedy: always settle the unvisited node with the smallest known cost;
 * its cost can never improve (no negative weights), so it is final.
 * O(V^2) array scan — simple, and identical in all four languages. Real
 * systems use a binary heap for O((V+E) log V).
 */
#define DIJ_N 6
#define DIJ_INF (1 << 29)

/* Undirected weighted graph (the classic textbook example), 0 = no edge. */
static const int DIJ_W[DIJ_N][DIJ_N] = {
    /*        0   1   2   3   4   5  */
    /* 0 */ { 0,  7,  9,  0,  0, 14 },
    /* 1 */ { 7,  0, 10, 15,  0,  0 },
    /* 2 */ { 9, 10,  0, 11,  0,  2 },
    /* 3 */ { 0, 15, 11,  0,  6,  0 },
    /* 4 */ { 0,  0,  0,  6,  0,  9 },
    /* 5 */ {14,  0,  2,  0,  9,  0 },
};

static void dijkstra(int start, int *dist)
{
    int visited[DIJ_N] = { 0 };

    for (int i = 0; i < DIJ_N; i++) dist[i] = DIJ_INF;
    dist[start] = 0;

    for (int round = 0; round < DIJ_N; round++) {
        int u = -1;
        for (int i = 0; i < DIJ_N; i++) {
            if (!visited[i] && (u == -1 || dist[i] < dist[u])) u = i;
        }
        if (u == -1 || dist[u] == DIJ_INF) break;
        visited[u] = 1;

        for (int v = 0; v < DIJ_N; v++) {
            if (DIJ_W[u][v] > 0 && dist[u] + DIJ_W[u][v] < dist[v]) {
                dist[v] = dist[u] + DIJ_W[u][v];
            }
        }
    }
}

/* ===========================================================================
 * 5. EDIT DISTANCE — dynamic programming on two strings
 * ===========================================================================
 * dp[i][j] = min edits turning a[0..i) into b[0..j): delete, insert, or
 * substitute. Overlapping subproblems solved once each — the DP signature.
 */
static int min3(int a, int b, int c)
{
    int m = (a < b) ? a : b;
    return (c < m) ? c : m;
}

static int edit_distance(const char *a, const char *b)
{
    int m = (int)strlen(a), n = (int)strlen(b);
    int dp[16][16]; /* inputs are short; heap-free */

    for (int i = 0; i <= m; i++) dp[i][0] = i;
    for (int j = 0; j <= n; j++) dp[0][j] = j;

    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            dp[i][j] = min3(dp[i - 1][j] + 1,        /* delete   */
                            dp[i][j - 1] + 1,        /* insert   */
                            dp[i - 1][j - 1] + cost);/* subst    */
        }
    }
    return dp[m][n];
}

/* ===========================================================================
 * 6. INTERVAL SCHEDULING — greedy by earliest end time
 * ===========================================================================
 * Sort by end; always take the meeting that frees the room soonest.
 * Provably optimal — one of the rare greedy problems where local best IS
 * global best.
 */
typedef struct {
    int start, end;
} Interval;

static int cmp_interval_end(const void *x, const void *y)
{
    return ((const Interval *)x)->end - ((const Interval *)y)->end;
}

/* ===========================================================================
 * 7. UNION-FIND — near-O(1) dynamic connectivity
 * ===========================================================================
 * Forest of trees; find() walks to the root with PATH HALVING (each node
 * re-points to its grandparent), keeping trees flat.
 */
static int uf_parent[8];

static void uf_init(int n)
{
    for (int i = 0; i < n; i++) uf_parent[i] = i;
}

static int uf_find(int x)
{
    while (uf_parent[x] != x) {
        uf_parent[x] = uf_parent[uf_parent[x]]; /* path halving */
        x = uf_parent[x];
    }
    return x;
}

static void uf_union(int a, int b)
{
    uf_parent[uf_find(a)] = uf_find(b);
}

static int uf_connected(int a, int b)
{
    return uf_find(a) == uf_find(b);
}

/* ===========================================================================
 * 8. TOPOLOGICAL SORT — Kahn's algorithm
 * ===========================================================================
 * Repeatedly emit a node with in-degree 0 and delete its edges. FIFO
 * processing + fixed adjacency order keeps the output deterministic
 * across all four languages.
 */
#define TOPO_N 6

static const int TOPO_ADJ[TOPO_N][3] = {
    { -1, -1, -1 },  /* 0 */
    { -1, -1, -1 },  /* 1 */
    { 3, -1, -1 },   /* 2 */
    { 1, -1, -1 },   /* 3 */
    { 0, 1, -1 },    /* 4 */
    { 2, 0, -1 },    /* 5 */
};

static void topo_sort(int *order)
{
    int indeg[TOPO_N] = { 0 };
    int queue[TOPO_N];
    int head = 0, tail = 0, out = 0;

    for (int u = 0; u < TOPO_N; u++) {
        for (int k = 0; k < 3 && TOPO_ADJ[u][k] != -1; k++) {
            indeg[TOPO_ADJ[u][k]]++;
        }
    }
    for (int u = 0; u < TOPO_N; u++) {
        if (indeg[u] == 0) queue[tail++] = u;
    }
    while (head < tail) {
        int u = queue[head++];
        order[out++] = u;
        for (int k = 0; k < 3 && TOPO_ADJ[u][k] != -1; k++) {
            if (--indeg[TOPO_ADJ[u][k]] == 0) {
                queue[tail++] = TOPO_ADJ[u][k];
            }
        }
    }
}

/* ===========================================================================
 * 9. KADANE — maximum subarray sum in one pass
 * ===========================================================================
 * best_here = best sum of a subarray ENDING at i: either extend the
 * previous one or start fresh. O(n), O(1) space — DP distilled to two
 * variables.
 */
static int kadane(const int *a, int n)
{
    int best_here = a[0];
    int best = a[0];
    for (int i = 1; i < n; i++) {
        best_here = (a[i] > best_here + a[i]) ? a[i] : best_here + a[i];
        if (best_here > best) best = best_here;
    }
    return best;
}

/* ===========================================================================
 * 10. RABIN-KARP — substring search by rolling hash
 * ===========================================================================
 * Hash the pattern once; roll the window hash across the text in O(1) per
 * step. Hash match -> verify char-by-char (hashes can collide).
 */
static int rabin_karp(const char *text, const char *pat, int *out)
{
    const uint64_t D = 256, Q = 101;
    int n = (int)strlen(text), m = (int)strlen(pat);
    int count = 0;

    uint64_t high = 1; /* D^(m-1) mod Q, for removing the leaving char */
    for (int i = 0; i < m - 1; i++) high = (high * D) % Q;

    uint64_t ph = 0, th = 0;
    for (int i = 0; i < m; i++) {
        ph = (ph * D + (unsigned char)pat[i]) % Q;
        th = (th * D + (unsigned char)text[i]) % Q;
    }

    for (int i = 0; i + m <= n; i++) {
        if (ph == th && memcmp(text + i, pat, (size_t)m) == 0) {
            out[count++] = i; /* verified match */
        }
        if (i + m < n) {
            th = (th + Q * D - (unsigned char)text[i] * high % Q) % Q;
            th = (th * D + (unsigned char)text[i + m]) % Q;
        }
    }
    return count;
}

/* ===========================================================================
 * 11. RESERVOIR SAMPLING (novel #1) — uniform sample from a stream
 * ===========================================================================
 * Keep the first k items; thereafter item i replaces a random slot with
 * probability k/(i+1). One pass, O(k) memory, stream length unknown in
 * advance — every item ends up in the sample with equal probability.
 * The tiny LCG is identical in all four languages, so the "random" sample
 * is reproducible everywhere (seeded randomness = testable randomness).
 */
static uint64_t g_lcg_state;

static uint64_t lcg_next(void)
{
    g_lcg_state = g_lcg_state * 6364136223846793005ULL + 1442695040888963407ULL;
    return g_lcg_state;
}

static void reservoir_sample(int stream_len, int k, int *out)
{
    for (int i = 0; i < k; i++) out[i] = i;
    for (int i = k; i < stream_len; i++) {
        uint64_t j = lcg_next() % (uint64_t)(i + 1);
        if (j < (uint64_t)k) {
            out[j] = i;
        }
    }
}

/* ===========================================================================
 * 12. HYPERLOGLOG (novel #2) — count distinct items in O(1) memory
 * ===========================================================================
 * Insight: the maximum number of leading zero bits seen in hashed items
 * estimates log2 of the distinct count. 256 registers each track their own
 * maximum; the harmonic mean combines them. ~2% typical error here in 256
 * BYTES of state — regardless of whether the stream has thousands or
 * billions of distinct items. This is Redis's PFCOUNT.
 */
#define HLL_M 256 /* registers; 8 index bits */

/* FNV-1a is fast but its high bits avalanche poorly on similar keys, which
 * skews HLL's leading-zero statistics. The murmur3 finalizer fixes that:
 * good hashes are a prerequisite for probabilistic algorithms. */
static uint64_t fmix64(uint64_t h)
{
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
}

static uint64_t fnv1a64(const char *s)
{
    uint64_t h = 14695981039346656037ULL;
    for (; *s; s++) {
        h ^= (unsigned char)*s;
        h *= 1099511628211ULL;
    }
    return fmix64(h);
}

static void hll_add(uint8_t *reg, const char *item)
{
    uint64_t h = fnv1a64(item);
    int idx = (int)(h & (HLL_M - 1)); /* low 8 bits pick the register */
    uint64_t w = h >> 8;              /* remaining 56 bits */

    /* rho = position of the first 1-bit from the top of the 56 bits. */
    int rho = 1;
    uint64_t mask = 1ULL << 55;
    while (mask != 0 && (w & mask) == 0) {
        rho++;
        mask >>= 1;
    }
    if (rho > reg[idx]) reg[idx] = (uint8_t)rho;
}

static long long hll_estimate(const uint8_t *reg)
{
    double sum = 0.0;
    for (int j = 0; j < HLL_M; j++) {
        sum += 1.0 / (double)(1ULL << reg[j]);
    }
    double alpha = 0.7213 / (1.0 + 1.079 / 256.0);
    double estimate = alpha * 256.0 * 256.0 / sum;
    return (long long)(estimate + 0.5);
}

/* ===========================================================================
 * MAIN — runs all twelve with the shared, cross-language output format
 * ===========================================================================
 */

int main(void)
{
    printf("language: C\n");

    /* 1 --------------------------------------------------------------- */
    printf("\n== 1. binary search ==\n");
    int sorted[] = { 2, 5, 8, 12, 16, 23, 38, 56, 72, 91 };
    printf("sorted: ");
    print_array(sorted, 10);
    printf("\n");
    printf("index of 23 -> %d\n", binary_search(sorted, 10, 23));
    printf("index of 4 -> %d\n", binary_search(sorted, 10, 4));

    /* 2 --------------------------------------------------------------- */
    printf("\n== 2. quicksort ==\n");
    int unsorted[] = { 42, 7, 19, 3, 88, 27, 1, 65 };
    printf("before: ");
    print_array(unsorted, 8);
    printf("\n");
    quicksort(unsorted, 0, 7);
    printf("after:  ");
    print_array(unsorted, 8);
    printf("\n");

    /* 3 --------------------------------------------------------------- */
    printf("\n== 3. breadth-first search ==\n");
    int hops[BFS_N];
    bfs_hops(0, hops);
    printf("hops from node 0: ");
    print_array(hops, BFS_N);
    printf("\n");

    /* 4 --------------------------------------------------------------- */
    printf("\n== 4. dijkstra shortest paths ==\n");
    int dist[DIJ_N];
    dijkstra(0, dist);
    printf("cheapest cost from node 0: ");
    print_array(dist, DIJ_N);
    printf("\n");

    /* 5 --------------------------------------------------------------- */
    printf("\n== 5. edit distance (dynamic programming) ==\n");
    printf("kitten -> sitting: %d\n", edit_distance("kitten", "sitting"));
    printf("sunday -> saturday: %d\n", edit_distance("sunday", "saturday"));

    /* 6 --------------------------------------------------------------- */
    printf("\n== 6. interval scheduling (greedy) ==\n");
    Interval meetings[] = {
        { 0, 6 }, { 1, 4 }, { 3, 5 }, { 3, 8 },
        { 4, 7 }, { 5, 9 }, { 6, 10 }, { 8, 11 },
    };
    qsort(meetings, 8, sizeof meetings[0], cmp_interval_end);
    Interval picked[8];
    int picked_count = 0;
    int busy_until = -1;
    for (int i = 0; i < 8; i++) {
        if (meetings[i].start >= busy_until) {
            picked[picked_count++] = meetings[i];
            busy_until = meetings[i].end;
        }
    }
    printf("picked %d meetings: [", picked_count);
    for (int i = 0; i < picked_count; i++) {
        printf("(%d,%d)%s", picked[i].start, picked[i].end,
               (i + 1 < picked_count) ? ", " : "");
    }
    printf("]\n");

    /* 7 --------------------------------------------------------------- */
    printf("\n== 7. union-find (disjoint sets) ==\n");
    uf_init(8);
    uf_union(0, 1);
    uf_union(1, 2);
    uf_union(3, 4);
    printf("connected(0,2) -> %s\n", uf_connected(0, 2) ? "true" : "false");
    printf("connected(0,3) -> %s\n", uf_connected(0, 3) ? "true" : "false");
    uf_union(2, 3);
    printf("after union(2,3): connected(0,4) -> %s\n",
           uf_connected(0, 4) ? "true" : "false");

    /* 8 --------------------------------------------------------------- */
    printf("\n== 8. topological sort (kahn) ==\n");
    int order[TOPO_N];
    topo_sort(order);
    printf("build order: ");
    print_array(order, TOPO_N);
    printf("\n");

    /* 9 --------------------------------------------------------------- */
    printf("\n== 9. kadane max subarray ==\n");
    int gains[] = { -2, 1, -3, 4, -1, 2, 1, -5, 4 };
    printf("daily changes: ");
    print_array(gains, 9);
    printf("\n");
    printf("best run sum: %d\n", kadane(gains, 9));

    /* 10 -------------------------------------------------------------- */
    printf("\n== 10. rabin-karp substring search ==\n");
    int matches[8];
    int match_count = rabin_karp("abracadabra", "abra", matches);
    printf("\"abra\" in \"abracadabra\" at: ");
    print_array(matches, match_count);
    printf("\n");

    /* 11 -------------------------------------------------------------- */
    printf("\n== 11. reservoir sampling (streaming) ==\n");
    g_lcg_state = 42;
    int sample[5];
    reservoir_sample(1000, 5, sample);
    printf("uniform sample of 5 from a 1000-item stream: ");
    print_array(sample, 5);
    printf("\n");

    /* 12 -------------------------------------------------------------- */
    printf("\n== 12. hyperloglog (probabilistic counting) ==\n");
    uint8_t registers[HLL_M] = { 0 };
    char item[32];
    for (int i = 0; i < 10000; i++) {
        snprintf(item, sizeof item, "user-%d", i % 1000);
        hll_add(registers, item);
    }
    printf("stream: 10000 items, 1000 distinct\n");
    printf("estimated distinct (256 bytes of state): %lld\n",
           hll_estimate(registers));

    printf("\nall 12 algorithms demonstrated.\n");
    return 0;
}
