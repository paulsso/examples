// ============================================================================
//  main.rs — Algorithms & Their Use Cases (Rust implementation)
// ============================================================================
//
//  One of FOUR implementations of the same twelve algorithms (C, C++, Rust,
//  Python), engineered to print BYTE-IDENTICAL output after the first line —
//  `make verify` diffs them. Same algorithms as main.c; the idiomatic
//  differences are Rust's: slices instead of pointer+length, Vec instead of
//  fixed arrays, wrapping_* for the deliberate modular arithmetic, and the
//  borrow checker watching every index. The ALGORITHMS do not change.
// ============================================================================

/// Formats "[a, b, c]" exactly like the other three languages.
fn fmt_array(a: &[i64]) -> String {
    let items: Vec<String> = a.iter().map(|v| v.to_string()).collect();
    format!("[{}]", items.join(", "))
}

// ============================== 1. binary search ===========================

fn binary_search_idx(a: &[i64], target: i64) -> i64 {
    let (mut lo, mut hi) = (0usize, a.len());
    while lo < hi {
        let mid = lo + (hi - lo) / 2;
        if a[mid] == target {
            return mid as i64;
        }
        if a[mid] < target {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    -1
}

// ================================ 2. quicksort =============================

fn quicksort(a: &mut [i64], lo: isize, hi: isize) {
    if lo >= hi {
        return;
    }
    let pivot = a[hi as usize];
    let mut i = lo;
    for j in lo..hi {
        if a[j as usize] < pivot {
            a.swap(i as usize, j as usize);
            i += 1;
        }
    }
    a.swap(i as usize, hi as usize);
    quicksort(a, lo, i - 1);
    quicksort(a, i + 1, hi);
}

// ========================== 3. breadth-first search ========================

fn bfs_hops(adj: &[Vec<usize>], start: usize) -> Vec<i64> {
    let mut dist = vec![-1i64; adj.len()];
    let mut queue = Vec::new();
    let mut head = 0;

    dist[start] = 0;
    queue.push(start);
    while head < queue.len() {
        let node = queue[head];
        head += 1;
        for &next in &adj[node] {
            if dist[next] == -1 {
                dist[next] = dist[node] + 1;
                queue.push(next);
            }
        }
    }
    dist
}

// ================================ 4. dijkstra ==============================

fn dijkstra(w: &[Vec<i64>], start: usize) -> Vec<i64> {
    const INF: i64 = 1 << 29;
    let n = w.len();
    let mut dist = vec![INF; n];
    let mut visited = vec![false; n];
    dist[start] = 0;

    for _ in 0..n {
        let mut u = usize::MAX;
        for i in 0..n {
            if !visited[i] && (u == usize::MAX || dist[i] < dist[u]) {
                u = i;
            }
        }
        if u == usize::MAX || dist[u] == INF {
            break;
        }
        visited[u] = true;
        for v in 0..n {
            if w[u][v] > 0 && dist[u] + w[u][v] < dist[v] {
                dist[v] = dist[u] + w[u][v];
            }
        }
    }
    dist
}

// ============================== 5. edit distance ===========================

fn edit_distance(a: &str, b: &str) -> i64 {
    let (a, b) = (a.as_bytes(), b.as_bytes());
    let (m, n) = (a.len(), b.len());
    let mut dp = vec![vec![0i64; n + 1]; m + 1];

    for (i, row) in dp.iter_mut().enumerate() {
        row[0] = i as i64;
    }
    for j in 0..=n {
        dp[0][j] = j as i64;
    }
    for i in 1..=m {
        for j in 1..=n {
            let cost = if a[i - 1] == b[j - 1] { 0 } else { 1 };
            dp[i][j] = (dp[i - 1][j] + 1)
                .min(dp[i][j - 1] + 1)
                .min(dp[i - 1][j - 1] + cost);
        }
    }
    dp[m][n]
}

// =============================== 7. union-find =============================

struct UnionFind {
    parent: Vec<usize>,
}

impl UnionFind {
    fn new(n: usize) -> Self {
        UnionFind { parent: (0..n).collect() }
    }

    fn find(&mut self, mut x: usize) -> usize {
        while self.parent[x] != x {
            self.parent[x] = self.parent[self.parent[x]]; // path halving
            x = self.parent[x];
        }
        x
    }

    fn unite(&mut self, a: usize, b: usize) {
        let (ra, rb) = (self.find(a), self.find(b));
        self.parent[ra] = rb;
    }

    fn connected(&mut self, a: usize, b: usize) -> bool {
        self.find(a) == self.find(b)
    }
}

// ============================ 8. topological sort ==========================

fn topo_sort(adj: &[Vec<usize>]) -> Vec<i64> {
    let n = adj.len();
    let mut indeg = vec![0usize; n];
    let mut queue = Vec::new();
    let mut order = Vec::new();
    let mut head = 0;

    for neighbors in adj {
        for &v in neighbors {
            indeg[v] += 1;
        }
    }
    for (u, &d) in indeg.iter().enumerate() {
        if d == 0 {
            queue.push(u);
        }
    }
    while head < queue.len() {
        let u = queue[head];
        head += 1;
        order.push(u as i64);
        for &v in &adj[u] {
            indeg[v] -= 1;
            if indeg[v] == 0 {
                queue.push(v);
            }
        }
    }
    order
}

// ================================= 9. kadane ===============================

fn kadane(a: &[i64]) -> i64 {
    let mut best_here = a[0];
    let mut best = a[0];
    for &x in &a[1..] {
        best_here = x.max(best_here + x);
        best = best.max(best_here);
    }
    best
}

// =============================== 10. rabin-karp ============================

fn rabin_karp(text: &str, pat: &str) -> Vec<i64> {
    const D: u64 = 256;
    const Q: u64 = 101;
    let (text, pat) = (text.as_bytes(), pat.as_bytes());
    let (n, m) = (text.len(), pat.len());
    let mut matches = Vec::new();

    let mut high: u64 = 1;
    for _ in 0..m.saturating_sub(1) {
        high = (high * D) % Q;
    }

    let (mut ph, mut th) = (0u64, 0u64);
    for i in 0..m {
        ph = (ph * D + pat[i] as u64) % Q;
        th = (th * D + text[i] as u64) % Q;
    }

    for i in 0..=(n - m) {
        if ph == th && &text[i..i + m] == pat {
            matches.push(i as i64);
        }
        if i + m < n {
            th = (th + Q * D - (text[i] as u64) * high % Q) % Q;
            th = (th * D + text[i + m] as u64) % Q;
        }
    }
    matches
}

// =========================== 11. reservoir sampling ========================

struct Lcg {
    state: u64,
}

impl Lcg {
    fn next(&mut self) -> u64 {
        self.state = self
            .state
            .wrapping_mul(6364136223846793005)
            .wrapping_add(1442695040888963407);
        self.state
    }
}

fn reservoir_sample(stream_len: usize, k: usize, rng: &mut Lcg) -> Vec<i64> {
    let mut out: Vec<i64> = (0..k as i64).collect();
    for i in k..stream_len {
        let j = rng.next() % (i as u64 + 1);
        if j < k as u64 {
            out[j as usize] = i as i64;
        }
    }
    out
}

// ============================== 12. hyperloglog ============================

// FNV-1a's high bits avalanche poorly on similar keys; the murmur3
// finalizer fixes the bit distribution HLL depends on.
fn fmix64(mut h: u64) -> u64 {
    h ^= h >> 33;
    h = h.wrapping_mul(0xff51afd7ed558ccd);
    h ^= h >> 33;
    h = h.wrapping_mul(0xc4ceb9fe1a85ec53);
    h ^= h >> 33;
    h
}

fn fnv1a64(s: &str) -> u64 {
    let mut h: u64 = 14695981039346656037;
    for byte in s.bytes() {
        h ^= byte as u64;
        h = h.wrapping_mul(1099511628211);
    }
    fmix64(h)
}

struct HyperLogLog {
    reg: [u8; 256],
}

impl HyperLogLog {
    fn new() -> Self {
        HyperLogLog { reg: [0; 256] }
    }

    fn add(&mut self, item: &str) {
        let h = fnv1a64(item);
        let idx = (h & 255) as usize;
        let w = h >> 8;

        let mut rho: u8 = 1;
        let mut mask: u64 = 1 << 55;
        while mask != 0 && (w & mask) == 0 {
            rho += 1;
            mask >>= 1;
        }
        if rho > self.reg[idx] {
            self.reg[idx] = rho;
        }
    }

    fn estimate(&self) -> i64 {
        let mut sum = 0.0f64;
        for &r in self.reg.iter() {
            sum += 1.0 / ((1u64 << r) as f64);
        }
        let alpha = 0.7213 / (1.0 + 1.079 / 256.0);
        let e = alpha * 256.0 * 256.0 / sum;
        (e + 0.5) as i64
    }
}

// ==================================== main =================================

fn main() {
    println!("language: Rust");

    // 1
    println!("\n== 1. binary search ==");
    let sorted: Vec<i64> = vec![2, 5, 8, 12, 16, 23, 38, 56, 72, 91];
    println!("sorted: {}", fmt_array(&sorted));
    println!("index of 23 -> {}", binary_search_idx(&sorted, 23));
    println!("index of 4 -> {}", binary_search_idx(&sorted, 4));

    // 2
    println!("\n== 2. quicksort ==");
    let mut unsorted: Vec<i64> = vec![42, 7, 19, 3, 88, 27, 1, 65];
    println!("before: {}", fmt_array(&unsorted));
    let hi = unsorted.len() as isize - 1;
    quicksort(&mut unsorted, 0, hi);
    println!("after:  {}", fmt_array(&unsorted));

    // 3
    println!("\n== 3. breadth-first search ==");
    let bfs_adj: Vec<Vec<usize>> = vec![
        vec![1, 2],
        vec![0, 3],
        vec![0, 3, 6],
        vec![1, 2, 4],
        vec![3, 5],
        vec![4],
        vec![2],
    ];
    println!("hops from node 0: {}", fmt_array(&bfs_hops(&bfs_adj, 0)));

    // 4
    println!("\n== 4. dijkstra shortest paths ==");
    let weights: Vec<Vec<i64>> = vec![
        vec![0, 7, 9, 0, 0, 14],
        vec![7, 0, 10, 15, 0, 0],
        vec![9, 10, 0, 11, 0, 2],
        vec![0, 15, 11, 0, 6, 0],
        vec![0, 0, 0, 6, 0, 9],
        vec![14, 0, 2, 0, 9, 0],
    ];
    println!("cheapest cost from node 0: {}", fmt_array(&dijkstra(&weights, 0)));

    // 5
    println!("\n== 5. edit distance (dynamic programming) ==");
    println!("kitten -> sitting: {}", edit_distance("kitten", "sitting"));
    println!("sunday -> saturday: {}", edit_distance("sunday", "saturday"));

    // 6
    println!("\n== 6. interval scheduling (greedy) ==");
    let mut meetings: Vec<(i64, i64)> = vec![
        (0, 6), (1, 4), (3, 5), (3, 8), (4, 7), (5, 9), (6, 10), (8, 11),
    ];
    meetings.sort_by_key(|m| m.1);
    let mut picked: Vec<(i64, i64)> = Vec::new();
    let mut busy_until = -1i64;
    for &(start, end) in &meetings {
        if start >= busy_until {
            picked.push((start, end));
            busy_until = end;
        }
    }
    let picked_str: Vec<String> = picked.iter().map(|(s, e)| format!("({},{})", s, e)).collect();
    println!("picked {} meetings: [{}]", picked.len(), picked_str.join(", "));

    // 7
    println!("\n== 7. union-find (disjoint sets) ==");
    let mut uf = UnionFind::new(8);
    uf.unite(0, 1);
    uf.unite(1, 2);
    uf.unite(3, 4);
    println!("connected(0,2) -> {}", uf.connected(0, 2));
    println!("connected(0,3) -> {}", uf.connected(0, 3));
    uf.unite(2, 3);
    println!("after union(2,3): connected(0,4) -> {}", uf.connected(0, 4));

    // 8
    println!("\n== 8. topological sort (kahn) ==");
    let topo_adj: Vec<Vec<usize>> = vec![
        vec![],
        vec![],
        vec![3],
        vec![1],
        vec![0, 1],
        vec![2, 0],
    ];
    println!("build order: {}", fmt_array(&topo_sort(&topo_adj)));

    // 9
    println!("\n== 9. kadane max subarray ==");
    let gains: Vec<i64> = vec![-2, 1, -3, 4, -1, 2, 1, -5, 4];
    println!("daily changes: {}", fmt_array(&gains));
    println!("best run sum: {}", kadane(&gains));

    // 10
    println!("\n== 10. rabin-karp substring search ==");
    println!(
        "\"abra\" in \"abracadabra\" at: {}",
        fmt_array(&rabin_karp("abracadabra", "abra"))
    );

    // 11
    println!("\n== 11. reservoir sampling (streaming) ==");
    let mut rng = Lcg { state: 42 };
    println!(
        "uniform sample of 5 from a 1000-item stream: {}",
        fmt_array(&reservoir_sample(1000, 5, &mut rng))
    );

    // 12
    println!("\n== 12. hyperloglog (probabilistic counting) ==");
    let mut hll = HyperLogLog::new();
    for i in 0..10000 {
        hll.add(&format!("user-{}", i % 1000));
    }
    println!("stream: 10000 items, 1000 distinct");
    println!("estimated distinct (256 bytes of state): {}", hll.estimate());

    println!("\nall 12 algorithms demonstrated.");
}
