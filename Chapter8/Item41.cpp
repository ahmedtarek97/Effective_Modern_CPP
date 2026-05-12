// Item 41: Consider pass by value for copyable parameters that are cheap to
//          move and always copied.
//
// Key Points:
//  1. For copyable, cheap-to-move parameters that are ALWAYS copied inside the
//     function, pass by value costs exactly one copy (lvalue) or one move
//     (rvalue), matching the best achievable with overloading — but with only
//     a single function definition.
//  2. Overloading on (const T&) and (T&&) achieves the same cost but requires
//     writing two functions (or N*2^N overloads for N parameters).
//  3. The "always copied" condition is critical.  When the copy is conditional,
//     passing by value may pay the construction cost even when no copy occurs,
//     making it more expensive than passing by reference.
//  4. Pass by value incurs one extra move compared to the reference-based
//     approaches for lvalue arguments: the caller's object is copied into the
//     parameter, then the parameter is moved into the destination.
//  5. "Cheap to move" matters: pass by value is inappropriate for types that
//     are expensive to move (e.g., std::array, or legacy types without a move
//     constructor), because the extra move then carries non-trivial cost.
//  6. Pass by value is appropriate only for copyable types.  Move-only types
//     (std::unique_ptr, etc.) should be taken by value only when the intent is
//     to transfer ownership, which is a different design pattern.
//  7. The slicing problem: if T is a base class, passing by value silently
//     slices derived objects, so reference semantics are required there.

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <cassert>

// ─────────────────────────────────────────────────────────────────────────────
// Instrumented string wrapper to count copies and moves
// ─────────────────────────────────────────────────────────────────────────────

struct Stats {
    int copies = 0;
    int moves  = 0;

    void reset() { copies = moves = 0; }

    void print(const char* label) const {
        std::cout << "  [" << label << "] copies=" << copies
                  << "  moves=" << moves << '\n';
    }
};

// A minimal wrapper around std::string that records construction operations.
struct InstrStr {
    std::string value;
    Stats*      stats;   // non-owning observer

    // Constructors from raw string literal / std::string lvalue
    explicit InstrStr(Stats& s, std::string v = {})
        : value(std::move(v)), stats(&s) {}

    // Copy constructor
    InstrStr(const InstrStr& o)
        : value(o.value), stats(o.stats)
    { ++stats->copies; }

    // Move constructor
    InstrStr(InstrStr&& o) noexcept
        : value(std::move(o.value)), stats(o.stats)
    { ++stats->moves; }

    // Copy-assignment
    InstrStr& operator=(const InstrStr& o) {
        value = o.value;
        ++stats->copies;
        return *this;
    }

    // Move-assignment
    InstrStr& operator=(InstrStr&& o) noexcept {
        value = std::move(o.value);
        ++stats->moves;
        return *this;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Point 1 & 2  — three implementation strategies for "add a name" in a Widget
//
//  Strategy A: two overloads (const& + &&)          — optimal cost, verbose
//  Strategy B: universal reference + std::forward   — optimal cost, complex
//  Strategy C: pass by value                        — one extra move for lvalue,
//                                                     same as (A) for rvalue
// ─────────────────────────────────────────────────────────────────────────────

// ── Strategy A: overloading ──
class WidgetOverload {
public:
    void addName(const InstrStr& newName) { names_.push_back(newName);            }
    void addName(InstrStr&&      newName) { names_.push_back(std::move(newName)); }
private:
    std::vector<InstrStr> names_;
};

// ── Strategy B: universal reference ──
class WidgetUniRef {
public:
    template<typename T>
    void addName(T&& newName) { names_.push_back(std::forward<T>(newName)); }
private:
    std::vector<InstrStr> names_;
};

// ── Strategy C: pass by value ──
class WidgetByValue {
public:
    void addName(InstrStr newName) { names_.push_back(std::move(newName)); }
private:
    std::vector<InstrStr> names_;
};

// ─────────────────────────────────────────────────────────────────────────────
// Point 1 & 2 — demonstration
// ─────────────────────────────────────────────────────────────────────────────

void point1_and_2_cost_comparison()
{
    std::cout << "=== Points 1 & 2: Cost comparison of three strategies ===\n";

    // ── Lvalue argument ──────────────────────────────────────────────────────
    std::cout << "\n  -- Passing an lvalue --\n";
    Stats s;

    {
        s.reset();
        InstrStr name(s, "Alice");
        WidgetOverload w;
        w.addName(name);                        // calls const& overload
        s.print("Overload (lvalue)");           // 1 copy, 0 moves  ← optimal
    }
    {
        s.reset();
        InstrStr name(s, "Alice");
        WidgetUniRef w;
        w.addName(name);                        // T = InstrStr&, perfect-forward
        s.print("UniRef  (lvalue)");            // 1 copy, 0 moves  ← optimal
    }
    {
        s.reset();
        InstrStr name(s, "Alice");
        WidgetByValue w;
        w.addName(name);                        // copy-construct param, then move into vector
        s.print("ByValue (lvalue)");            // 1 copy, 1 move   ← one extra move
    }

    // ── Rvalue argument ─────────────────────────────────────────────────────
    std::cout << "\n  -- Passing an rvalue --\n";
    {
        s.reset();
        InstrStr name(s, "Bob");
        WidgetOverload w;
        w.addName(std::move(name));             // calls && overload
        s.print("Overload (rvalue)");           // 0 copies, 1 move  ← optimal
    }
    {
        s.reset();
        InstrStr name(s, "Bob");
        WidgetUniRef w;
        w.addName(std::move(name));
        s.print("UniRef  (rvalue)");            // 0 copies, 1 move  ← optimal
    }
    {
        s.reset();
        InstrStr name(s, "Bob");
        WidgetByValue w;
        w.addName(std::move(name));             // move-construct param, then move into vector
        s.print("ByValue (rvalue)");            // 0 copies, 2 moves ← one extra move
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 3 — "always copied" condition: if the copy is conditional, pass by
//           value pays construction cost even when nothing is stored.
// ─────────────────────────────────────────────────────────────────────────────

class FilterByValue {
public:
    // BAD: parameter is always constructed even when the condition fails.
    void addFiltered(InstrStr name, bool accept) {
        if (accept)
            names_.push_back(std::move(name));
        // If !accept the freshly-constructed 'name' is just destroyed.
    }
private:
    std::vector<InstrStr> names_;
};

class FilterByRef {
public:
    // GOOD: no construction unless the condition holds.
    void addFiltered(const InstrStr& name, bool accept) {
        if (accept)
            names_.push_back(name);
    }
private:
    std::vector<InstrStr> names_;
};

void point3_always_copied_condition()
{
    std::cout << "\n=== Point 3: 'Always copied' condition ===\n";

    Stats s;

    // accept == false  → byValue still paid for construction
    {
        s.reset();
        InstrStr name(s, "Charlie");
        FilterByValue f;
        f.addFiltered(name, /*accept=*/false);
        s.print("ByValue  (rejected)");   // 1 copy wasted
    }
    {
        s.reset();
        InstrStr name(s, "Charlie");
        FilterByRef f;
        f.addFiltered(name, /*accept=*/false);
        s.print("ByRef    (rejected)");   // 0 copies — saved the work
    }

    // accept == true  → costs are equal (both do one copy)
    {
        s.reset();
        InstrStr name(s, "Dave");
        FilterByValue f;
        f.addFiltered(name, /*accept=*/true);
        s.print("ByValue  (accepted)");
    }
    {
        s.reset();
        InstrStr name(s, "Dave");
        FilterByRef f;
        f.addFiltered(name, /*accept=*/true);
        s.print("ByRef    (accepted)");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 4 — The extra move for lvalue arguments in the by-value approach.
//
// With an lvalue:
//   • overload/universal-ref: 1 copy  into the destination
//   • by value              : 1 copy  into param  +  1 move  into destination
//
// The move is cheap for most standard-library types (O(1), pointer swap), so
// the extra move is usually acceptable.  The point is to be *aware* of it.
// ─────────────────────────────────────────────────────────────────────────────

void point4_extra_move_for_lvalue()
{
    std::cout << "\n=== Point 4: Extra move for lvalue in by-value ===\n";

    Stats s;
    InstrStr name(s, "Eve");

    // by value: copy-construct param, then move-assign into destination
    s.reset();
    {
        WidgetByValue w;
        w.addName(name);
        s.print("ByValue lvalue  (copy+move)");
    }

    // overload: only copy directly into destination
    s.reset();
    {
        WidgetOverload w;
        w.addName(name);
        s.print("Overload lvalue (copy only)");
    }

    std::cout << "  Note: the extra move is O(1) for std::string/std::vector —\n"
                 "        usually acceptable, but it is there.\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 5 — "Cheap to move" matters.
//
// std::array<T,N> has no heap-allocated storage; its move constructor copies
// all N elements — O(N), NOT O(1).  For such types the extra move in the
// by-value strategy is expensive and the overload/universal-ref strategy
// should be preferred.
// ─────────────────────────────────────────────────────────────────────────────

#include <array>
#include <chrono>

struct HeavyMatrix {
    // Simulate a type whose move is O(N) — like std::array<double, N>
    static constexpr std::size_t kSize = 1'000;
    std::array<double, kSize> data{};

    HeavyMatrix() = default;

    HeavyMatrix(const HeavyMatrix&)     { /* copies kSize doubles */ }
    HeavyMatrix(HeavyMatrix&&) noexcept { /* ALSO copies kSize doubles — not cheap! */ }
    HeavyMatrix& operator=(const HeavyMatrix&)     = default;
    HeavyMatrix& operator=(HeavyMatrix&&) noexcept = default;
};

class ProcessorByValue {
public:
    void setMatrix(HeavyMatrix m) { matrix_ = std::move(m); }   // extra move
private:
    HeavyMatrix matrix_;
};

class ProcessorByRef {
public:
    void setMatrix(const HeavyMatrix& m) { matrix_ = m; }       // one copy, no extra move
    void setMatrix(HeavyMatrix&&      m) { matrix_ = std::move(m); }
private:
    HeavyMatrix matrix_;
};

void point5_cheap_to_move()
{
    std::cout << "\n=== Point 5: 'Cheap to move' condition ===\n";
    std::cout << "  HeavyMatrix move == copy (O(N)) — by-value causes extra O(N) move.\n";

    HeavyMatrix src;

    // Measure by-value
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 1'000; ++i) {
        ProcessorByValue p;
        p.setMatrix(src);           // 1 copy (param) + 1 move (into member) = 2×O(N)
    }
    auto t1 = std::chrono::steady_clock::now();

    // Measure by-ref overload
    auto t2 = std::chrono::steady_clock::now();
    for (int i = 0; i < 1'000; ++i) {
        ProcessorByRef p;
        p.setMatrix(src);           // 1 copy directly into member = 1×O(N)
    }
    auto t3 = std::chrono::steady_clock::now();

    using ms = std::chrono::duration<double, std::milli>;
    std::cout << "  by-value  time: " << ms(t1 - t0).count() << " ms\n";
    std::cout << "  by-ref    time: " << ms(t3 - t2).count() << " ms\n";
    std::cout << "  (by-value is slower because the extra 'move' is actually O(N))\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 6 — Move-only types: pass by value to transfer ownership.
//
// For std::unique_ptr the caller *must* move the argument; pass by value
// signals ownership transfer directly and cleanly.  This is NOT the "consider
// pass by value" scenario — it is a different, intentional design.
// ─────────────────────────────────────────────────────────────────────────────

class Connection {
public:
    explicit Connection(int id) : id_(id) {}
    int id() const { return id_; }
private:
    int id_;
};

class ConnectionPool {
public:
    // Takes ownership — pass by value is idiomatic for move-only types here.
    void add(std::unique_ptr<Connection> conn) {
        connections_.push_back(std::move(conn));
    }

    void printAll() const {
        for (const auto& c : connections_)
            std::cout << "  connection id=" << c->id() << '\n';
    }
private:
    std::vector<std::unique_ptr<Connection>> connections_;
};

void point6_move_only_types()
{
    std::cout << "\n=== Point 6: Move-only types — by-value signals ownership transfer ===\n";

    ConnectionPool pool;
    pool.add(std::make_unique<Connection>(1));
    pool.add(std::make_unique<Connection>(2));
    pool.add(std::make_unique<Connection>(3));

    // The caller *must* std::move — the parameter signature makes this explicit.
    auto c4 = std::make_unique<Connection>(4);
    pool.add(std::move(c4));

    std::cout << "  Pool contents after adding 4 connections:\n";
    pool.printAll();

    // c4 is now null — ownership was transferred
    std::cout << "  c4 is " << (c4 ? "valid" : "null (ownership transferred)") << '\n';
}

// ─────────────────────────────────────────────────────────────────────────────
// Point 7 — Slicing problem: pass by value slices derived objects.
//
// If the parameter type is a base class, passing by value copies only the
// base sub-object; virtual dispatch on the copy will dispatch to Base methods,
// which is almost certainly a bug.  Use (const Base&) or (Base&&) instead.
// ─────────────────────────────────────────────────────────────────────────────

struct Shape {
    virtual std::string name() const { return "Shape"; }
    virtual ~Shape() = default;
};

struct Circle : Shape {
    std::string name() const override { return "Circle"; }
};

struct Square : Shape {
    std::string name() const override { return "Square"; }
};

// BAD: silently slices the derived object
void printShapeByValue(Shape s) {
    std::cout << "  by-value sees: " << s.name() << '\n';   // always "Shape"
}

// GOOD: polymorphism preserved
void printShapeByRef(const Shape& s) {
    std::cout << "  by-ref   sees: " << s.name() << '\n';   // Circle / Square
}

void point7_slicing()
{
    std::cout << "\n=== Point 7: Slicing — by-value is wrong for base-class parameters ===\n";

    Circle c;
    Square sq;

    printShapeByValue(c);    // sliced → prints "Shape"
    printShapeByRef(c);      // prints "Circle"

    printShapeByValue(sq);   // sliced → prints "Shape"
    printShapeByRef(sq);     // prints "Square"

    std::cout << "  Conclusion: pass by value loses the derived type entirely.\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Summary helper — quick recap printed at runtime
// ─────────────────────────────────────────────────────────────────────────────

void printSummary()
{
    std::cout << "\n=== Summary: When to use pass by value ===\n"
              << "  USE pass by value when ALL of the following hold:\n"
              << "    (a) the parameter type is copyable\n"
              << "    (b) the move constructor is cheap (O(1))\n"
              << "    (c) the parameter is ALWAYS copied/moved into storage\n"
              << "    (d) the parameter type is not a base class (no slicing risk)\n"
              << "  AVOID pass by value when:\n"
              << "    - the copy is conditional (wasted construction on the rejected path)\n"
              << "    - the type is expensive to move (e.g., std::array<T,N>)\n"
              << "    - the type is a base class (slicing)\n"
              << "    - you need perfect forwarding over a large overload set\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main()
{
    point1_and_2_cost_comparison();
    point3_always_copied_condition();
    point4_extra_move_for_lvalue();
    point5_cheap_to_move();
    point6_move_only_types();
    point7_slicing();
    printSummary();

    return 0;
}
