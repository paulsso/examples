/*
 * ============================================================================
 *  main.cpp — Game Engine Development in C++: From Beginner to Advanced
 * ============================================================================
 *
 *  This file is the backbone of a course on modern C++ (C++17) aimed at one
 *  concrete goal: building a fast, cross-platform 2D game engine from
 *  scratch. Every chapter implements a real engine subsystem, in the order
 *  a student would need them, and every function is documented here and in
 *  DOCUMENTATION.md.
 *
 *  Chapters:
 *    1.  From C to C++        — references, overloading, namespaces, auto
 *    2.  Classes & RAII       — resource handles that free themselves
 *    3.  Game Math            — Vec2, operator overloading, 2D transforms
 *    4.  Templates & STL      — generic Grid, containers, algorithms
 *    5.  Polymorphism         — virtual dispatch, interfaces, entities
 *    6.  Move Semantics       — rule of five, smart pointers, ownership
 *    7.  Memory & Performance — arena allocator, object pool, AoS vs SoA
 *    8.  Entity Component System — the core architecture of modern engines
 *    9.  Game Loop & Timing   — clocks, fixed timestep, interpolation
 *    10. Events & Input       — event bus, observer pattern, input mapping
 *    11. Collision & Physics  — AABB/circle tests, integration, bouncing
 *    12. Renderer Abstraction — platform-independent drawing + console
 *                               backend, sprites and animation
 *    13. Pathfinding & AI     — A* on a grid, finite state machines
 *    14. Concurrency          — a job system (thread pool), atomics
 *    15. Mini Game            — everything combined into a playable sim
 *
 *  Build:  make          (produces the `main` binary)
 *  Run:    ./main        (or `make run`)
 * ============================================================================
 */

#include <algorithm>            /* std::sort, std::find_if, std::min/max    */
#include <array>                /* std::array — fixed-size, stack-friendly  */
#include <atomic>               /* std::atomic for lock-free counters       */
#include <chrono>               /* std::chrono — the engine's clock         */
#include <cmath>                /* std::sqrt, std::sin, std::cos            */
#include <condition_variable>   /* worker-thread sleeping                   */
#include <cstdint>              /* std::uint32_t and friends                */
#include <cstddef>              /* std::size_t                              */
#include <functional>           /* std::function — type-erased callbacks    */
#include <iomanip>              /* std::setprecision, std::setw             */
#include <iostream>             /* std::cout                                */
#include <memory>               /* std::unique_ptr, std::shared_ptr         */
#include <mutex>                /* std::mutex, std::lock_guard              */
#include <numeric>              /* std::accumulate                          */
#include <queue>                /* std::queue, std::priority_queue          */
#include <random>               /* std::mt19937 — seedable, portable RNG    */
#include <string>               /* std::string                              */
#include <thread>               /* std::thread                              */
#include <tuple>                /* std::tuple for the A* open set           */
#include <unordered_map>        /* hash maps for components & assets        */
#include <vector>               /* the workhorse container                  */

/*
 * chapter
 * -------
 * Prints a banner separating each demo's output, exactly like the C course.
 * In C++ we prefer a small function over a macro: it is type-checked,
 * debuggable, and scoped.
 */
static void chapter(const std::string& title)
{
    std::cout << "\n=============================================\n"
              << " " << title << "\n"
              << "=============================================\n";
}

/* ===========================================================================
 * CHAPTER 1 — FROM C TO C++
 * ===========================================================================
 * C++ keeps everything from the C course and adds tools that make large
 * engine codebases manageable. This chapter shows the everyday differences.
 */

/*
 * apply_damage
 * ------------
 * Takes an int by REFERENCE (int&). A reference is an alias for the
 * caller's variable — like a pointer that cannot be null and needs no
 * dereference syntax. This replaces the C idiom of passing addresses.
 */
static void apply_damage(int& hp, int amount)
{
    hp -= amount;
}

/*
 * describe (overload set)
 * -----------------------
 * C++ allows several functions with the same name but different parameter
 * types; the compiler picks the right one at each call site. In C these
 * would need three different names.
 */
static void describe(int value)
{
    std::cout << "  int: " << value << "\n";
}

static void describe(float value)
{
    std::cout << "  float: " << value << "\n";
}

static void describe(const std::string& value)
{
    std::cout << "  string: \"" << value << "\"\n";
}

/*
 * spawn_enemy
 * -----------
 * Default arguments: callers may omit trailing parameters. Handy for
 * engine APIs with sensible defaults (spawn at full health, level 1).
 */
static void spawn_enemy(const std::string& name, int hp = 100, int level = 1)
{
    std::cout << "  spawned " << name << " (hp " << hp
              << ", level " << level << ")\n";
}

/*
 * namespace units
 * ---------------
 * Namespaces prevent name collisions between engine modules and third-
 * party code — essential once a project has more than a handful of files.
 * constexpr values are computed at compile time.
 */
namespace units {
constexpr float PIXELS_PER_METER = 32.0f;
constexpr float GRAVITY = 9.81f;
}

static void demo_cpp_basics(void)
{
    /* References replace out-pointers. */
    int hp = 100;
    apply_damage(hp, 35);
    std::cout << "after apply_damage(hp, 35): hp = " << hp << "\n";

    /* Overloading: one name, three functions. */
    describe(42);
    describe(3.5f);
    describe(std::string("goblin"));

    /* Default arguments. */
    spawn_enemy("slime");
    spawn_enemy("orc", 250);
    spawn_enemy("dragon", 5000, 20);

    /* Namespaces + constexpr. */
    std::cout << "3 meters = " << 3.0f * units::PIXELS_PER_METER
              << " pixels (units::PIXELS_PER_METER)\n";

    /* auto deduces types; range-for iterates without index bookkeeping.
     * std::string manages its own memory — no strlen/strcpy/free. */
    std::vector<std::string> loot = { "sword", "potion", "gem" };
    auto total = loot.size();
    std::cout << "loot (" << total << " items): ";
    for (const auto& item : loot) {
        std::cout << item << " ";
    }
    std::cout << "\n";
}

/* ===========================================================================
 * CHAPTER 2 — CLASSES & RAII
 * ===========================================================================
 * RAII (Resource Acquisition Is Initialization) is THE core C++ idiom:
 * a resource is acquired in a constructor and released in the destructor,
 * so cleanup happens automatically and exactly once, even on early return.
 * Game engines live and die by this — textures, sounds, GPU buffers and
 * file handles all become classes that clean up after themselves.
 */

/*
 * class TextureHandle
 * -------------------
 * Simulates owning a GPU texture. The constructor "uploads" it, the
 * destructor "frees" it — watch the log output to see destruction happen
 * automatically at the closing brace of each scope.
 *
 * Copying is DELETED: two handles freeing the same GPU texture would be
 * a double-free, so the type forbids it at compile time.
 */
class TextureHandle {
public:
    explicit TextureHandle(std::string name)
        : name_(std::move(name)), id_(next_id_++)
    {
        std::cout << "  [gpu] uploaded texture '" << name_
                  << "' (id " << id_ << ")\n";
    }

    ~TextureHandle()
    {
        std::cout << "  [gpu] freed texture '" << name_
                  << "' (id " << id_ << ")\n";
    }

    TextureHandle(const TextureHandle&) = delete;
    TextureHandle& operator=(const TextureHandle&) = delete;

    const std::string& name() const { return name_; }
    int id() const { return id_; }

private:
    std::string name_;
    int id_;
    static int next_id_;
};

int TextureHandle::next_id_ = 1;

/*
 * class Player
 * ------------
 * A plain game class showing encapsulation: state is private and can only
 * change through methods that enforce the rules (hp never goes below 0 or
 * above max). `const` methods promise not to modify the object, so they
 * can be called on const references — the compiler enforces the promise.
 */
class Player {
public:
    explicit Player(std::string name, int max_hp)
        : name_(std::move(name)), hp_(max_hp), max_hp_(max_hp) {}

    void take_damage(int amount)
    {
        hp_ = std::max(0, hp_ - amount);
    }

    void heal(int amount)
    {
        hp_ = std::min(max_hp_, hp_ + amount);
    }

    bool is_alive() const { return hp_ > 0; }
    int hp() const { return hp_; }
    const std::string& name() const { return name_; }

private:
    std::string name_;
    int hp_;
    int max_hp_;
};

static void demo_raii(void)
{
    std::cout << "entering outer scope\n";
    {
        TextureHandle grass("grass.png");
        {
            TextureHandle hero("hero.png");
            std::cout << "  using " << hero.name() << " and "
                      << grass.name() << "\n";
        }   /* hero's destructor runs HERE, automatically */
        std::cout << "  inner scope closed\n";
    }       /* grass's destructor runs here — reverse order of construction */
    std::cout << "outer scope closed — both textures freed, no manual code\n";

    Player hero("Ari", 100);
    hero.take_damage(130);          /* clamped at 0, cannot go negative */
    std::cout << hero.name() << ": hp " << hero.hp()
              << ", alive: " << (hero.is_alive() ? "yes" : "no") << "\n";
    hero.heal(60);
    std::cout << hero.name() << " healed to " << hero.hp() << " hp\n";
}

/* ===========================================================================
 * CHAPTER 3 — GAME MATH
 * ===========================================================================
 * Every 2D engine is built on a small math library: 2D vectors for
 * positions and velocities, and 3x3 matrices for transforms. Operator
 * overloading lets the math read like math.
 */

/*
 * struct Vec2
 * -----------
 * A 2D vector. Default member initializers make Vec2{} the zero vector.
 * Kept as a plain struct with public members: math types are "value
 * types" — there is no invariant to protect, so getters would only add
 * noise.
 */
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    /* length: magnitude of the vector, sqrt(x^2 + y^2). */
    float length() const { return std::sqrt(x * x + y * y); }

    /* normalized: same direction, length 1. The zero vector has no
     * direction, so it is returned unchanged (guarding the div by zero). */
    Vec2 normalized() const
    {
        float len = length();
        if (len == 0.0f) return {};
        return { x / len, y / len };
    }
};

/* Arithmetic operators as free functions, so both operands convert
 * symmetrically. These make `pos += vel * dt` legal and readable. */
static Vec2 operator+(Vec2 a, Vec2 b) { return { a.x + b.x, a.y + b.y }; }
static Vec2 operator-(Vec2 a, Vec2 b) { return { a.x - b.x, a.y - b.y }; }
static Vec2 operator*(Vec2 v, float s) { return { v.x * s, v.y * s }; }
static Vec2 operator*(float s, Vec2 v) { return v * s; }
static Vec2& operator+=(Vec2& a, Vec2 b) { a.x += b.x; a.y += b.y; return a; }

/* operator<< lets std::cout print vectors — invaluable for debugging. */
static std::ostream& operator<<(std::ostream& os, Vec2 v)
{
    return os << "(" << v.x << ", " << v.y << ")";
}

/*
 * dot
 * ---
 * The dot product: |a||b|cos(angle). Positive when vectors point the
 * same way — used constantly for "is the enemy in front of me?" checks,
 * lighting, and projections.
 */
static float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }

/*
 * cross2d
 * -------
 * The 2D cross product (z of the 3D cross). Its SIGN tells you whether
 * b is clockwise or counter-clockwise from a — the basis of turning
 * decisions and winding tests.
 */
static float cross2d(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }

/*
 * lerp
 * ----
 * Linear interpolation: a at t=0, b at t=1, the blend between. The
 * single most-used function in game programming: camera smoothing,
 * animation blending, color fades, render interpolation (Chapter 9).
 */
static Vec2 lerp(Vec2 a, Vec2 b, float t)
{
    return a + (b - a) * t;
}

/*
 * clampf
 * ------
 * Constrains a value to [lo, hi]. Keeps players inside the world,
 * health inside its bar, and volume inside 0..1.
 */
static float clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(v, hi));
}

/*
 * deg_to_rad
 * ----------
 * Trig functions take radians; designers think in degrees. Convert once
 * at the boundary.
 */
static float deg_to_rad(float degrees)
{
    constexpr float PI = 3.14159265358979f;
    return degrees * PI / 180.0f;
}

/*
 * struct Mat3
 * -----------
 * A 3x3 matrix for 2D transforms. Using 3x3 (not 2x2) lets translation
 * be a matrix too (homogeneous coordinates: points carry a hidden 1),
 * so translate/rotate/scale all compose with one multiply — exactly how
 * every real engine builds its transform hierarchy.
 */
struct Mat3 {
    float m[3][3] = { {1, 0, 0}, {0, 1, 0}, {0, 0, 1} };   /* identity */

    /* Named constructors for the three fundamental transforms. */
    static Mat3 translation(Vec2 t)
    {
        Mat3 r;
        r.m[0][2] = t.x;
        r.m[1][2] = t.y;
        return r;
    }

    static Mat3 rotation(float radians)
    {
        Mat3 r;
        float c = std::cos(radians), s = std::sin(radians);
        r.m[0][0] = c; r.m[0][1] = -s;
        r.m[1][0] = s; r.m[1][1] = c;
        return r;
    }

    static Mat3 scale(Vec2 s)
    {
        Mat3 r;
        r.m[0][0] = s.x;
        r.m[1][1] = s.y;
        return r;
    }

    /* Matrix multiplication composes transforms. ORDER MATTERS:
     * (T * R) applies rotation first, then translation. */
    Mat3 operator*(const Mat3& o) const
    {
        Mat3 r;
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                float sum = 0.0f;
                for (int k = 0; k < 3; k++) {
                    sum += m[row][k] * o.m[k][col];
                }
                r.m[row][col] = sum;
            }
        }
        return r;
    }

    /* transform_point: multiply (x, y, 1) by the matrix. The trailing 1
     * is what makes translation work. */
    Vec2 transform_point(Vec2 p) const
    {
        return {
            m[0][0] * p.x + m[0][1] * p.y + m[0][2],
            m[1][0] * p.x + m[1][1] * p.y + m[1][2],
        };
    }
};

static void demo_game_math(void)
{
    std::cout << std::fixed << std::setprecision(2);

    Vec2 pos{ 10.0f, 5.0f };
    Vec2 vel{ 3.0f, -1.0f };
    float dt = 0.5f;

    pos += vel * dt;                     /* reads exactly like the physics */
    std::cout << "pos += vel * dt      -> " << pos << "\n";

    Vec2 dir{ 3.0f, 4.0f };
    std::cout << "length of " << dir << "   = " << dir.length() << "\n";
    std::cout << "normalized           = " << dir.normalized() << "\n";

    Vec2 facing{ 1.0f, 0.0f };
    Vec2 to_enemy{ 0.7f, 0.7f };
    std::cout << "dot(facing, to_enemy) = " << dot(facing, to_enemy)
              << " (> 0: enemy is in front)\n";
    std::cout << "cross2d(facing, to_enemy) = " << cross2d(facing, to_enemy)
              << " (> 0: enemy is to the left)\n";

    std::cout << "lerp((0,0),(10,20), 0.25) = "
              << lerp({ 0, 0 }, { 10, 20 }, 0.25f) << "\n";
    std::cout << "clampf(15, 0, 10) = " << clampf(15, 0, 10) << "\n";
    std::cout << "2 * dir = " << 2.0f * dir
              << " (scalar * vector works too)\n";

    /* Compose: scale, then rotate 90 degrees, then translate. */
    Mat3 world = Mat3::translation({ 100, 50 })
               * Mat3::rotation(deg_to_rad(90.0f))
               * Mat3::scale({ 2, 2 });
    Vec2 local{ 1.0f, 0.0f };
    std::cout << "local point " << local << " -> world "
              << world.transform_point(local)
              << "  (scaled x2, rotated 90deg, moved to (100,50))\n";

    std::cout << std::defaultfloat << std::setprecision(6);
}

/* ===========================================================================
 * CHAPTER 4 — TEMPLATES & THE STL
 * ===========================================================================
 * Templates generate type-safe code for any type at compile time — zero
 * runtime cost, unlike void* generics in C. The STL (vector, unordered_map,
 * algorithms) is built on them and replaces most hand-rolled containers.
 */

/*
 * my_clamp<T>
 * -----------
 * A function template: one definition, works for int, float, double...
 * The compiler stamps out a concrete version per type used. (std::clamp
 * exists since C++17; re-implementing it shows how simple templates are.)
 */
template <typename T>
static T my_clamp(T value, T lo, T hi)
{
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

/*
 * class template Grid<T>
 * ----------------------
 * A 2D grid stored as ONE flat vector — better cache behavior than a
 * vector of vectors, and a single allocation. Element (x, y) lives at
 * index y * width + x (row-major, same as the C course's matrices).
 * Reused later by the tilemap, A* pathfinding, and the mini game.
 */
template <typename T>
class Grid {
public:
    Grid(int width, int height, T fill = T{})
        : width_(width), height_(height),
          cells_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), fill)
    {
    }

    T& at(int x, int y)
    {
        return cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_)
                      + static_cast<std::size_t>(x)];
    }

    const T& at(int x, int y) const
    {
        return cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_)
                      + static_cast<std::size_t>(x)];
    }

    bool in_bounds(int x, int y) const
    {
        return x >= 0 && x < width_ && y >= 0 && y < height_;
    }

    int width() const { return width_; }
    int height() const { return height_; }

private:
    int width_;
    int height_;
    std::vector<T> cells_;
};

static void demo_templates_and_stl(void)
{
    /* One template, three instantiations. */
    std::cout << "my_clamp(15, 0, 10)       = " << my_clamp(15, 0, 10) << "\n";
    std::cout << "my_clamp(3.7f, 0.f, 1.f)  = " << my_clamp(3.7f, 0.0f, 1.0f) << "\n";

    /* Grid<T>: the tilemap of every 2D game. */
    Grid<char> tilemap(6, 3, '.');
    tilemap.at(2, 1) = '#';
    tilemap.at(3, 1) = '#';
    std::cout << "tilemap (" << tilemap.width() << "x" << tilemap.height() << "):\n";
    for (int y = 0; y < tilemap.height(); y++) {
        std::cout << "  ";
        for (int x = 0; x < tilemap.width(); x++) {
            std::cout << tilemap.at(x, y);
        }
        std::cout << "\n";
    }

    /* std::sort with a lambda: sort sprites back-to-front by layer —
     * exactly what a 2D renderer does every frame. */
    struct SpriteInfo {
        std::string name;
        int layer;
    };
    std::vector<SpriteInfo> draw_list = {
        { "ui_healthbar", 10 }, { "background", 0 },
        { "player", 5 }, { "tree", 3 },
    };
    std::sort(draw_list.begin(), draw_list.end(),
              [](const SpriteInfo& a, const SpriteInfo& b) {
                  return a.layer < b.layer;
              });
    std::cout << "draw order   : ";
    for (const auto& s : draw_list) {
        std::cout << s.name << "(" << s.layer << ") ";
    }
    std::cout << "\n";

    /* std::unordered_map: O(1) average lookup, the asset registry's core. */
    std::unordered_map<std::string, int> texture_ids = {
        { "hero.png", 1 }, { "grass.png", 2 }, { "slime.png", 3 },
    };
    std::cout << "texture_ids[\"slime.png\"] = " << texture_ids["slime.png"] << "\n";

    /* std::find_if and std::accumulate: algorithms over any container. */
    auto it = std::find_if(draw_list.begin(), draw_list.end(),
                           [](const SpriteInfo& s) { return s.layer > 4; });
    std::cout << "first sprite above layer 4: " << it->name << "\n";

    std::vector<int> damage_log = { 12, 7, 30, 5 };
    int total = std::accumulate(damage_log.begin(), damage_log.end(), 0);
    std::cout << "total damage dealt: " << total << "\n";
}

/* ===========================================================================
 * CHAPTER 5 — POLYMORPHISM & VIRTUAL DISPATCH
 * ===========================================================================
 * Virtual functions let code operate on a base-class interface while each
 * derived class supplies its own behavior — decided at RUNTIME through the
 * vtable. Engines use this for renderer backends (Chapter 12), audio
 * backends, and scripted entity behaviors.
 */

/*
 * class Enemy (abstract base)
 * ---------------------------
 * `= 0` makes attack() PURE virtual: Enemy is an interface that cannot be
 * instantiated; derived classes MUST implement it. The virtual destructor
 * is essential — deleting a derived object through a base pointer without
 * one is undefined behavior.
 */
class Enemy {
public:
    explicit Enemy(Vec2 pos) : pos_(pos) {}
    virtual ~Enemy() = default;

    virtual void attack() const = 0;            /* pure: must override */

    /* Virtual with a default: derived classes may override or inherit. */
    virtual void update(float dt)
    {
        pos_ += Vec2{ 1.0f, 0.0f } * dt;        /* default: drift right */
    }

    Vec2 position() const { return pos_; }

protected:
    Vec2 pos_;
};

/*
 * Goblin / Dragon
 * ---------------
 * Two concrete enemies. `override` asks the compiler to verify the
 * signature actually overrides something — it catches typos that would
 * otherwise silently create a NEW function. `final` on Dragon forbids
 * further derivation (and enables devirtualization optimizations).
 */
class Goblin : public Enemy {
public:
    using Enemy::Enemy;         /* inherit the constructor */

    void attack() const override
    {
        std::cout << "  Goblin stabs for 5 damage\n";
    }
};

class Dragon final : public Enemy {
public:
    using Enemy::Enemy;

    void attack() const override
    {
        std::cout << "  Dragon breathes fire for 50 damage\n";
    }

    void update(float dt) override
    {
        pos_ += Vec2{ 0.0f, -2.0f } * dt;       /* dragons fly upward */
    }
};

static void demo_polymorphism(void)
{
    Goblin goblin({ 0, 0 });
    Dragon dragon({ 10, 10 });

    /* Polymorphism works through pointers or references to the base.
     * (Storing by VALUE in an Enemy array would "slice" off the derived
     * parts — a classic bug worth demonstrating in lecture.) */
    std::array<Enemy*, 2> enemies = { &goblin, &dragon };

    std::cout << std::fixed << std::setprecision(1);
    for (Enemy* e : enemies) {
        e->attack();                    /* dispatches via the vtable */
        e->update(1.0f);                /* Goblin drifts, Dragon flies */
        std::cout << "    now at " << e->position() << "\n";
    }
    std::cout << std::defaultfloat << std::setprecision(6);

    /* dynamic_cast: safe downcast that returns nullptr on mismatch.
     * Use sparingly — needing it often signals a design smell. */
    for (Enemy* e : enemies) {
        if (auto* d = dynamic_cast<Dragon*>(e)) {
            std::cout << "  found the dragon at " << d->position().x
                      << " via dynamic_cast\n";
        }
    }
}

/* ===========================================================================
 * CHAPTER 6 — MOVE SEMANTICS & SMART POINTERS
 * ===========================================================================
 * Big resources (vertex buffers, audio clips) are expensive to copy.
 * Move semantics let ownership TRANSFER instead: the new object steals the
 * internals and the old one is left empty. Smart pointers then encode
 * ownership in the type system so `delete` disappears from engine code.
 */

/*
 * class VertexBuffer — the Rule of Five, spelled out
 * --------------------------------------------------
 * Owns a heap array of floats (stand-in for GPU vertex data). Because it
 * manages a raw resource it defines all five special members:
 * destructor, copy ctor, copy assign, move ctor, move assign.
 * Each logs, so the demo output shows exactly when copies and moves occur.
 */
class VertexBuffer {
public:
    explicit VertexBuffer(std::size_t count)
        : count_(count), data_(new float[count]())
    {
        std::cout << "  ctor: allocated " << count_ << " floats\n";
    }

    ~VertexBuffer()
    {
        if (data_ != nullptr) {
            std::cout << "  dtor: freed " << count_ << " floats\n";
        }
        delete[] data_;
    }

    /* Copy = duplicate the buffer (expensive, sometimes necessary). */
    VertexBuffer(const VertexBuffer& other)
        : count_(other.count_), data_(new float[other.count_])
    {
        std::copy(other.data_, other.data_ + count_, data_);
        std::cout << "  copy ctor: DUPLICATED " << count_ << " floats\n";
    }

    VertexBuffer& operator=(const VertexBuffer& other)
    {
        if (this != &other) {
            float* fresh = new float[other.count_];
            std::copy(other.data_, other.data_ + other.count_, fresh);
            delete[] data_;
            data_ = fresh;
            count_ = other.count_;
            std::cout << "  copy assign: DUPLICATED " << count_ << " floats\n";
        }
        return *this;
    }

    /* Move = steal the pointer, null the source. O(1) regardless of size.
     * noexcept matters: std::vector only uses moves during reallocation
     * if they cannot throw. */
    VertexBuffer(VertexBuffer&& other) noexcept
        : count_(other.count_), data_(other.data_)
    {
        other.data_ = nullptr;
        other.count_ = 0;
        std::cout << "  move ctor: STOLE the buffer (no allocation)\n";
    }

    VertexBuffer& operator=(VertexBuffer&& other) noexcept
    {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            count_ = other.count_;
            other.data_ = nullptr;
            other.count_ = 0;
            std::cout << "  move assign: STOLE the buffer\n";
        }
        return *this;
    }

    std::size_t size() const { return count_; }

private:
    std::size_t count_;
    float* data_;
};

/*
 * struct Texture / struct Sprite
 * ------------------------------
 * A tiny asset type shared by multiple sprites via shared_ptr: the asset
 * stays alive while ANY sprite uses it and is freed when the last one
 * lets go — reference counting doing asset lifetime management.
 */
struct Texture {
    std::string name;
};

struct Sprite {
    std::shared_ptr<Texture> texture;
    Vec2 position;
};

static void demo_move_and_smart_pointers(void)
{
    std::cout << "-- rule of five --\n";
    VertexBuffer original(1000);
    VertexBuffer copy = original;               /* copy ctor: expensive  */
    VertexBuffer moved = std::move(original);   /* move ctor: pointer steal */
    std::cout << "  after move: moved.size()=" << moved.size()
              << ", original is empty (valid but unspecified)\n";
    (void)copy;

    std::cout << "-- unique_ptr: exclusive ownership --\n";
    {
        auto atlas = std::make_unique<TextureHandle>("atlas.png");
        std::cout << "  unique_ptr owns '" << atlas->name() << "'\n";
        /* No delete, no leak: destruction at scope end is guaranteed. */
    }

    std::cout << "-- shared_ptr: shared asset ownership --\n";
    auto tex = std::make_shared<Texture>(Texture{ "tileset.png" });
    Sprite a{ tex, { 0, 0 } };
    Sprite b{ tex, { 5, 5 } };
    std::cout << "  '" << tex->name << "' use_count = " << tex.use_count()
              << " (tex + 2 sprites)\n";

    std::weak_ptr<Texture> watcher = tex;   /* observes without owning */
    tex.reset();
    a.texture.reset();
    b.texture.reset();
    std::cout << "  after all owners reset, weak_ptr expired: "
              << (watcher.expired() ? "yes — texture was freed" : "no") << "\n";
}

/* ===========================================================================
 * CHAPTER 7 — MEMORY & PERFORMANCE
 * ===========================================================================
 * "Fast" is a requirement, not a hope. Engines avoid general-purpose heap
 * allocation on the hot path (it's slow and fragments memory) using custom
 * allocators, and lay data out for the CPU cache, which is ~100x faster
 * than main memory.
 */

/*
 * class ArenaAllocator
 * --------------------
 * The simplest and fastest allocator: one big block, a bump pointer.
 * allocate() just advances an offset (with alignment); there is NO
 * per-object free — reset() reclaims everything at once. Perfect for
 * per-frame scratch data: allocate all frame, reset once at frame end.
 */
class ArenaAllocator {
public:
    explicit ArenaAllocator(std::size_t capacity)
        : buffer_(new unsigned char[capacity]), capacity_(capacity) {}

    ~ArenaAllocator() { delete[] buffer_; }

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    /* Round the offset up to `alignment`, hand out the slot, bump. */
    void* allocate(std::size_t size, std::size_t alignment)
    {
        std::size_t aligned = (offset_ + alignment - 1) & ~(alignment - 1);
        if (aligned + size > capacity_) {
            return nullptr;                     /* arena exhausted */
        }
        offset_ = aligned + size;
        return buffer_ + aligned;
    }

    /* Typed convenience wrapper. */
    template <typename T>
    T* alloc_array(std::size_t n)
    {
        return static_cast<T*>(allocate(n * sizeof(T), alignof(T)));
    }

    void reset() { offset_ = 0; }               /* free EVERYTHING, O(1) */
    std::size_t used() const { return offset_; }

private:
    unsigned char* buffer_;
    std::size_t capacity_;
    std::size_t offset_ = 0;
};

/*
 * class template ObjectPool<T, Capacity>
 * --------------------------------------
 * Pre-allocates a fixed block of objects and recycles them via a free
 * list, so acquire/release never touch the heap. Bullets, particles and
 * audio voices — anything spawned and destroyed constantly — live in
 * pools in real engines.
 */
template <typename T, std::size_t Capacity>
class ObjectPool {
public:
    ObjectPool()
    {
        free_.reserve(Capacity);
        /* Push indices in reverse so slot 0 is handed out first. */
        for (std::size_t i = Capacity; i-- > 0; ) {
            free_.push_back(i);
        }
    }

    /* acquire: O(1). Returns nullptr when the pool is exhausted —
     * in games, running out of bullets is handled, not fatal. */
    T* acquire()
    {
        if (free_.empty()) return nullptr;
        std::size_t index = free_.back();
        free_.pop_back();
        return &slots_[index];
    }

    /* release: O(1). Pointer arithmetic recovers the slot index. */
    void release(T* object)
    {
        std::size_t index = static_cast<std::size_t>(object - slots_.data());
        free_.push_back(index);
    }

    std::size_t available() const { return free_.size(); }

private:
    std::array<T, Capacity> slots_{};
    std::vector<std::size_t> free_;
};

/*
 * AoS vs SoA — the data-layout lesson
 * -----------------------------------
 * Array-of-Structs stores whole particles contiguously; updating only
 * position still drags every particle's unused bytes through the cache.
 * Struct-of-Arrays keeps each field contiguous, so the position update
 * touches only position bytes. Same math, measurably different speed —
 * this insight is why modern engines are built around ECS (Chapter 8).
 */
struct ParticleAoS {
    Vec2 position;
    Vec2 velocity;
    float lifetime = 0.0f;
    float size = 1.0f;
    std::uint32_t color = 0xFFFFFFFF;
    bool active = true;
};

struct ParticlesSoA {
    std::vector<Vec2> position;
    std::vector<Vec2> velocity;
    std::vector<float> lifetime;
    std::vector<float> size;
    std::vector<std::uint32_t> color;
    std::vector<bool> active;
};

static void demo_memory_and_performance(void)
{
    std::cout << "-- arena allocator --\n";
    ArenaAllocator frame_arena(1024);
    float* positions = frame_arena.alloc_array<float>(64);
    int* ids = frame_arena.alloc_array<int>(32);
    positions[0] = 1.5f;
    ids[0] = 7;
    std::cout << "  allocated 64 floats + 32 ints, arena used "
              << frame_arena.used() << " / 1024 bytes\n";
    frame_arena.reset();
    std::cout << "  after reset: used " << frame_arena.used()
              << " bytes (whole frame freed in O(1))\n";

    std::cout << "-- object pool --\n";
    struct Bullet {
        Vec2 pos, vel;
    };
    ObjectPool<Bullet, 8> bullets;
    Bullet* b1 = bullets.acquire();
    Bullet* b2 = bullets.acquire();
    b1->pos = { 0, 0 };
    b2->pos = { 1, 0 };
    std::cout << "  fired 2 bullets, pool has " << bullets.available()
              << " / 8 slots left\n";
    bullets.release(b1);
    std::cout << "  bullet recycled, " << bullets.available()
              << " slots available (no heap traffic)\n";

    std::cout << "-- AoS vs SoA (updating position only) --\n";
    const std::size_t N = 200000;
    const int STEPS = 20;
    const float dt = 1.0f / 60.0f;

    std::vector<ParticleAoS> aos(N);
    ParticlesSoA soa;
    soa.position.resize(N);
    soa.velocity.resize(N);
    soa.lifetime.resize(N);
    soa.size.resize(N);
    soa.color.resize(N);
    soa.active.resize(N);
    for (std::size_t i = 0; i < N; i++) {
        Vec2 v{ static_cast<float>(i % 100) * 0.01f, 0.5f };
        aos[i].velocity = v;
        soa.velocity[i] = v;
    }

    using clock = std::chrono::steady_clock;

    auto t0 = clock::now();
    for (int s = 0; s < STEPS; s++) {
        for (std::size_t i = 0; i < N; i++) {
            aos[i].position += aos[i].velocity * dt;
        }
    }
    auto aos_us = std::chrono::duration_cast<std::chrono::microseconds>(
                      clock::now() - t0).count();

    auto t1 = clock::now();
    for (int s = 0; s < STEPS; s++) {
        for (std::size_t i = 0; i < N; i++) {
            soa.position[i] += soa.velocity[i] * dt;
        }
    }
    auto soa_us = std::chrono::duration_cast<std::chrono::microseconds>(
                      clock::now() - t1).count();

    /* Print a checksum so the compiler cannot delete the loops. */
    std::cout << "  " << N << " particles x " << STEPS << " steps:\n";
    std::cout << "  AoS: " << aos_us << " us   SoA: " << soa_us
              << " us   (checksums " << aos[N - 1].position.x << " / "
              << soa.position[N - 1].x << ")\n";
    std::cout << "  (timings vary per run; SoA wins as unused fields grow)\n";
}

/* ===========================================================================
 * CHAPTER 8 — ENTITY COMPONENT SYSTEM (ECS)
 * ===========================================================================
 * The architecture of modern engines. Instead of deep inheritance trees
 * (Enemy -> FlyingEnemy -> FlyingFireEnemy...), an ENTITY is just an ID,
 * COMPONENTS are plain data attached to IDs, and SYSTEMS are functions
 * that process every entity holding the components they care about.
 * Composition over inheritance, and data laid out for the cache.
 */

using Entity = std::uint32_t;

/* Components are pure data — no methods, no virtuals. */
struct TransformComp {
    Vec2 position;
    float rotation = 0.0f;
};

struct VelocityComp {
    Vec2 value;
};

struct HealthComp {
    int hp = 100;
};

/*
 * class World
 * -----------
 * Owns all entities and component storage. This teaching version stores
 * components in unordered_maps keyed by entity — simple and clear.
 * Production ECS (EnTT, flecs) packs each component type into dense
 * arrays for the SoA cache wins from Chapter 7; the API stays the same.
 */
class World {
public:
    /* create_entity: an entity is born as just a fresh ID. */
    Entity create_entity()
    {
        Entity e = next_id_++;
        alive_.push_back(e);
        return e;
    }

    /* destroy_entity: remove the ID and every component attached to it. */
    void destroy_entity(Entity e)
    {
        alive_.erase(std::remove(alive_.begin(), alive_.end(), e), alive_.end());
        transforms.erase(e);
        velocities.erase(e);
        healths.erase(e);
    }

    std::size_t entity_count() const { return alive_.size(); }

    /* Public component tables: systems iterate them directly. */
    std::unordered_map<Entity, TransformComp> transforms;
    std::unordered_map<Entity, VelocityComp> velocities;
    std::unordered_map<Entity, HealthComp> healths;

private:
    Entity next_id_ = 1;
    std::vector<Entity> alive_;
};

/*
 * movement_system
 * ---------------
 * Processes every entity that has BOTH a velocity and a transform:
 * position += velocity * dt. Notice it knows nothing about goblins or
 * bullets — any entity with the right components moves.
 */
static void movement_system(World& world, float dt)
{
    for (auto& [entity, vel] : world.velocities) {
        auto it = world.transforms.find(entity);
        if (it != world.transforms.end()) {
            it->second.position += vel.value * dt;
        }
    }
}

/*
 * poison_system
 * -------------
 * Ticks damage on every entity with health. Systems are just functions:
 * adding a mechanic = adding a system, touching nothing else.
 */
static void poison_system(World& world, int damage)
{
    for (auto& [entity, health] : world.healths) {
        health.hp -= damage;
    }
}

/*
 * reap_system
 * -----------
 * Collects entities whose hp reached 0 and destroys them. Deferred
 * destruction (collect first, then destroy) avoids invalidating the map
 * we are iterating — a real ECS lesson in miniature.
 */
static std::size_t reap_system(World& world)
{
    std::vector<Entity> dead;
    for (const auto& [entity, health] : world.healths) {
        if (health.hp <= 0) dead.push_back(entity);
    }
    for (Entity e : dead) {
        world.destroy_entity(e);
    }
    return dead.size();
}

static void demo_ecs(void)
{
    World world;

    /* Compose entities from components — no class hierarchy needed. */
    Entity player = world.create_entity();
    world.transforms[player] = { { 0, 0 }, 0.0f };
    world.velocities[player] = { { 2.0f, 1.0f } };
    world.healths[player] = { 30 };

    Entity arrow = world.create_entity();               /* moves, no hp   */
    world.transforms[arrow] = { { 5, 5 }, 0.0f };
    world.velocities[arrow] = { { 10.0f, 0.0f } };

    Entity chest = world.create_entity();               /* static, has hp */
    world.transforms[chest] = { { 3, 3 }, 0.0f };
    world.healths[chest] = { 10 };

    std::cout << "entities: player(move+hp), arrow(move), chest(hp) — "
              << world.entity_count() << " total\n";

    std::cout << std::fixed << std::setprecision(1);
    for (int frame = 1; frame <= 3; frame++) {
        movement_system(world, 0.5f);
        poison_system(world, 6);
        std::size_t reaped = reap_system(world);
        std::cout << "frame " << frame
                  << ": player at " << world.transforms[player].position
                  << ", arrow at " << world.transforms[arrow].position
                  << ", reaped " << reaped << " entities\n";
    }
    std::cout << std::defaultfloat << std::setprecision(6);
    std::cout << "entities remaining: " << world.entity_count()
              << " (the chest died to poison on frame 2)\n";
}

/* ===========================================================================
 * CHAPTER 9 — GAME LOOP & TIMING
 * ===========================================================================
 * The heartbeat of the engine. Frames take variable real time, but
 * physics must be DETERMINISTIC — so we simulate in fixed steps and let
 * rendering interpolate between them. This is the "fixed timestep with
 * accumulator" pattern (Glenn Fiedler's famous "Fix Your Timestep!").
 */

/*
 * class Clock
 * -----------
 * Thin wrapper over std::chrono::steady_clock (monotonic — never jumps
 * when the user changes the wall clock, which system_clock does).
 * restart() returns seconds since the previous restart: the frame delta.
 */
class Clock {
public:
    Clock() : last_(std::chrono::steady_clock::now()) {}

    float restart()
    {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<float> delta = now - last_;
        last_ = now;
        return delta.count();
    }

private:
    std::chrono::steady_clock::time_point last_;
};

/*
 * demo_game_loop
 * --------------
 * Runs the fixed-timestep pattern over SYNTHETIC frame times so the
 * output is deterministic and inspectable:
 *
 *   accumulator += frame_time;
 *   while (accumulator >= dt) { simulate(dt); accumulator -= dt; }
 *   alpha = accumulator / dt;   // how far INTO the next step we are
 *   render(lerp(previous_state, current_state, alpha));
 *
 * Slow frames simulate several steps to catch up; fast frames simulate
 * none and just re-render. Physics always sees exactly dt.
 */
static void demo_game_loop(void)
{
    /* First: measure something real with the Clock. */
    Clock clock;
    volatile long sink = 0;
    for (long i = 0; i < 3000000; i++) {
        sink += i;
    }
    (void)sink;
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Clock measured a busy-loop at " << clock.restart() * 1000.0f
              << " ms (varies per run)\n\n";

    /* Now the fixed-timestep loop with synthetic frame times. */
    const float dt = 1.0f / 60.0f;              /* simulation step: 16.67 ms */
    float accumulator = 0.0f;

    Vec2 prev_pos{ 0, 0 };                      /* state before the newest step */
    Vec2 pos{ 0, 0 };
    const Vec2 velocity{ 60.0f, 0.0f };         /* 60 units per second */

    const float frame_times[] = { 0.016f, 0.034f, 0.008f, 0.025f, 0.041f };

    std::cout << "fixed dt = " << dt << " s; ball moves 60 units/s\n";
    int frame_number = 0;
    for (float frame_time : frame_times) {
        frame_number++;
        accumulator += frame_time;

        int steps = 0;
        while (accumulator >= dt) {
            prev_pos = pos;
            pos += velocity * dt;               /* simulate ONE fixed step */
            accumulator -= dt;
            steps++;
        }

        /* Render interpolation: blend the last two physics states by how
         * far into the next step we are, eliminating visual stutter. */
        float alpha = accumulator / dt;
        Vec2 render_pos = lerp(prev_pos, pos, alpha);

        std::cout << "frame " << frame_number
                  << " (" << std::setw(6) << frame_time * 1000.0f << " ms): "
                  << steps << " step(s), sim x = " << std::setw(7) << pos.x
                  << ", render x = " << std::setw(7) << render_pos.x
                  << " (alpha " << alpha << ")\n";
    }
    std::cout << std::defaultfloat << std::setprecision(6);
    std::cout << "note: the 8 ms frame ran 0 steps; the 41 ms frame ran 3 — "
                 "physics stays deterministic\n";
}

/* ===========================================================================
 * CHAPTER 10 — EVENTS & INPUT
 * ===========================================================================
 * Systems must talk without knowing about each other: the audio system
 * should not #include the combat system. An EVENT BUS decouples them —
 * publishers emit events, subscribers register callbacks (the observer
 * pattern), and a queue defers delivery to a controlled point in the frame.
 */

enum class EventType {
    KeyPressed,
    EntityDamaged,
    ItemPickedUp,
};

static const char* event_type_name(EventType t)
{
    switch (t) {
    case EventType::KeyPressed:    return "KeyPressed";
    case EventType::EntityDamaged: return "EntityDamaged";
    case EventType::ItemPickedUp:  return "ItemPickedUp";
    }
    return "unknown";
}

/*
 * struct Event
 * ------------
 * A small tagged record. `a` and `b` mean different things per type
 * (key code, entity id + damage, ...). Real engines use std::variant or
 * per-type structs; two ints keep the pattern in plain sight.
 */
struct Event {
    EventType type = EventType::KeyPressed;
    int a = 0;
    int b = 0;
};

/*
 * class EventBus
 * --------------
 * subscribe(): registers a callback (any lambda or function, thanks to
 * std::function's type erasure) for one event type.
 * publish():   appends the event to a queue — nothing runs yet.
 * dispatch():  drains the queue, invoking every subscriber of each
 *              event's type. Deferring keeps callbacks from firing in
 *              the middle of another system's update.
 */
class EventBus {
public:
    using Handler = std::function<void(const Event&)>;

    void subscribe(EventType type, Handler handler)
    {
        subscribers_[type].push_back(std::move(handler));
    }

    void publish(const Event& event)
    {
        queue_.push(event);
    }

    void dispatch()
    {
        while (!queue_.empty()) {
            Event event = queue_.front();
            queue_.pop();
            auto it = subscribers_.find(event.type);
            if (it == subscribers_.end()) continue;
            for (const Handler& handler : it->second) {
                handler(event);
            }
        }
    }

private:
    std::unordered_map<EventType, std::vector<Handler>> subscribers_;
    std::queue<Event> queue_;
};

/*
 * Input mapping
 * -------------
 * Never scatter raw key codes through gameplay code. Keys map to
 * abstract ACTIONS in one table; gameplay reads actions. Rebinding keys
 * or adding a gamepad then touches exactly one place — a cornerstone of
 * cross-platform input handling.
 */
enum class Action { MoveLeft, MoveRight, Jump, Fire };

static const char* action_name(Action a)
{
    switch (a) {
    case Action::MoveLeft:  return "MoveLeft";
    case Action::MoveRight: return "MoveRight";
    case Action::Jump:      return "Jump";
    case Action::Fire:      return "Fire";
    }
    return "unknown";
}

static void demo_events_and_input(void)
{
    EventBus bus;

    /* Two independent systems subscribe to the same event. */
    bus.subscribe(EventType::EntityDamaged, [](const Event& e) {
        std::cout << "  [audio] playing hit sound for entity " << e.a << "\n";
    });
    bus.subscribe(EventType::EntityDamaged, [](const Event& e) {
        std::cout << "  [ui]    flashing health bar (-" << e.b << " hp)\n";
    });
    bus.subscribe(EventType::ItemPickedUp, [](const Event& e) {
        std::cout << "  [quest] item " << e.a << " collected, updating quest\n";
    });

    /* Combat publishes without knowing audio, UI or quests exist. */
    auto publish_logged = [&bus](const Event& event) {
        bus.publish(event);
        std::cout << "queued " << event_type_name(event.type)
                  << " (a=" << event.a << ", b=" << event.b << ")\n";
    };
    publish_logged({ EventType::EntityDamaged, 42, 12 });
    publish_logged({ EventType::ItemPickedUp, 7, 0 });
    std::cout << "nothing has run yet — now dispatching:\n";
    bus.dispatch();

    /* Key -> action mapping. */
    const std::unordered_map<char, Action> bindings = {
        { 'a', Action::MoveLeft },
        { 'd', Action::MoveRight },
        { 'w', Action::Jump },
        { ' ', Action::Fire },
    };

    const std::string keys_this_frame = "adw x";
    std::cout << "keys pressed: \"" << keys_this_frame << "\" -> actions: ";
    for (char key : keys_this_frame) {
        auto it = bindings.find(key);
        if (it != bindings.end()) {
            std::cout << action_name(it->second) << " ";
        } else {
            std::cout << "(unbound:'" << key << "') ";
        }
    }
    std::cout << "\n";
}

/* ===========================================================================
 * CHAPTER 11 — COLLISION & PHYSICS
 * ===========================================================================
 * 2D games mostly need two collision shapes — axis-aligned boxes and
 * circles — plus simple velocity integration. That covers platformers,
 * shooters, and most arcade genres.
 */

/*
 * struct AABB — axis-aligned bounding box
 * ---------------------------------------
 * Stored as min/max corners. Axis-aligned boxes never rotate, which
 * makes their overlap test four comparisons.
 */
struct AABB {
    Vec2 min;
    Vec2 max;
};

/*
 * aabb_overlap
 * ------------
 * Separating-axis logic in its simplest form: the boxes overlap unless
 * one is entirely to the left/right/above/below the other.
 */
static bool aabb_overlap(const AABB& a, const AABB& b)
{
    return a.min.x < b.max.x && a.max.x > b.min.x &&
           a.min.y < b.max.y && a.max.y > b.min.y;
}

/*
 * aabb_penetration
 * ----------------
 * For overlapping boxes, returns the smallest vector that pushes `a`
 * out of `b`. Resolving along the axis of LEAST penetration is what
 * makes characters slide along walls instead of sticking.
 */
static Vec2 aabb_penetration(const AABB& a, const AABB& b)
{
    float push_right = b.max.x - a.min.x;       /* push a to the right  */
    float push_left = a.max.x - b.min.x;        /* push a to the left   */
    float push_down = b.max.y - a.min.y;
    float push_up = a.max.y - b.min.y;

    float min_x = std::min(push_right, push_left);
    float min_y = std::min(push_down, push_up);

    if (min_x < min_y) {
        return { (push_right < push_left) ? push_right : -push_left, 0.0f };
    }
    return { 0.0f, (push_down < push_up) ? push_down : -push_up };
}

/*
 * struct CircleShape + circle_overlap
 * -----------------------------------
 * Circles overlap when the distance between centers is less than the
 * sum of radii. Comparing SQUARED distances avoids the sqrt — a free
 * and idiomatic optimization.
 */
struct CircleShape {
    Vec2 center;
    float radius = 0.0f;
};

static bool circle_overlap(const CircleShape& a, const CircleShape& b)
{
    Vec2 d = b.center - a.center;
    float r = a.radius + b.radius;
    return dot(d, d) < r * r;
}

/*
 * circle_vs_aabb
 * --------------
 * Clamp the circle's center to the box to find the closest point on the
 * box, then it's just a point-in-circle test. One clamp turns a hard-
 * looking problem into an easy one.
 */
static bool circle_vs_aabb(const CircleShape& c, const AABB& box)
{
    Vec2 closest{
        clampf(c.center.x, box.min.x, box.max.x),
        clampf(c.center.y, box.min.y, box.max.y),
    };
    Vec2 d = c.center - closest;
    return dot(d, d) < c.radius * c.radius;
}

/*
 * struct Body + integrate
 * -----------------------
 * Semi-implicit Euler integration: update velocity FIRST, then position
 * with the new velocity. More stable than naive Euler at game timesteps
 * and what most 2D engines actually ship.
 */
struct Body {
    Vec2 position;
    Vec2 velocity;
};

static void integrate(Body& body, Vec2 gravity, float dt)
{
    body.velocity += gravity * dt;
    body.position += body.velocity * dt;
}

static void demo_collision_and_physics(void)
{
    std::cout << std::fixed << std::setprecision(2);

    AABB player{ { 0, 0 }, { 2, 2 } };
    AABB wall{ { 1.5f, 0 }, { 4, 4 } };
    std::cout << "player vs wall overlap: "
              << (aabb_overlap(player, wall) ? "yes" : "no") << "\n";
    Vec2 push = aabb_penetration(player, wall);
    std::cout << "resolve by pushing player " << push
              << " (least-penetration axis)\n";

    CircleShape bomb{ { 0, 0 }, 3.0f };
    CircleShape crate{ { 4, 0 }, 1.5f };
    std::cout << "bomb blast hits crate: "
              << (circle_overlap(bomb, crate) ? "yes" : "no") << "\n";
    std::cout << "bomb blast hits wall : "
              << (circle_vs_aabb(bomb, wall) ? "yes" : "no") << "\n";

    /* A ball dropped from height 10, bouncing with restitution 0.6. */
    Body ball{ { 0.0f, 10.0f }, { 0.0f, 0.0f } };
    const Vec2 gravity{ 0.0f, -units::GRAVITY };
    const float restitution = 0.6f;
    const float dt = 1.0f / 10.0f;              /* big steps to keep output short */

    std::cout << "bouncing ball (y, vy):";
    for (int step = 0; step < 12; step++) {
        integrate(ball, gravity, dt);
        if (ball.position.y < 0.0f) {           /* hit the floor */
            ball.position.y = 0.0f;
            ball.velocity.y = -ball.velocity.y * restitution;
        }
        std::cout << " (" << ball.position.y << ", " << ball.velocity.y << ")";
    }
    std::cout << "\n";
    std::cout << std::defaultfloat << std::setprecision(6);
}

/* ===========================================================================
 * CHAPTER 12 — RENDERER ABSTRACTION (THE PLATFORM LAYER)
 * ===========================================================================
 * Cross-platform means game code NEVER calls a platform API directly.
 * It draws through an abstract interface; each platform gets a backend
 * (OpenGL, Metal, DirectX, ...). Here the "platform" is the terminal:
 * a console backend proves the abstraction and needs zero dependencies —
 * and swapping in an SDL/OpenGL backend later changes no game code.
 */

/*
 * class IRenderer (interface)
 * ---------------------------
 * Pure-virtual drawing contract. Note the pattern from Chapter 5 at
 * engine scale: game code holds an IRenderer&, never a ConsoleRenderer.
 */
class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual void clear(char background) = 0;
    virtual void draw_glyph(int x, int y, char glyph) = 0;
    virtual void draw_text(int x, int y, const std::string& text) = 0;
    virtual void present() = 0;

    virtual int width() const = 0;
    virtual int height() const = 0;
};

/*
 * class ConsoleRenderer
 * ---------------------
 * The terminal backend: an off-screen character framebuffer (reusing
 * Grid<char> from Chapter 4). Draw calls write into the buffer;
 * present() flushes it to stdout in one go. This is DOUBLE BUFFERING —
 * the same architecture as a GPU swap chain, in ASCII.
 */
class ConsoleRenderer final : public IRenderer {
public:
    ConsoleRenderer(int width, int height)
        : framebuffer_(width, height, ' ') {}

    void clear(char background) override
    {
        for (int y = 0; y < framebuffer_.height(); y++) {
            for (int x = 0; x < framebuffer_.width(); x++) {
                framebuffer_.at(x, y) = background;
            }
        }
    }

    void draw_glyph(int x, int y, char glyph) override
    {
        if (framebuffer_.in_bounds(x, y)) {     /* clip off-screen draws */
            framebuffer_.at(x, y) = glyph;
        }
    }

    void draw_text(int x, int y, const std::string& text) override
    {
        for (std::size_t i = 0; i < text.size(); i++) {
            draw_glyph(x + static_cast<int>(i), y, text[i]);
        }
    }

    void present() override
    {
        std::string border(static_cast<std::size_t>(framebuffer_.width()) + 2, '-');
        std::cout << "+" << border << "+\n";
        for (int y = 0; y < framebuffer_.height(); y++) {
            std::cout << "| ";
            for (int x = 0; x < framebuffer_.width(); x++) {
                std::cout << framebuffer_.at(x, y);
            }
            std::cout << " |\n";
        }
        std::cout << "+" << border << "+\n";
    }

    int width() const override { return framebuffer_.width(); }
    int height() const override { return framebuffer_.height(); }

private:
    Grid<char> framebuffer_;
};

/*
 * struct Animation
 * ----------------
 * Flipbook animation: cycle through frames at a fixed rate, driven by
 * the frame delta (never by frame COUNT, or the game animates faster on
 * faster machines — the classic beginner mistake).
 */
struct Animation {
    std::vector<char> frames;
    float seconds_per_frame = 0.1f;
    float elapsed = 0.0f;
    std::size_t index = 0;

    void update(float dt)
    {
        elapsed += dt;
        while (elapsed >= seconds_per_frame) {
            elapsed -= seconds_per_frame;
            index = (index + 1) % frames.size();
        }
    }

    char current() const { return frames[index]; }
};

/*
 * draw_scene
 * ----------
 * Game-side drawing that only knows IRenderer — proof the abstraction
 * holds. It would compile unchanged against an OpenGL backend.
 */
static void draw_scene(IRenderer& renderer, int hero_x, char hero_glyph)
{
    renderer.clear(' ');
    for (int x = 0; x < renderer.width(); x++) {
        renderer.draw_glyph(x, renderer.height() - 1, '=');    /* ground */
    }
    renderer.draw_glyph(3, renderer.height() - 2, 'T');        /* a tree */
    renderer.draw_glyph(hero_x, renderer.height() - 2, hero_glyph);
    renderer.draw_text(0, 0, "score: 120");
    renderer.present();
}

static void demo_renderer(void)
{
    ConsoleRenderer console(20, 5);
    IRenderer& renderer = console;              /* game code sees only this */

    Animation spin{ { '|', '/', '-', '\\' }, 0.1f, 0.0f, 0 };

    /* Three "frames": the hero walks right while their glyph animates. */
    for (int frame = 0; frame < 3; frame++) {
        std::cout << "frame " << frame + 1 << ":\n";
        draw_scene(renderer, 8 + frame * 3, spin.current());
        spin.update(0.1f);                      /* advance by the frame delta */
    }
    std::cout << "same draw_scene() would run on an OpenGL/SDL backend "
                 "implementing IRenderer\n";
}

/* ===========================================================================
 * CHAPTER 13 — PATHFINDING & AI
 * ===========================================================================
 * Two workhorses of game AI: A* to find WHERE to go, and finite state
 * machines to decide WHAT to do.
 */

/*
 * struct GridPos
 * --------------
 * Integer tile coordinates for pathfinding (distinct from the float Vec2
 * used by physics — mixing the two is a classic source of bugs).
 */
struct GridPos {
    int x = 0;
    int y = 0;
};

/*
 * find_path_astar
 * ---------------
 * A* over a tile grid (0 = walkable, 1 = wall), 4-directional movement.
 *
 * A* = Dijkstra + a HEURISTIC: it expands the node with the lowest
 * f = g + h, where g is the real cost from the start and h is an
 * admissible estimate to the goal (Manhattan distance here — never
 * overestimates on a 4-connected grid, which guarantees optimal paths).
 *
 * Implementation notes worth teaching:
 *  - the open set is a min-heap (std::priority_queue with std::greater);
 *  - stale heap entries are skipped instead of updated ("lazy deletion"),
 *    much simpler than a decrease-key operation;
 *  - came_from records each tile's predecessor so the path can be
 *    reconstructed backwards from the goal.
 *
 * Returns the path including start and goal, or empty if unreachable.
 */
static std::vector<GridPos> find_path_astar(const Grid<int>& map,
                                            GridPos start, GridPos goal)
{
    const int w = map.width();
    const int h = map.height();
    auto index = [w](int x, int y) { return static_cast<std::size_t>(y * w + x); };
    auto heuristic = [&goal](int x, int y) {
        return std::abs(x - goal.x) + std::abs(y - goal.y);
    };

    const int INF = 1 << 29;
    std::vector<int> g_cost(static_cast<std::size_t>(w * h), INF);
    std::vector<int> came_from(static_cast<std::size_t>(w * h), -1);

    /* Min-heap of (f, x, y). std::greater flips priority_queue's default
     * max-heap into the min-heap we need. */
    using OpenEntry = std::tuple<int, int, int>;
    std::priority_queue<OpenEntry, std::vector<OpenEntry>, std::greater<OpenEntry>> open;

    g_cost[index(start.x, start.y)] = 0;
    open.emplace(heuristic(start.x, start.y), start.x, start.y);

    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    while (!open.empty()) {
        auto [f, x, y] = open.top();
        open.pop();

        if (x == goal.x && y == goal.y) break;      /* goal reached */

        /* Lazy deletion: skip if this entry is stale. */
        if (f - heuristic(x, y) > g_cost[index(x, y)]) continue;

        for (int dir = 0; dir < 4; dir++) {
            int nx = x + dx[dir];
            int ny = y + dy[dir];
            if (!map.in_bounds(nx, ny) || map.at(nx, ny) != 0) continue;

            int new_g = g_cost[index(x, y)] + 1;
            if (new_g < g_cost[index(nx, ny)]) {
                g_cost[index(nx, ny)] = new_g;
                came_from[index(nx, ny)] = static_cast<int>(index(x, y));
                open.emplace(new_g + heuristic(nx, ny), nx, ny);
            }
        }
    }

    if (came_from[index(goal.x, goal.y)] == -1 &&
        !(start.x == goal.x && start.y == goal.y)) {
        return {};                                   /* unreachable */
    }

    /* Walk predecessors backwards from the goal, then reverse. */
    std::vector<GridPos> path;
    std::size_t current = index(goal.x, goal.y);
    for (;;) {
        path.push_back({ static_cast<int>(current) % w,
                         static_cast<int>(current) / w });
        int prev = came_from[current];
        if (prev == -1) break;
        current = static_cast<std::size_t>(prev);
    }
    std::reverse(path.begin(), path.end());
    return path;
}

/*
 * class Guard — a finite state machine
 * ------------------------------------
 * States (Patrol / Chase / Attack) with transitions driven by distance
 * to the player. FSMs make AI behavior explicit and debuggable: at any
 * moment the guard is in exactly one state, and every transition is a
 * visible, loggable event. Most game AI up to mid-size titles is FSMs.
 */
enum class GuardState { Patrol, Chase, Attack };

static const char* guard_state_name(GuardState s)
{
    switch (s) {
    case GuardState::Patrol: return "Patrol";
    case GuardState::Chase:  return "Chase";
    case GuardState::Attack: return "Attack";
    }
    return "unknown";
}

class Guard {
public:
    /* update: one AI tick. Distance thresholds pick the desired state;
     * transitions are logged; the state's behavior then runs. */
    void update(Vec2 player_pos)
    {
        float distance = (player_pos - pos_).length();

        GuardState desired;
        if (distance > 6.0f) {
            desired = GuardState::Patrol;
        } else if (distance > 1.5f) {
            desired = GuardState::Chase;
        } else {
            desired = GuardState::Attack;
        }

        if (desired != state_) {
            std::cout << "  guard: " << guard_state_name(state_) << " -> "
                      << guard_state_name(desired)
                      << " (player distance " << distance << ")\n";
            state_ = desired;
        }

        switch (state_) {
        case GuardState::Patrol:
            /* wander along the patrol route */
            break;
        case GuardState::Chase:
            pos_ += (player_pos - pos_).normalized() * 1.0f;   /* step closer */
            break;
        case GuardState::Attack:
            std::cout << "  guard: swings sword!\n";
            break;
        }
    }

    GuardState state() const { return state_; }

private:
    GuardState state_ = GuardState::Patrol;
    Vec2 pos_{ 0.0f, 0.0f };
};

static void demo_pathfinding_and_ai(void)
{
    /* Build a small map with a wall the path must route around. */
    Grid<int> map(10, 6, 0);
    for (int y = 0; y < 4; y++) {
        map.at(5, y) = 1;
    }

    GridPos start{ 1, 1 };
    GridPos goal{ 8, 1 };
    std::vector<GridPos> path = find_path_astar(map, start, goal);
    std::cout << "A* found a path of " << path.size() << " tiles:\n";

    /* Render the map with the path overlaid. */
    Grid<char> view(map.width(), map.height(), '.');
    for (int y = 0; y < map.height(); y++) {
        for (int x = 0; x < map.width(); x++) {
            if (map.at(x, y) != 0) view.at(x, y) = '#';
        }
    }
    for (const GridPos& p : path) {
        view.at(p.x, p.y) = '*';
    }
    view.at(start.x, start.y) = 'S';
    view.at(goal.x, goal.y) = 'G';
    for (int y = 0; y < view.height(); y++) {
        std::cout << "  ";
        for (int x = 0; x < view.width(); x++) {
            std::cout << view.at(x, y);
        }
        std::cout << "\n";
    }

    /* FSM: the player approaches, engages, and flees. */
    std::cout << std::fixed << std::setprecision(1);
    Guard guard;
    const Vec2 player_positions[] = {
        { 10, 0 }, { 5, 0 }, { 3, 0 }, { 1, 0 }, { 0.5f, 0 }, { 9, 0 },
    };
    for (Vec2 p : player_positions) {
        guard.update(p);
    }
    std::cout << std::defaultfloat << std::setprecision(6);
}

/* ===========================================================================
 * CHAPTER 14 — CONCURRENCY: A JOB SYSTEM
 * ===========================================================================
 * Modern CPUs have many cores; engines feed them with a JOB SYSTEM:
 * a pool of worker threads consuming small tasks from a shared queue.
 * Physics, animation, particles and asset loading all become jobs.
 */

/*
 * class ThreadPool
 * ----------------
 * N workers sleep on a condition variable until work arrives.
 *
 *  submit()    — enqueue any callable; one sleeping worker wakes.
 *  wait_idle() — block until the queue is empty AND no job is running
 *                (a frame barrier: "all jobs done, safe to render").
 *  destructor  — sets the stop flag, wakes everyone, joins all threads.
 *
 * The mutex protects the queue and counters; the two condition variables
 * separate "work available" (workers wait on it) from "all work done"
 * (wait_idle waits on it). This ~60-line pattern is the heart of every
 * production job system.
 */
class ThreadPool {
public:
    explicit ThreadPool(std::size_t thread_count)
    {
        for (std::size_t i = 0; i < thread_count; i++) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    ~ThreadPool()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        work_available_.notify_all();
        for (std::thread& worker : workers_) {
            worker.join();
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void submit(std::function<void()> job)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            jobs_.push(std::move(job));
        }
        work_available_.notify_one();
    }

    void wait_idle()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        all_done_.wait(lock, [this] {
            return jobs_.empty() && active_jobs_ == 0;
        });
    }

private:
    void worker_loop()
    {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                work_available_.wait(lock, [this] {
                    return stopping_ || !jobs_.empty();
                });
                if (stopping_ && jobs_.empty()) {
                    return;
                }
                job = std::move(jobs_.front());
                jobs_.pop();
                ++active_jobs_;
            }

            job();                              /* run OUTSIDE the lock */

            {
                std::lock_guard<std::mutex> lock(mutex_);
                --active_jobs_;
                if (jobs_.empty() && active_jobs_ == 0) {
                    all_done_.notify_all();
                }
            }
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> jobs_;
    std::mutex mutex_;
    std::condition_variable work_available_;
    std::condition_variable all_done_;
    std::size_t active_jobs_ = 0;
    bool stopping_ = false;
};

static void demo_job_system(void)
{
    const unsigned hw = std::thread::hardware_concurrency();
    std::cout << "hardware_concurrency() reports " << hw << " threads\n";

    ThreadPool pool(4);

    /* Parallel sum: each job owns ONE slot of `partial`, so no two jobs
     * ever write the same memory — parallelism without locks. */
    const std::size_t N = 1000000;
    std::vector<int> data(N);
    for (std::size_t i = 0; i < N; i++) {
        data[i] = static_cast<int>(i % 100);
    }

    const std::size_t CHUNKS = 4;
    std::vector<long long> partial(CHUNKS, 0);
    const std::size_t chunk_size = N / CHUNKS;

    for (std::size_t c = 0; c < CHUNKS; c++) {
        pool.submit([c, chunk_size, &data, &partial] {
            std::size_t begin = c * chunk_size;
            std::size_t end = begin + chunk_size;
            long long sum = 0;
            for (std::size_t i = begin; i < end; i++) {
                sum += data[i];
            }
            partial[c] = sum;
        });
    }
    pool.wait_idle();                           /* the frame barrier */

    long long total = std::accumulate(partial.begin(), partial.end(), 0LL);
    long long expected = std::accumulate(data.begin(), data.end(), 0LL);
    std::cout << "parallel sum = " << total << ", serial sum = " << expected
              << " -> " << (total == expected ? "match" : "MISMATCH") << "\n";

    /* Atomics: increments from many threads with no lock and no lost
     * updates. A plain int here would be a data race (undefined behavior)
     * and would likely lose counts. */
    std::atomic<int> particles_spawned{ 0 };
    for (int j = 0; j < 8; j++) {
        pool.submit([&particles_spawned] {
            for (int i = 0; i < 10000; i++) {
                particles_spawned.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    pool.wait_idle();
    std::cout << "atomic counter after 8 jobs x 10000 increments: "
              << particles_spawned.load() << " (exactly 80000)\n";
    std::cout << "rule of thumb: parallelize BIG work; tiny jobs cost more "
                 "to schedule than they save\n";
}

/* ===========================================================================
 * CHAPTER 15 — MINI GAME: EVERYTHING TOGETHER
 * ===========================================================================
 * "Asteroid Dodge": asteroids fall down the screen, the player dodges.
 * One small playable simulation using the chapters together:
 *   math (Ch3), Grid & renderer (Ch4/12), fixed-step updates (Ch9),
 *   input actions (Ch10), collision (Ch11), seeded RNG for determinism.
 * The input is a scripted keystroke sequence so the run is reproducible
 * headlessly — swap it for real keyboard reads and it's interactive.
 */

/*
 * struct Asteroid
 * ---------------
 * Integer tile coordinates — this game snaps to the character grid.
 */
struct Asteroid {
    int x = 0;
    int y = 0;
};

/*
 * run_mini_game
 * -------------
 * The complete game loop: input -> update -> collide -> render, once
 * per frame for a fixed number of frames.
 */
static void run_mini_game(void)
{
    const int W = 24, H = 7;
    ConsoleRenderer console(W, H);
    IRenderer& renderer = console;

    /* Deterministic RNG: same seed, same asteroid pattern, every run —
     * exactly how games implement replays and daily challenges. */
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> spawn_column(0, W - 1);

    int player_x = W / 2;
    const int player_y = H - 1;
    std::vector<Asteroid> asteroids;
    int dodged = 0;
    int hits = 0;

    /* Scripted input: 'a' = left, 'd' = right, '.' = stay. */
    const std::string script = "..add.dda.aa";

    for (std::size_t frame = 0; frame < script.size(); frame++) {
        /* 1. INPUT — translate the key into movement, clamped to the field. */
        char key = script[frame];
        if (key == 'a') player_x = std::max(0, player_x - 2);
        if (key == 'd') player_x = std::min(W - 1, player_x + 2);

        /* 2. UPDATE — spawn on even frames, everything falls one row. */
        if (frame % 2 == 0) {
            asteroids.push_back({ spawn_column(rng), 0 });
        }
        for (Asteroid& a : asteroids) {
            a.y += 1;
        }

        /* 3. COLLISION — grid-exact: same tile means a hit. Asteroids
         * that reach the bottom row without hitting count as dodged. */
        std::vector<Asteroid> survivors;
        for (const Asteroid& a : asteroids) {
            if (a.y == player_y && a.x == player_x) {
                hits++;
            } else if (a.y >= H) {
                dodged++;
            } else {
                survivors.push_back(a);
            }
        }
        asteroids = std::move(survivors);

        /* 4. RENDER — through the abstract interface, as always. */
        renderer.clear(' ');
        for (const Asteroid& a : asteroids) {
            renderer.draw_glyph(a.x, a.y, '*');
        }
        renderer.draw_glyph(player_x, player_y, '@');
        renderer.draw_text(0, 0, "dodged:" + std::to_string(dodged) +
                                 " hits:" + std::to_string(hits));
        std::cout << "frame " << frame + 1 << " (input '" << key << "'):\n";
        renderer.present();
    }

    std::cout << "final score — dodged: " << dodged << ", hits: " << hits
              << ". Same seed + same inputs = same result: deterministic!\n";
}

/* ===========================================================================
 * MAIN — runs every chapter in order
 * ===========================================================================
 */

int main(int argc, char* argv[])
{
    std::cout << "engine course invoked as: " << argv[0]
              << " (argc = " << argc << ")\n";

    chapter("1. From C to C++");
    demo_cpp_basics();

    chapter("2. Classes & RAII");
    demo_raii();

    chapter("3. Game math");
    demo_game_math();

    chapter("4. Templates & the STL");
    demo_templates_and_stl();

    chapter("5. Polymorphism & virtual dispatch");
    demo_polymorphism();

    chapter("6. Move semantics & smart pointers");
    demo_move_and_smart_pointers();

    chapter("7. Memory & performance");
    demo_memory_and_performance();

    chapter("8. Entity Component System");
    demo_ecs();

    chapter("9. Game loop & timing");
    demo_game_loop();

    chapter("10. Events & input");
    demo_events_and_input();

    chapter("11. Collision & physics");
    demo_collision_and_physics();

    chapter("12. Renderer abstraction");
    demo_renderer();

    chapter("13. Pathfinding & AI");
    demo_pathfinding_and_ai();

    chapter("14. Concurrency: job system");
    demo_job_system();

    chapter("15. Mini game: Asteroid Dodge");
    run_mini_game();

    std::cout << "\nAll chapters completed successfully.\n";
    return EXIT_SUCCESS;
}
