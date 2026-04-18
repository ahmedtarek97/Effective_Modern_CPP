// Item 29: Assume that move operations are not present, not cheap, and not used.
//
// Move semantics can be a significant performance win, but several real-world
// situations prevent you from benefiting from them. This item shows each case.

#include <array>
#include <chrono>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

// ============================================================
// CASE 1: No move operations — the class was written before C++11
// and defines a copy constructor/assignment, which suppresses
// compiler-generated move operations.
// ============================================================

class OldStyleWidget {
public:
    OldStyleWidget() : data_(new int[1024]{}) {}

    // User-declared copy constructor suppresses move operations.
    OldStyleWidget(const OldStyleWidget& other) : data_(new int[1024]{}) {
        std::copy(other.data_, other.data_ + 1024, data_);
        std::cout << "  OldStyleWidget: COPY constructor\n";
    }

    OldStyleWidget& operator=(const OldStyleWidget& other) {
        if (this != &other)
            std::copy(other.data_, other.data_ + 1024, data_);
        std::cout << "  OldStyleWidget: COPY assignment\n";
        return *this;
    }

    ~OldStyleWidget() { delete[] data_; }

private:
    int* data_;
};

void demo_no_move_operations() {
    std::cout << "=== CASE 1: No move operations ===\n";

    OldStyleWidget a;
    // std::move merely casts to rvalue, but because there is no move
    // constructor, the compiler falls back to the copy constructor.
    OldStyleWidget b(std::move(a));   // calls COPY ctor, not move ctor
    OldStyleWidget c;
    c = std::move(b);                 // calls COPY assignment, not move assignment
    std::cout << '\n';
}

// ============================================================
// CASE 2: Move is NOT cheaper than copy — std::array
//
// std::array stores its data inline (no heap pointer to steal).
// Moving an array copies every element just like a copy would.
// Contrast with std::vector, where a move is O(1).
// ============================================================

void demo_array_move_not_cheap() {
    std::cout << "=== CASE 2: std::array — move is O(n), same as copy ===\n";

    constexpr std::size_t N = 5;
    std::array<int, N> src = {1, 2, 3, 4, 5};

    // Move-constructing an array still copies every element because the
    // data lives inside the array object itself (no heap pointer to steal).
    std::array<int, N> dst(std::move(src));

    std::cout << "  dst: ";
    for (int v : dst) std::cout << v << ' ';
    std::cout << '\n';

    // Compare with std::vector: move is O(1) — only the internal pointer,
    // size, and capacity are transferred.
    std::vector<int> vsrc = {1, 2, 3, 4, 5};
    std::vector<int> vdst(std::move(vsrc)); // O(1) pointer swap
    std::cout << "  vector move: vsrc.size()=" << vsrc.size()
              << ", vdst.size()=" << vdst.size() << '\n';
    std::cout << '\n';
}

// ============================================================
// CASE 3: const objects cannot be moved from
//
// Move constructors and move assignment operators take non-const
// rvalue references (T&&).  A const object cannot bind to T&&,
// so the compiler silently falls back to copy.
// ============================================================

struct Widget {
    Widget() = default;
    Widget(const Widget&)            { std::cout << "  Widget: COPY ctor\n"; }
    Widget(Widget&&) noexcept        { std::cout << "  Widget: MOVE ctor\n"; }
    Widget& operator=(const Widget&) { std::cout << "  Widget: COPY assignment\n"; return *this; }
    Widget& operator=(Widget&&) noexcept { std::cout << "  Widget: MOVE assignment\n"; return *this; }
};

void demo_const_no_move() {
    std::cout << "=== CASE 3: const objects cannot be moved from ===\n";

    const Widget cw;          // const — move ctor cannot bind to const Widget&&
    Widget w1(std::move(cw)); // std::move(cw) yields const Widget&&,
                              // which matches const Widget& (copy ctor) better
                              // than Widget&& (move ctor).
    std::cout << '\n';
}

// ============================================================
// CASE 4: Move not used because it is not declared noexcept,
// and the calling code requires a strong exception guarantee.
//
// std::vector::push_back (and resize) will only move elements
// during reallocation if the move constructor is noexcept.
// Otherwise it copies to preserve the strong exception guarantee.
// ============================================================

struct MoveNotNoexcept {
    MoveNotNoexcept() = default;
    MoveNotNoexcept(const MoveNotNoexcept&)       { std::cout << "  MoveNotNoexcept: COPY\n"; }
    MoveNotNoexcept(MoveNotNoexcept&&) /* no noexcept */ { std::cout << "  MoveNotNoexcept: MOVE\n"; }
};

struct MoveIsNoexcept {
    MoveIsNoexcept() = default;
    MoveIsNoexcept(const MoveIsNoexcept&)            { std::cout << "  MoveIsNoexcept: COPY\n"; }
    MoveIsNoexcept(MoveIsNoexcept&&) noexcept        { std::cout << "  MoveIsNoexcept: MOVE\n"; }
};

void demo_noexcept_required() {
    std::cout << "=== CASE 4: Move not used — move ctor not noexcept ===\n";

    // Force reallocation by exceeding capacity.
    {
        std::cout << "  Without noexcept (vector uses COPY during reallocation):\n";
        std::vector<MoveNotNoexcept> v;
        v.reserve(1);
        v.emplace_back();  // fills capacity
        v.emplace_back();  // triggers reallocation — existing element is COPIED
    }

    {
        std::cout << "  With noexcept (vector uses MOVE during reallocation):\n";
        std::vector<MoveIsNoexcept> v;
        v.reserve(1);
        v.emplace_back();  // fills capacity
        v.emplace_back();  // triggers reallocation — existing element is MOVED
    }
    std::cout << '\n';
}

// ============================================================
// CASE 5: Small String Optimization (SSO)
//
// Most std::string implementations store short strings in an
// internal buffer (typically ≤15 chars on 64-bit platforms).
// Because no heap memory is involved, moving a short string
// requires copying the characters — it is not O(1).
// Long strings use heap allocation, so their moves are O(1).
// ============================================================

void demo_sso() {
    std::cout << "=== CASE 5: Small String Optimization (SSO) ===\n";

    std::string small = "hi";                        // fits in SSO buffer
    std::string large(64, 'x');                      // definitely on heap

    std::cout << "  small string size: " << small.size() << '\n';
    std::cout << "  large string size: " << large.size() << '\n';

    // Both moves compile fine, but for 'small' the characters are
    // copied internally — no heap pointer to steal.
    std::string moved_small(std::move(small));
    std::string moved_large(std::move(large));

    // After the move, large is empty (heap pointer stolen), but
    // for small the standard makes no such guarantee of O(1) cost.
    std::cout << "  After move: large.size()=" << large.size()
              << " (emptied, heap pointer was stolen)\n";
    std::cout << "  After move: small.size()=" << small.size()
              << " (implementation-defined; SSO chars copied)\n";
    std::cout << '\n';
}

// ============================================================
// CASE 6: Generic code cannot assume moves are cheap
//
// A template that forwards or moves an arbitrary T must be
// written defensively — T might be any of the types above.
// ============================================================

template <typename T>
T processAndReturn(T obj) {
    // Do work on obj …
    return obj; // NRVO or move — but if T has no cheap move, this copies.
}

template <typename Container>
void fillContainer(Container& c, typename Container::value_type val, std::size_t n) {
    // Cannot assume push_back/emplace_back uses cheap moves when
    // the container needs to reallocate.
    for (std::size_t i = 0; i < n; ++i)
        c.push_back(val);
}

void demo_generic_code() {
    std::cout << "=== CASE 6: Generic code — cannot assume cheap moves ===\n";

    // Works correctly regardless of whether T's move is cheap.
    std::vector<int>    vi;
    std::vector<Widget> vw;

    fillContainer(vi, 42,     5);
    std::cout << "  int vector filled: " << vi.size() << " elements\n";

    // fillContainer uses Widget's copy/move depending on noexcept status
    // (Widget move IS noexcept here, so vector will use it).
    fillContainer(vw, Widget{}, 2);
    std::cout << '\n';
}

// ============================================================
// main
// ============================================================

int main() {
    demo_no_move_operations();
    demo_array_move_not_cheap();
    demo_const_no_move();
    demo_noexcept_required();
    demo_sso();
    demo_generic_code();

    std::cout << "Key takeaway: move semantics are not a silver bullet.\n"
              << "Write code that is correct even when moves are absent,\n"
              << "expensive, or silently replaced by copies.\n";
    return 0;
}
