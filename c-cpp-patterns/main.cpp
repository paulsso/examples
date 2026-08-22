/*
 * ============================================================================
 *  main.cpp — Patterns & Anti-Patterns in C and C++
 * ============================================================================
 *
 *  This is a CONCEPTUAL course: the real course text is DOCUMENTATION.md,
 *  which explains why each anti-pattern emerges, what it costs, how to
 *  refactor out of it, and — crucially — when the "anti-pattern" is
 *  actually the right call. This file is the course's living illustration:
 *  every chapter runs the anti-pattern and the pattern side by side so the
 *  difference is observable, not just asserted.
 *
 *  A note on safety: several anti-patterns (leaks, double frees, data
 *  races) are undefined behavior when executed for real. This program
 *  never invokes UB. Instead it SIMULATES the disasters with logging
 *  counters — a fake resource registry counts leaked handles, a fake
 *  allocator records the double free — so the failure is visible in the
 *  output while the program itself stays well-defined. Where even that is
 *  impossible, the bad code appears in comments marked `// UB:`.
 *
 *  Chapters:
 *    1.  Ownership & Cleanup      — leaks, goto-cleanup (a C pattern!), RAII
 *    2.  Error Handling           — sentinel soup, ignored returns, Result
 *    3.  Magic Values             — magic numbers, the boolean trap,
 *                                   strong types
 *    4.  Global State             — action at a distance, explicit context,
 *                                   the least-bad singleton
 *    5.  Buffers & Strings        — unbounded copies, truncation traps,
 *                                   size-carrying APIs
 *    6.  Undefined Behavior       — the bestiary, and the defensive idioms
 *    7.  Function Shape           — arrow code, guard clauses, parameter
 *                                   structs, table-driven logic
 *    8.  The Preprocessor         — double evaluation, constexpr,
 *                                   the legitimate X-macro
 *    9.  Inheritance Abuse        — slicing, hierarchy-as-reuse,
 *                                   composition
 *    10. Copies & Ownership       — shallow copy, rule of three/five/zero,
 *                                   measuring copies
 *    11. Concurrency              — the read-modify-write hole, locks done
 *                                   right
 *    12. Design for Change        — hidden dependencies, premature
 *                                   pessimization, const as contract
 *
 *  Build:  make          (produces the `main` binary)
 *  Run:    ./main        (or `make run`)
 * ============================================================================
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static void chapter(const std::string& title)
{
    std::cout << "\n=============================================\n"
              << " " << title << "\n"
              << "=============================================\n";
}

/* Small helpers to label the two sides of every demo. */
static void anti(const std::string& text) { std::cout << "ANTI  " << text << "\n"; }
static void good(const std::string& text) { std::cout << "GOOD  " << text << "\n"; }

/* ===========================================================================
 * CHAPTER 1 — OWNERSHIP & CLEANUP
 * ===========================================================================
 * The anti-pattern: acquire resources, then leak them on one of the early
 * returns nobody tests. The C-side pattern: single-exit cleanup with goto
 * (yes — a *pattern* in C). The C++ pattern: RAII, which removes the
 * cleanup code path entirely.
 *
 * The simulated resource registry below counts open handles, so leaks are
 * VISIBLE in the output instead of invisible until production.
 */

static int g_open_handles = 0;

static int acquire_handle(const char* name)
{
    static int next_id = 100;
    ++g_open_handles;
    std::cout << "    open  " << name << " (handles now " << g_open_handles << ")\n";
    return next_id++;
}

static void release_handle(int /*id*/, const char* name)
{
    --g_open_handles;
    std::cout << "    close " << name << " (handles now " << g_open_handles << ")\n";
}

/*
 * leaky_load
 * ----------
 * THE ANTI-PATTERN. Two acquisitions, then a validation failure returns
 * early — past the cleanup. Every early return is a separate cleanup site,
 * and one of them is always missed. (Errors, not the happy path, are where
 * leaks live: the failure branch is the one nobody exercises.)
 */
static bool leaky_load(bool input_is_valid)
{
    int file = acquire_handle("config.txt");
    int socket = acquire_handle("telemetry-socket");

    if (!input_is_valid) {
        return false; // LEAK: both handles still open
    }

    release_handle(file, "config.txt");
    release_handle(socket, "telemetry-socket");
    return true;
}

/*
 * goto_cleanup_load
 * -----------------
 * The accepted C PATTERN for the same problem: one exit point, cleanup in
 * reverse acquisition order, `goto fail` from anywhere. This is how the
 * Linux kernel does it — in C, disciplined goto is the fix, not the sin.
 */
static bool goto_cleanup_load(bool input_is_valid)
{
    bool ok = false;
    int file = acquire_handle("config.txt");
    int socket = acquire_handle("telemetry-socket");

    if (!input_is_valid) {
        goto cleanup;
    }

    ok = true;

cleanup:
    release_handle(socket, "telemetry-socket");
    release_handle(file, "config.txt");
    return ok;
}

/*
 * class ScopedHandle + raii_load
 * ------------------------------
 * The C++ PATTERN: the resource cleans itself up in its destructor. There
 * is no cleanup path to forget, because there is no cleanup path at all —
 * every return, exception, or new early exit added next year is covered.
 */
class ScopedHandle {
public:
    explicit ScopedHandle(const char* name)
        : name_(name), id_(acquire_handle(name)) {}
    ~ScopedHandle() { release_handle(id_, name_); }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

private:
    const char* name_;
    int id_;
};

static bool raii_load(bool input_is_valid)
{
    ScopedHandle file("config.txt");
    ScopedHandle socket("telemetry-socket");

    if (!input_is_valid) {
        return false; // destructors run anyway — no leak possible
    }
    return true;
}

static void demo_ownership_and_cleanup()
{
    anti("early return past manual cleanup:");
    leaky_load(false);
    std::cout << "    => leaked handles: " << g_open_handles << "  (invisible in real code)\n";
    g_open_handles = 0; // reset the simulation

    good("C pattern — single-exit goto cleanup:");
    goto_cleanup_load(false);
    std::cout << "    => leaked handles: " << g_open_handles << "\n";

    good("C++ pattern — RAII (no cleanup code at all):");
    raii_load(false);
    std::cout << "    => leaked handles: " << g_open_handles << "\n";
}

/* ===========================================================================
 * CHAPTER 2 — ERROR HANDLING
 * ===========================================================================
 * Anti-patterns: every function inventing its own failure sentinel
 * (-1? 0? NULL? INT_MAX?), and callers ignoring returns entirely.
 * Patterns: one status enum for the module (C), or a Result type that
 * physically couples the value to its validity (C++).
 */

/*
 * The sentinel soup, reproduced. Three functions, three conventions —
 * the caller must memorize which is which, and the compiler checks none
 * of it.
 */
static int find_user_bad(const char* name)      /* -1 = failure */
{
    return (std::strcmp(name, "admin") == 0) ? 7 : -1;
}
static int count_sessions_bad(bool db_up)       /* 0 = failure... or zero sessions? */
{
    return db_up ? 3 : 0;
}

/*
 * ignored_write
 * -------------
 * Simulates a short write (disk full): returns how many bytes it stored.
 * The anti-pattern is at the CALL SITE — the return value evaporates.
 */
static std::size_t ignored_write(std::size_t requested)
{
    return requested / 2; // only half fit
}

/* The C pattern: one enum, every function in the module uses it, the
 * payload travels through an out-parameter. Boring and uniform — exactly
 * what error handling should be. */
enum class Status { Ok, NotFound, Unavailable };

static const char* status_name(Status s)
{
    switch (s) {
    case Status::Ok:          return "Ok";
    case Status::NotFound:    return "NotFound";
    case Status::Unavailable: return "Unavailable";
    }
    return "?";
}

static Status find_user_good(const char* name, int* out_id)
{
    if (std::strcmp(name, "admin") != 0) {
        return Status::NotFound;
    }
    *out_id = 7;
    return Status::Ok;
}

/*
 * Result<T, E>
 * ------------
 * The C++ pattern: value and validity in one object, so an unchecked
 * "value" cannot exist. This is a 20-line teaching version of
 * std::expected (C++23) / Rust's Result.
 */
template <typename T, typename E>
struct Result {
    bool ok;
    T value{};
    E error{};

    static Result success(T v) { return { true, v, E{} }; }
    static Result failure(E e) { return { false, T{}, e }; }
};

static Result<int, Status> parse_port(const std::string& text)
{
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos) {
        return Result<int, Status>::failure(Status::NotFound);
    }
    int port = std::stoi(text);
    if (port < 1 || port > 65535) {
        return Result<int, Status>::failure(Status::Unavailable);
    }
    return Result<int, Status>::success(port);
}

static void demo_error_handling()
{
    anti("sentinel soup — every function, a different failure value:");
    std::cout << "    find_user(\"guest\") = " << find_user_bad("guest")
              << "   (-1 means failure here)\n";
    std::cout << "    count_sessions(down) = " << count_sessions_bad(false)
              << "    (0 means failure here... or zero sessions?)\n";

    anti("ignored return — a short write nobody notices:");
    ignored_write(1024); // return value silently dropped
    std::cout << "    wrote 1024 bytes... actually stored 512; data lost silently\n";

    good("C pattern — one status enum + out-parameter:");
    int id = 0;
    Status s = find_user_good("guest", &id);
    std::cout << "    find_user(\"guest\") -> " << status_name(s) << "\n";
    s = find_user_good("admin", &id);
    std::cout << "    find_user(\"admin\") -> " << status_name(s) << ", id=" << id << "\n";

    good("C++ pattern — Result couples value to validity:");
    for (const char* input : { "8080", "99999", "80x0" }) {
        auto r = parse_port(input);
        if (r.ok) {
            std::cout << "    parse_port(\"" << input << "\") -> port " << r.value << "\n";
        } else {
            std::cout << "    parse_port(\"" << input << "\") -> "
                      << status_name(r.error) << " (no half-valid int exists)\n";
        }
    }
}

/* ===========================================================================
 * CHAPTER 3 — MAGIC VALUES
 * ===========================================================================
 * Anti-patterns: bare numeric literals whose meaning lives in someone's
 * head, and the boolean trap — call sites like f(true, false, true) that
 * are write-only code. Patterns: named constants, scoped enums, and
 * strong types that make UNIT mistakes a compile error.
 */

/* The boolean trap, reproduced. Guess what this call configures. */
static void create_window_bad(int w, int h, bool a, bool b, bool c)
{
    std::cout << "    window " << w << "x" << h
              << " fullscreen=" << a << " vsync=" << b << " resizable=" << c << "\n";
}

/* The pattern: an options struct with named, defaulted fields. The call
 * site becomes self-documenting and new options don't break callers. */
struct WindowOptions {
    int width = 1280;
    int height = 720;
    bool fullscreen = false;
    bool vsync = true;
    bool resizable = true;
};

static void create_window_good(const WindowOptions& opt)
{
    std::cout << "    window " << opt.width << "x" << opt.height
              << " fullscreen=" << opt.fullscreen << " vsync=" << opt.vsync
              << " resizable=" << opt.resizable << "\n";
}

/*
 * Strong types (the "newtype" idiom): Meters and Feet both wrap double,
 * but they are DIFFERENT types — adding one to the other does not compile.
 * The Mars Climate Orbiter was lost to exactly the bug this prevents.
 */
struct Meters {
    double value;
};
struct Feet {
    double value;
};

static Meters operator+(Meters a, Meters b) { return { a.value + b.value }; }
static Meters to_meters(Feet f) { return { f.value * 0.3048 }; }

static void demo_magic_values()
{
    anti("magic numbers + boolean trap (what do true, false, true mean?):");
    create_window_bad(1280, 720, true, false, true);

    good("options struct — the call site documents itself:");
    WindowOptions opt;
    opt.fullscreen = true;
    opt.vsync = false;
    create_window_good(opt);

    good("strong types — mixing units is now a COMPILE error:");
    Meters runway{ 3000.0 };
    Feet altitude{ 1000.0 };
    // Meters oops = runway + altitude;        // does not compile — the point
    Meters total = runway + to_meters(altitude); // conversion is explicit
    std::cout << "    3000 m + 1000 ft = " << total.value << " m (conversion forced by the types)\n";
}

/* ===========================================================================
 * CHAPTER 4 — GLOBAL STATE
 * ===========================================================================
 * The anti-pattern: mutable globals create "action at a distance" — module
 * A silently changes behavior of module B, and no function signature warns
 * you. Patterns: pass context explicitly; and when a single instance is
 * genuinely required, the function-local static (Meyers singleton) is the
 * least-bad way to get one.
 */

namespace hidden_coupling {
int g_retry_limit = 3; // shared mutable global

void module_a_tune_for_slow_network() { g_retry_limit = 10; }
int module_b_send() { return g_retry_limit; } // depends on who ran before us
}

/* The pattern: dependencies in the signature. Nothing can change behind
 * the caller's back, and the function is trivially testable. */
struct NetConfig {
    int retry_limit;
};

static int send_with_config(const NetConfig& cfg)
{
    return cfg.retry_limit;
}

/* The least-bad singleton: construction on first use (fixes the static
 * initialization order fiasco), thread-safe since C++11. Reach for it for
 * loggers and registries — not as a default architecture. */
struct Logger {
    int lines_logged = 0;
    void log(const std::string& msg)
    {
        ++lines_logged;
        std::cout << "    [log] " << msg << "\n";
    }
};

static Logger& global_logger()
{
    static Logger instance; // constructed on first call, exactly once
    return instance;
}

static void demo_global_state()
{
    using namespace hidden_coupling;

    anti("action at a distance — module A silently reconfigures module B:");
    std::cout << "    module_b_send() uses " << module_b_send() << " retries\n";
    module_a_tune_for_slow_network(); // somewhere far away...
    std::cout << "    module_b_send() uses " << module_b_send()
              << " retries — nothing in B's code changed\n";

    good("explicit context — behavior visible in the signature:");
    NetConfig fast{ 3 };
    NetConfig slow{ 10 };
    std::cout << "    send_with_config(fast) -> " << send_with_config(fast)
              << ", send_with_config(slow) -> " << send_with_config(slow) << "\n";

    good("when a singleton IS warranted — construct-on-first-use:");
    global_logger().log("first use constructs the logger");
    global_logger().log("second use reuses it");
    std::cout << "    lines logged: " << global_logger().lines_logged << "\n";
}

/* ===========================================================================
 * CHAPTER 5 — BUFFERS & STRINGS
 * ===========================================================================
 * The anti-patterns are famous CVE generators: gets() (unbounded read),
 * strcpy/sprintf (unbounded write), and strncpy (may silently leave the
 * destination UNTERMINATED). Patterns: APIs that carry sizes, always
 * terminate, and report truncation — and in C++, std::string, which makes
 * the whole category disappear.
 */

/*
 * bounded_copy — an strlcpy-style function
 * ----------------------------------------
 * Copies as much as fits, ALWAYS terminates, and returns the length of
 * the source, so `return >= dst_size` detects truncation. This is the
 * C pattern all three broken functions should have been.
 *
 *   // UB: gets(buf);                 reads past any buffer, removed in C11
 *   // UB: strcpy(small, long_str);   writes past the end
 *   // BUG: strncpy(dst, src, n);     no terminator when src fills dst
 */
static std::size_t bounded_copy(char* dst, std::size_t dst_size, const char* src)
{
    std::size_t src_len = std::strlen(src);
    if (dst_size != 0) {
        std::size_t n = std::min(src_len, dst_size - 1);
        std::memcpy(dst, src, n);
        dst[n] = '\0'; // unconditional terminator — the whole point
    }
    return src_len;
}

static void demo_buffers_and_strings()
{
    anti("gets/strcpy/sprintf write past buffers; strncpy may not terminate");
    std::cout << "    (see the UB/BUG comments in the source — not runnable safely)\n";

    good("size-carrying copy that always terminates and reports truncation:");
    char small[8];
    std::size_t needed = bounded_copy(small, sizeof small, "configuration");
    std::cout << "    copied into char[8]: \"" << small << "\"";
    if (needed >= sizeof small) {
        std::cout << "  TRUNCATED (needed " << needed + 1 << " bytes) — detected, not silent\n";
    }

    good("C++ — std::string removes the category of bug:");
    std::string s = "configuration";
    s += " loaded";
    std::cout << "    \"" << s << "\" (" << s.size() << " chars, grows as needed)\n";

    good("non-owning views: pass (pointer, length), never assume termination:");
    // C++17 std::string_view is this idea standardized; the C equivalent
    // is a struct { const char* data; size_t len; } pair.
    std::string_view view(s.data(), 13);
    std::cout << "    first 13 chars viewed without copying: \"" << view << "\"\n";
}

/* ===========================================================================
 * CHAPTER 6 — UNDEFINED BEHAVIOR
 * ===========================================================================
 * UB is not "it crashes": it is "the compiler may assume this never
 * happens" — and optimize accordingly. The bestiary lives in the
 * documentation; here we run the DEFENSIVE IDIOMS that keep code out of
 * it. Every `// UB:` line below is real and must never be executed.
 */

/*
 * safe_add — the overflow pre-check pattern
 * -----------------------------------------
 * // UB: int sum = a + b;   when it overflows INT_MAX (signed overflow)
 * Check against the LIMITS first, using only arithmetic that cannot
 * itself overflow.
 */
static bool safe_add(int a, int b, int* out)
{
    if (b > 0 && a > INT_MAX - b) return false; // would overflow up
    if (b < 0 && a < INT_MIN - b) return false; // would overflow down
    *out = a + b;
    return true;
}

/*
 * float_bits — type punning done right
 * ------------------------------------
 * // UB: uint32_t bits = *(uint32_t*)&f;   breaks strict aliasing
 * memcpy is the blessed way to reinterpret bytes; compilers turn it into
 * the same single instruction, with defined semantics.
 */
static std::uint32_t float_bits(float f)
{
    std::uint32_t bits;
    std::memcpy(&bits, &f, sizeof bits);
    return bits;
}

static void demo_undefined_behavior()
{
    anti("the bestiary (in comments + docs): signed overflow, dangling");
    std::cout << "    pointers, uninitialized reads, out-of-bounds, i = i++ ...\n";

    good("signed overflow — pre-check instead of 'add and hope':");
    int sum = 0;
    std::cout << "    safe_add(INT_MAX, 1)  -> " << (safe_add(INT_MAX, 1, &sum) ? "ok" : "refused")
              << " (the UB never executes)\n";
    std::cout << "    safe_add(2, 3)        -> " << (safe_add(2, 3, &sum) ? "ok" : "refused")
              << ", sum = " << sum << "\n";

    good("unsigned wraparound is DEFINED — use unsigned for modular math:");
    unsigned u = UINT_MAX;
    u = u + 1u;
    std::cout << "    UINT_MAX + 1 == " << u << " by definition (great for tick counters)\n";

    good("type punning via memcpy (strict-aliasing safe):");
    std::cout << "    bits of 1.5f = 0x" << std::hex << float_bits(1.5f) << std::dec << "\n";

    good("initialize everything — value-init makes it a habit:");
    int counters[4] = {}; // all zero, guaranteed
    std::cout << "    int counters[4] = {} -> " << counters[0] << counters[1]
              << counters[2] << counters[3] << " (no garbage reads possible)\n";

    good("tooling is part of the pattern: -Wall -Wextra, ASan/UBSan in CI\n"
         "      (this project's Makefile has a `make sanitize` target)");
}

/* ===========================================================================
 * CHAPTER 7 — FUNCTION SHAPE
 * ===========================================================================
 * Anti-patterns: arrow code (nesting that marches to the right margin),
 * god functions, and six-positional-parameter signatures. Patterns:
 * guard clauses, decomposition, parameter structs (Chapter 3), and
 * table-driven logic that replaces branching with data.
 */

/* Arrow code, reproduced: the success path is buried four levels deep,
 * and each else-branch is a mile from its if. */
static const char* validate_order_arrow(bool has_items, bool in_stock, bool paid, bool address_ok)
{
    if (has_items) {
        if (in_stock) {
            if (paid) {
                if (address_ok) {
                    return "accepted";
                } else {
                    return "bad address";
                }
            } else {
                return "unpaid";
            }
        } else {
            return "out of stock";
        }
    } else {
        return "empty order";
    }
}

/* Guard clauses: reject early, one condition per line, success path flat
 * at the bottom. Identical behavior — compare the shapes. */
static const char* validate_order_guards(bool has_items, bool in_stock, bool paid, bool address_ok)
{
    if (!has_items)  return "empty order";
    if (!in_stock)   return "out of stock";
    if (!paid)       return "unpaid";
    if (!address_ok) return "bad address";
    return "accepted";
}

/* Table-driven logic: a switch/else-if ladder replaced by data. Adding an
 * entry is a table row, not a new branch to test. */
struct HttpStatusRow {
    int code;
    const char* meaning;
};

static const HttpStatusRow HTTP_TABLE[] = {
    { 200, "OK" },
    { 301, "Moved Permanently" },
    { 404, "Not Found" },
    { 418, "I'm a teapot" },
    { 503, "Service Unavailable" },
};

static const char* http_meaning(int code)
{
    for (const auto& row : HTTP_TABLE) {
        if (row.code == code) return row.meaning;
    }
    return "Unknown";
}

static void demo_function_shape()
{
    anti("arrow code — the success path is 4 levels deep:");
    std::cout << "    validate(items, stock, paid, !address) = "
              << validate_order_arrow(true, true, true, false) << "\n";

    good("guard clauses — same logic, flat shape:");
    std::cout << "    validate(items, stock, paid, !address) = "
              << validate_order_guards(true, true, true, false) << "\n";
    std::cout << "    validate(all good)                     = "
              << validate_order_guards(true, true, true, true) << "\n";

    good("table-driven logic — branches become data rows:");
    for (int code : { 200, 418, 999 }) {
        std::cout << "    " << code << " -> " << http_meaning(code) << "\n";
    }
}

/* ===========================================================================
 * CHAPTER 8 — THE PREPROCESSOR
 * ===========================================================================
 * Anti-patterns: function-like macros that evaluate arguments twice, and
 * #define constants with no type and no scope. Patterns: constexpr and
 * inline functions do the same job with real semantics. And one macro
 * technique that remains legitimately unbeatable: the X-macro.
 */

/* The double-evaluation trap. TWICE() uses its argument two times; pass
 * a call with side effects and the side effect happens twice. */
#define TWICE(x) ((x) + (x))

static int g_rng_calls = 0;

static int fake_random()
{
    ++g_rng_calls;
    return 4; // chosen by fair dice roll
}

/* The pattern: an inline function evaluates its argument exactly once,
 * has a type, obeys scope, and shows up in the debugger. */
static inline int twice_fn(int x)
{
    return x + x;
}

/* constexpr: computed at compile time when possible — everything a
 * #define constant offers, plus a type and a namespace. */
constexpr int MAX_CLIENTS = 64;
constexpr int half_of(int n) { return n / 2; }

/*
 * The X-MACRO — the preprocessor earning its keep. Define the list once;
 * expand it multiple ways. Enum and name table can never drift apart,
 * which is exactly the bug hand-maintained parallel lists always grow.
 */
#define PACKET_KINDS(X) \
    X(Handshake)        \
    X(Data)             \
    X(Ack)              \
    X(Goodbye)

enum class PacketKind {
#define AS_ENUM(name) name,
    PACKET_KINDS(AS_ENUM)
#undef AS_ENUM
};

static const char* const PACKET_NAMES[] = {
#define AS_STRING(name) #name,
    PACKET_KINDS(AS_STRING)
#undef AS_STRING
};

static void demo_preprocessor()
{
    anti("function-like macro evaluates its argument twice:");
    g_rng_calls = 0;
    int m = TWICE(fake_random());
    std::cout << "    TWICE(fake_random()) = " << m << ", rng called "
              << g_rng_calls << " times (expected 1!)\n";

    good("inline function evaluates exactly once:");
    g_rng_calls = 0;
    int f = twice_fn(fake_random());
    std::cout << "    twice_fn(fake_random()) = " << f << ", rng called "
              << g_rng_calls << " time\n";

    good("constexpr replaces #define constants (typed, scoped, debuggable):");
    std::cout << "    MAX_CLIENTS = " << MAX_CLIENTS
              << ", half_of(MAX_CLIENTS) = " << half_of(MAX_CLIENTS)
              << " (computed at compile time)\n";

    good("the legitimate X-macro — enum and names from ONE list:");
    for (int i = 0; i < 4; ++i) {
        std::cout << "    PacketKind " << i << " = " << PACKET_NAMES[i] << "\n";
    }
}

/* ===========================================================================
 * CHAPTER 9 — INHERITANCE ABUSE
 * ===========================================================================
 * Anti-patterns: inheriting to REUSE code (rather than to satisfy an
 * interface), deep hierarchies that fossilize designs, and object slicing
 * — the silent amputation of a derived object assigned by value.
 * Pattern: small pure interfaces + composition ("has-a strategy" instead
 * of "is-a everything").
 */

struct Animal {
    virtual ~Animal() = default;
    virtual std::string speak() const { return "..."; }
};

struct Dog : Animal {
    std::string speak() const override { return "woof!"; }
};

/* Composition: behavior is a component, swappable at runtime — where a
 * hierarchy would need RobotDuck, RubberDuck, RobotRubberDuck... */
struct QuackBehavior {
    virtual ~QuackBehavior() = default;
    virtual std::string quack() const = 0;
};

struct NormalQuack : QuackBehavior {
    std::string quack() const override { return "quack"; }
};

struct RobotBeep : QuackBehavior {
    std::string quack() const override { return "BEEP-BEEP"; }
};

struct Duck {
    const QuackBehavior* voice; // has-a behavior, not is-a subtype
    std::string make_sound() const { return voice->quack(); }
};

static void demo_inheritance_abuse()
{
    Dog rex;

    anti("object slicing — assigning by value amputates the derived part:");
    Animal sliced = rex; // copies ONLY the Animal part; Dog-ness gone
    std::cout << "    Animal a = dog; a.speak() -> \"" << sliced.speak()
              << "\"  (the dog is gone)\n";

    good("polymorphism needs a reference or pointer:");
    const Animal& ref = rex;
    std::cout << "    const Animal& r = dog; r.speak() -> \"" << ref.speak() << "\"\n";
    // Related trap (// UB: if ~Animal weren't virtual):
    // deleting a Dog through Animal* without a virtual destructor.
    std::cout << "    (base classes meant for deletion get virtual destructors)\n";

    good("composition over inheritance — behavior as a swappable part:");
    NormalQuack normal;
    RobotBeep robot;
    Duck duck{ &normal };
    std::cout << "    duck says \"" << duck.make_sound() << "\"";
    duck.voice = &robot; // swapped at runtime; no new subclass invented
    std::cout << ", after upgrade: \"" << duck.make_sound() << "\"\n";
}

/* ===========================================================================
 * CHAPTER 10 — COPIES & OWNERSHIP
 * ===========================================================================
 * Anti-patterns: a class that owns memory but keeps the compiler-written
 * copy operations (shallow copy -> double free), and copies made by
 * accident because nobody measured. Patterns: the RULE OF ZERO first
 * (own resources via members that manage themselves), rule of five when
 * you truly manage a resource, and instrumentation to SEE the copies.
 */

/*
 * The shallow-copy disaster, SIMULATED. FakeAllocator logs alloc/free by
 * id instead of touching real memory, so the double free is printed, not
 * executed. A real `T* p` member + default copy ctor + real free() is
 * exactly this with a crash at the end.
 */
namespace fake_heap {
static int g_double_frees = 0;
static std::vector<int> g_freed;

static void reset()
{
    g_double_frees = 0;
    g_freed.clear();
}

static void free_block(int id)
{
    if (std::find(g_freed.begin(), g_freed.end(), id) != g_freed.end()) {
        ++g_double_frees;
        std::cout << "    [heap] DOUBLE FREE of block " << id << "!\n";
        return;
    }
    g_freed.push_back(id);
    std::cout << "    [heap] freed block " << id << "\n";
}
}

struct ShallowBuffer {                 // ANTI-PATTERN: owns block_id but
    int block_id;                      // uses the default (shallow) copy
    ~ShallowBuffer() { fake_heap::free_block(block_id); }
};

/* Rule of zero: own resources through members that already know how to
 * copy/move/destroy themselves. No destructor, no copy ctor, no bugs. */
struct Document {
    std::string title;
    std::vector<int> pages;
    // zero special member functions written — all six are correct
};

/* Instrumentation: a type that counts its own copies and moves, to make
 * invisible costs visible. */
struct Tracked {
    static int copies;
    static int moves;
    Tracked() = default;
    Tracked(const Tracked&) { ++copies; }
    Tracked(Tracked&&) noexcept { ++moves; }
    Tracked& operator=(const Tracked&) { ++copies; return *this; }
    Tracked& operator=(Tracked&&) noexcept { ++moves; return *this; }
};

int Tracked::copies = 0;
int Tracked::moves = 0;

static void take_by_value(Tracked t) { (void)t; }
static void take_by_ref(const Tracked& t) { (void)t; }

static void demo_copies_and_ownership()
{
    anti("shallow copy of an owning type — two owners, one block:");
    fake_heap::reset();
    {
        ShallowBuffer a{ 42 };
        ShallowBuffer b = a; // default copy: BOTH now own block 42
        (void)b;
    } // both destructors run...
    std::cout << "    => simulated double frees: " << fake_heap::g_double_frees
              << " (with real memory: heap corruption)\n";

    good("rule of zero — members manage themselves, copies just work:");
    Document d1{ "draft", { 1, 2, 3 } };
    Document d2 = d1; // deep, correct, automatic
    d2.pages.push_back(4);
    std::cout << "    original " << d1.pages.size() << " pages, copy "
              << d2.pages.size() << " pages — independent, no code written\n";

    good("measure copies instead of guessing:");
    Tracked::copies = Tracked::moves = 0;
    Tracked t;
    take_by_value(t);
    std::cout << "    pass by value:      " << Tracked::copies << " copy\n";
    Tracked::copies = Tracked::moves = 0;
    take_by_ref(t);
    std::cout << "    pass by const ref:  " << Tracked::copies << " copies\n";
    Tracked::copies = Tracked::moves = 0;
    std::vector<Tracked> v;
    v.reserve(2);
    v.push_back(std::move(t));
    std::cout << "    push_back(move):    " << Tracked::copies << " copies, "
              << Tracked::moves << " move (noexcept move ctor pays off)\n";
}

/* ===========================================================================
 * CHAPTER 11 — CONCURRENCY
 * ===========================================================================
 * The classic anti-pattern is unsynchronized access to shared data — but
 * a true data race is UB and cannot be demonstrated legally. What CAN be
 * run is its well-defined cousin: separate atomic load and store, which
 * is race-free yet still LOSES UPDATES, because check-then-act is not
 * atomic. It reproduces the bug's arithmetic without the UB.
 */

static void demo_concurrency()
{
    const int THREADS = 4;
    const int INCREMENTS = 25000;

    anti("read-modify-write hole — atomic load + store still loses updates:");
    {
        std::atomic<int> counter{ 0 };
        std::vector<std::thread> pool;
        for (int t = 0; t < THREADS; ++t) {
            pool.emplace_back([&counter] {
                for (int i = 0; i < INCREMENTS; ++i) {
                    counter.store(counter.load() + 1); // load...gap...store
                }
            });
        }
        for (auto& th : pool) th.join();
        std::cout << "    expected " << THREADS * INCREMENTS << ", got "
                  << counter.load() << " (updates lost in the gap; a plain\n"
                  << "    int would be the same bug PLUS undefined behavior)\n";
    }

    good("atomic read-modify-write — one indivisible operation:");
    {
        std::atomic<int> counter{ 0 };
        std::vector<std::thread> pool;
        for (int t = 0; t < THREADS; ++t) {
            pool.emplace_back([&counter] {
                for (int i = 0; i < INCREMENTS; ++i) {
                    counter.fetch_add(1);
                }
            });
        }
        for (auto& th : pool) th.join();
        std::cout << "    fetch_add: got " << counter.load() << " — exact\n";
    }

    good("mutex + lock_guard for anything bigger than one variable:");
    {
        std::mutex m;
        long total = 0;
        std::vector<std::thread> pool;
        for (int t = 0; t < THREADS; ++t) {
            pool.emplace_back([&m, &total] {
                for (int i = 0; i < INCREMENTS; ++i) {
                    std::lock_guard<std::mutex> lock(m); // RAII: no forgotten unlock
                    total += 1;
                }
            });
        }
        for (auto& th : pool) th.join();
        std::cout << "    mutex-guarded total: " << total
                  << " (lock_guard is Chapter 1's RAII applied to locks)\n";
    }
}

/* ===========================================================================
 * CHAPTER 12 — DESIGN FOR CHANGE
 * ===========================================================================
 * Anti-patterns: functions that secretly call the clock (untestable), and
 * "premature pessimization" — casually writing the slow version when the
 * fast one costs nothing (the flip side of the premature-optimization
 * warning). Patterns: inject dependencies, reserve capacity, let const
 * document the contract.
 */

/* Hidden dependency: calls the real clock internally. Every test of this
 * function is a test of the wall clock too — it cannot be pinned down. */
static bool session_expired_bad(long started_ms)
{
    long now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count();
    return now - started_ms > 30 * 60 * 1000;
}

/* The pattern: time is a PARAMETER. Production passes the real clock;
 * tests pass any moment they want. Determinism restored. */
static bool session_expired_good(long started_ms, long now_ms)
{
    return now_ms - started_ms > 30 * 60 * 1000;
}

static void demo_design_for_change()
{
    anti("hidden clock — the function's result depends on when tests run:");
    std::cout << "    session_expired_bad(0) = " << session_expired_bad(0)
              << " (true today; what does the unit test assert?)\n";

    good("inject time — deterministic in production AND tests:");
    long start = 1000;
    std::cout << "    expired after 29 min? " << session_expired_good(start, start + 29 * 60 * 1000)
              << "   after 31 min? " << session_expired_good(start, start + 31 * 60 * 1000) << "\n";

    good("premature pessimization — reserve() costs one line:");
    const int N = 200000;
    using clk = std::chrono::steady_clock;

    auto t0 = clk::now();
    std::vector<int> grow;
    for (int i = 0; i < N; ++i) grow.push_back(i);
    auto grow_us = std::chrono::duration_cast<std::chrono::microseconds>(clk::now() - t0).count();

    auto t1 = clk::now();
    std::vector<int> reserved;
    reserved.reserve(N);
    for (int i = 0; i < N; ++i) reserved.push_back(i);
    auto reserved_us =
        std::chrono::duration_cast<std::chrono::microseconds>(clk::now() - t1).count();

    std::cout << "    push_back " << N << " ints: no reserve " << grow_us
              << " us, with reserve " << reserved_us << " us\n";
    std::cout << "    (numbers vary per run; the reallocations do not)\n";

    good("const is API documentation the compiler enforces:");
    const std::vector<int>& readonly = reserved;
    std::cout << "    a const& parameter promises \"I only read\" — and the\n"
              << "    compiler holds the function to it (size=" << readonly.size() << ")\n";
}

/* ===========================================================================
 * MAIN — runs every chapter in order
 * ===========================================================================
 */

int main()
{
    std::cout << "patterns & anti-patterns in C and C++ — every ANTI line is\n"
                 "run (or safely simulated) next to the GOOD line that fixes it\n";

    chapter("1. Ownership & cleanup");
    demo_ownership_and_cleanup();

    chapter("2. Error handling");
    demo_error_handling();

    chapter("3. Magic values");
    demo_magic_values();

    chapter("4. Global state");
    demo_global_state();

    chapter("5. Buffers & strings");
    demo_buffers_and_strings();

    chapter("6. Undefined behavior");
    demo_undefined_behavior();

    chapter("7. Function shape");
    demo_function_shape();

    chapter("8. The preprocessor");
    demo_preprocessor();

    chapter("9. Inheritance abuse");
    demo_inheritance_abuse();

    chapter("10. Copies & ownership");
    demo_copies_and_ownership();

    chapter("11. Concurrency");
    demo_concurrency();

    chapter("12. Design for change");
    demo_design_for_change();

    std::cout << "\nAll chapters completed successfully.\n";
    return EXIT_SUCCESS;
}
