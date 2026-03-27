// Item 25: Use std::move on rvalue references, std::forward on universal references
//
// Core rule:
//   - Rvalue reference parameters are ALWAYS bound to rvalues → use std::move (unconditional cast)
//   - Universal reference parameters may be lvalues OR rvalues → use std::forward (conditional cast)
//
// Applying the wrong cast causes correctness and/or performance bugs.

#include <iostream>
#include <string>
#include <utility>   // std::move, std::forward
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// PART 1 – POINT: Use std::move on rvalue references
//           An rvalue-reference parameter is always bound to an rvalue.
//           std::move unconditionally produces an rvalue, which is correct here.
// ─────────────────────────────────────────────────────────────────────────────

class Widget {
public:
    std::string name;
    std::vector<int> data;

    Widget() = default;

    // Explicit constructor so we can trace copies/moves
    Widget(std::string n, std::vector<int> d)
        : name(std::move(n)), data(std::move(d)) {}

    // Copy constructor
    Widget(const Widget& rhs) : name(rhs.name), data(rhs.data) {
        std::cout << "  [Widget copy-constructed: " << name << "]\n";
    }

    // Move constructor
    Widget(Widget&& rhs) noexcept : name(std::move(rhs.name)), data(std::move(rhs.data)) {
        std::cout << "  [Widget move-constructed: " << name << "]\n";
    }

    // Copy assignment
    Widget& operator=(const Widget& rhs) {
        name = rhs.name;
        data = rhs.data;
        std::cout << "  [Widget copy-assigned: " << name << "]\n";
        return *this;
    }

    // Move assignment
    Widget& operator=(Widget&& rhs) noexcept {
        name = std::move(rhs.name);
        data = std::move(rhs.data);
        std::cout << "  [Widget move-assigned: " << name << "]\n";
        return *this;
    }
};

// Correct: parameter is an rvalue reference → always use std::move
// The caller guarantees this won't be re-used after the call; it's safe to move from.
class Gadget {
public:
    Widget w;

    // rvalue reference parameter: use std::move so the Widget is moved, not copied
    void setWidget_correct(Widget&& rhs) {
        w = std::move(rhs);          // ✓ moves: cheap
    }

    // WRONG: forgetting std::move on rvalue reference → silently copies instead of moving
    void setWidget_wrong(Widget&& rhs) {
        w = rhs;                     // ✗ copies: expensive – rhs is an lvalue inside the body
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// PART 2 – POINT: Use std::forward on universal references
//           A universal (forwarding) reference may be bound to an lvalue or rvalue.
//           std::forward preserves the original value category.
// ─────────────────────────────────────────────────────────────────────────────

class DataStore {
public:
    Widget stored;

    // Universal reference: T&& where T is deduced
    // std::forward<T> casts to rvalue only when the caller passed an rvalue.
    template<typename T>
    void store_correct(T&& val) {
        stored = std::forward<T>(val);  // ✓ moves when rvalue, copies when lvalue
    }

    // WRONG: unconditionally moves from a universal reference
    template<typename T>
    void store_wrong(T&& val) {
        stored = std::move(val);        // ✗ silently moves from lvalues – leaves caller's
    }                                   //   object in a valid-but-unspecified state
};

// ─────────────────────────────────────────────────────────────────────────────
// PART 3 – POINT: Apply std::move / std::forward on the LAST USE of the parameter
//           Once you move from an object, it is in a valid-but-unspecified state.
//           Use the parameter normally beforehand; only cast on the final use.
// ─────────────────────────────────────────────────────────────────────────────

void logAndStore(std::vector<Widget>& sink, Widget&& w) {
    // ✓ Use the name BEFORE moving (safe – object still intact)
    std::cout << "  Logging widget name before move: " << w.name << "\n";

    // ✓ Move on the LAST use
    sink.push_back(std::move(w));

    // After here, w is in a moved-from state; don't use it further.
    // Uncommenting the next line would be a bug:
    // std::cout << w.name; // undefined behaviour in practice
}

template<typename T>
void logAndForward(std::vector<Widget>& sink, T&& w) {
    // ✓ Inspect before forwarding
    std::cout << "  Widget name before forward: " << w.name << "\n";

    // ✓ Forward on the last use
    sink.push_back(std::forward<T>(w));
}

// ─────────────────────────────────────────────────────────────────────────────
// PART 4 – POINT: Returning rvalue/universal references by value → move or forward
//           When a function returns an rvalue-reference or universal-reference parameter
//           by value, apply std::move or std::forward to enable move semantics.
//           Without it the compiler sees an lvalue expression and copies instead.
// ─────────────────────────────────────────────────────────────────────────────

// Returns Widget by value; parameter is rvalue reference
// Apply std::move so the return is a move, not a copy
Widget sink_rvalue(Widget&& w) {
    // std::move turns w (an lvalue-named rvalue-reference) back into an rvalue
    return std::move(w);   // ✓ moves
    // Without std::move: return w;  would invoke the copy constructor
}

// Returns Widget by value; parameter is universal reference
template<typename T>
Widget sink_universal(T&& w) {
    return std::forward<T>(w);  // ✓ moves when rvalue, copies when lvalue
}

// ─────────────────────────────────────────────────────────────────────────────
// PART 5 – POINT: Never apply std::move to local objects that are eligible for NRVO
//           Named Return Value Optimization (NRVO/RVO) allows the compiler to
//           construct the return value directly in the caller's stack frame,
//           eliminating any copy or move entirely.
//
//           Applying std::move to such a local object PREVENTS RVO because the
//           return expression is no longer the local object itself – it is a
//           reference to it, so the compiler cannot elide the move.
//
//           Result: using std::move on a local return value can be WORSE, not better.
//           The standard also mandates that if RVO is not performed, the local
//           is still treated as an rvalue for the return (implicit move), so
//           explicit std::move is redundant even in the non-elided case.
// ─────────────────────────────────────────────────────────────────────────────

Widget makeWidget_good() {
    Widget w{"good", {1, 2, 3}};
    // ✓ Return local object directly: compiler can apply NRVO (zero copies/moves)
    return w;
}

Widget makeWidget_bad() {
    Widget w{"bad", {4, 5, 6}};
    // ✗ Applying std::move prevents NRVO; forces a move even though elision was possible
    return std::move(w);
}

// The same rule applies to function parameters: they are NOT eligible for NRVO
// because they aren't local objects constructed in the function's stack frame.
// Here std::move IS appropriate.
Widget processAndReturn(Widget w) {          // w is a function parameter (not a local)
    // Do some processing …
    w.name += "_processed";
    // ✓ std::move is correct here; parameters are not RVO candidates
    return std::move(w);
}

// ─────────────────────────────────────────────────────────────────────────────
// PART 6 – Illustrating the asymmetry: move on rvalue ref vs forward on univ ref
//          with a realistic "emplacing setter" example used in the book
// ─────────────────────────────────────────────────────────────────────────────

class Message {
public:
    std::string text;

    Message() = default;
    explicit Message(std::string t) : text(std::move(t)) {}

    // ── Version A: overloaded for lvalue and rvalue (verbose, code duplication) ──
    void setText_overloaded(const std::string& t) {   // lvalue overload
        text = t;
        std::cout << "  setText: copy from lvalue\n";
    }
    void setText_overloaded(std::string&& t) {        // rvalue overload
        text = std::move(t);
        std::cout << "  setText: move from rvalue\n";
    }

    // ── Version B: single universal reference (preferred) ──
    template<typename T>
    void setText_forward(T&& t) {
        text = std::forward<T>(t);   // one template instead of two overloads
        std::cout << "  setText_forward called\n";
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// main – exercise every scenario
// ─────────────────────────────────────────────────────────────────────────────

int main() {
    // ── Part 1 demos ──────────────────────────────────────────────────────────
    std::cout << "=== Part 1: std::move on rvalue references ===\n";
    {
        Gadget g;
        Widget w1{"Alice", {1, 2}};
        Widget w2{"Bob",   {3, 4}};

        std::cout << "Correct (should move):\n";
        g.setWidget_correct(std::move(w1));   // move assignment

        std::cout << "Wrong (copies instead of moving):\n";
        g.setWidget_wrong(std::move(w2));     // copy assignment – w2 is still intact
    }

    // ── Part 2 demos ──────────────────────────────────────────────────────────
    std::cout << "\n=== Part 2: std::forward on universal references ===\n";
    {
        DataStore ds;
        Widget lv{"lvalue-widget", {10, 20}};

        std::cout << "store_correct with lvalue (should copy):\n";
        ds.store_correct(lv);                 // copy – lv is still usable

        std::cout << "lv.name after copy: " << lv.name << "\n";

        std::cout << "store_correct with rvalue (should move):\n";
        ds.store_correct(Widget{"rvalue-widget", {30, 40}});  // move

        std::cout << "store_wrong with lvalue (silently moves from lvalue!):\n";
        Widget lv2{"should-survive", {50}};
        ds.store_wrong(lv2);                  // moves from lv2 – bad!
        std::cout << "lv2.name after wrong move: \"" << lv2.name << "\" (moved-from)\n";
    }

    // ── Part 3 demos ──────────────────────────────────────────────────────────
    std::cout << "\n=== Part 3: Apply on the last use ===\n";
    {
        std::vector<Widget> sink;
        Widget w{"last-use-demo", {1}};
        logAndStore(sink, std::move(w));

        std::vector<Widget> sink2;
        Widget w2{"forward-last-use", {2}};
        logAndForward(sink2, std::move(w2));  // rvalue → forwarded as rvalue
        Widget w3{"forward-lvalue",   {3}};
        logAndForward(sink2, w3);             // lvalue → forwarded as lvalue (copy)
    }

    // ── Part 4 demos ──────────────────────────────────────────────────────────
    std::cout << "\n=== Part 4: Returning rvalue/universal references by value ===\n";
    {
        Widget source{"source", {99}};

        std::cout << "sink_rvalue (should move):\n";
        Widget result1 = sink_rvalue(std::move(source));
        std::cout << "result1.name: " << result1.name << "\n";

        Widget source2{"source2", {88}};
        std::cout << "sink_universal with rvalue (should move):\n";
        Widget result2 = sink_universal(std::move(source2));
        std::cout << "result2.name: " << result2.name << "\n";

        Widget source3{"source3", {77}};
        std::cout << "sink_universal with lvalue (should copy):\n";
        Widget result3 = sink_universal(source3);
        std::cout << "result3.name: " << result3.name
                  << ", source3.name still valid: " << source3.name << "\n";
    }

    // ── Part 5 demos ──────────────────────────────────────────────────────────
    std::cout << "\n=== Part 5: Do NOT move local objects eligible for NRVO ===\n";
    {
        // Both calls return a Widget; the difference is whether NRVO is possible.
        // With optimisations enabled (-O2) makeWidget_good will apply NRVO (0 copies/moves).
        // makeWidget_bad forces a move even at -O2 because std::move prevents elision.
        std::cout << "makeWidget_good (NRVO can apply – no move/copy printed at -O2):\n";
        Widget good = makeWidget_good();
        std::cout << "good.name: " << good.name << "\n";

        std::cout << "makeWidget_bad (std::move prevents NRVO – move printed):\n";
        Widget bad = makeWidget_bad();
        std::cout << "bad.name: " << bad.name << "\n";

        // Function parameter: std::move IS correct (not an RVO candidate)
        std::cout << "processAndReturn (parameter, std::move is correct):\n";
        Widget param{"param-widget", {1, 2, 3}};
        Widget processed = processAndReturn(std::move(param));
        std::cout << "processed.name: " << processed.name << "\n";
    }

    // ── Part 6 demos ──────────────────────────────────────────────────────────
    std::cout << "\n=== Part 6: Overloaded setters vs. single universal-reference setter ===\n";
    {
        Message m;
        std::string s = "hello";

        std::cout << "Overloaded – lvalue:\n";
        m.setText_overloaded(s);
        std::cout << "Overloaded – rvalue:\n";
        m.setText_overloaded(std::string("world"));

        std::cout << "Forward – lvalue:\n";
        m.setText_forward(s);
        std::cout << "Forward – rvalue:\n";
        m.setText_forward(std::string("earth"));
    }

    return 0;
}
