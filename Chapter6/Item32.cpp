/*
 * Item 32: Use init capture to move objects into closures.
 *
 * Key points covered:
 *  1. C++11 lambdas only support copy and reference capture — move-only types
 *     (e.g. std::unique_ptr, std::future) cannot be moved directly into a closure.
 *  2. C++14 introduces "init capture" (generalized lambda capture) which lets
 *     you specify both the data-member name inside the closure class and the
 *     initializing expression from the enclosing scope.
 *  3. The left side of the '=' in an init capture is in the closure class's
 *     scope; the right side is evaluated in the enclosing scope at lambda
 *     definition time.
 *  4. Any expression is valid on the right side — not just variable names.
 *  5. In C++11, move capture can be emulated with std::bind: bind an object
 *     to a helper lambda that receives it as a parameter.
 */

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <utility>
#include <algorithm>

// =============================================================================
// Helper class used in multiple examples
// =============================================================================
class Widget {
public:
    explicit Widget(std::string name) : name_(std::move(name)) {}

    bool isValidated() const { return true; }
    bool isProcessed() const { return true; }
    bool isArchived()  const { return false; }

    const std::string& name() const { return name_; }

private:
    std::string name_;
};

// =============================================================================
// POINT 1 — C++11 PROBLEM: move-only types cannot be captured directly.
//
// std::unique_ptr is move-only; copy-capture ([=]) doesn't compile and
// reference capture ([&]) is unsafe when the local goes out of scope.
// =============================================================================
void point1_cpp11_cannot_capture_move_only()
{
    std::cout << "\n--- Point 1: C++11 cannot capture move-only types ---\n";

    auto pw = std::make_unique<Widget>("Gadget");

    // The line below would NOT compile in C++11 because unique_ptr is not copyable:
    //   auto closure = [pw]{ return pw->isValidated(); };  // ERROR in C++11

    // Reference capture compiles but is dangerous – pw is destroyed when
    // point1_cpp11_cannot_capture_move_only() returns.
    auto closureRef = [&pw]{ return pw->isValidated(); };
    std::cout << "Validated (ref capture, OK while pw lives): "
              << closureRef() << "\n";

    // Using the closure after pw is reset → undefined behaviour:
    // pw.reset();
    // closureRef(); // ← UB: dangling reference

    std::cout << "C++11 has no direct way to move pw into the closure.\n";
}

// =============================================================================
// POINT 2 — C++14 SOLUTION: init capture moves a unique_ptr into the closure.
//
// Syntax:  [memberName = expression]
//   • "memberName" is the name of the data member inside the generated closure.
//   • "expression"  is evaluated in the enclosing scope at lambda definition.
// =============================================================================
void point2_init_capture_move_unique_ptr()
{
    std::cout << "\n--- Point 2: Init capture – moving unique_ptr into closure ---\n";

    auto pw = std::make_unique<Widget>("Gadget");

    // 'pw' on the right is the local unique_ptr; it is MOVED into 'pw' inside
    // the closure (the data member whose name is also 'pw', but the two 'pw's
    // live in different scopes).
    auto closure = [pw = std::move(pw)]() -> bool {
        return pw->isValidated() && pw->isProcessed();
    };

    // The local pw is now null — ownership transferred to the closure.
    std::cout << "Local pw is null after move: " << (pw == nullptr ? "yes" : "no") << "\n";
    std::cout << "Closure result: " << closure() << "\n";
}

// =============================================================================
// POINT 3 — Left side is in closure scope; right side is in enclosing scope.
//
// The member name inside the closure does NOT have to match the local name.
// Any valid expression can appear on the right of '='.
// =============================================================================
void point3_left_right_scopes()
{
    std::cout << "\n--- Point 3: Different names on left and right of init capture ---\n";

    auto pw = std::make_unique<Widget>("Gizmo");

    // Left side ("widget") becomes the data-member name inside the closure.
    // Right side ("std::move(pw)") is evaluated in this (enclosing) scope.
    auto closure = [widget = std::move(pw)](bool verbose) {
        if (verbose)
            std::cout << "Widget name: " << widget->name() << "\n";
        return widget->isValidated();
    };

    std::cout << "Local pw null: " << (pw == nullptr ? "yes" : "no") << "\n";
    closure(true);
}

// =============================================================================
// POINT 4 — Right side can be any expression, not just a variable name.
//
// Here we create the unique_ptr directly inside the init capture expression,
// so there is never a local variable at all — the object lives only in the
// closure from the moment of creation.
// =============================================================================
void point4_expression_on_right_side()
{
    std::cout << "\n--- Point 4: Arbitrary expression on right side of init capture ---\n";

    // make_unique is called as part of the capture — no intermediate variable.
    auto closure = [pw = std::make_unique<Widget>("Thingamajig")]() {
        std::cout << "Widget: " << pw->name() << "\n";
        return pw->isValidated() && !pw->isArchived();
    };

    std::cout << "Closure result: " << closure() << "\n";

    // Another example: capture a computed value.
    int base = 10;
    auto addBase = [sum = base * 2](int x) { return sum + x; };
    std::cout << "10*2 + 5 = " << addBase(5) << "\n";
}

// =============================================================================
// POINT 5 — C++11 WORKAROUND: emulate move capture with std::bind.
//
// Strategy:
//   a) Move the object into a std::bind call object.
//   b) Give the lambda a reference parameter so it receives the stored copy.
//
// The bind object holds the moved-in object for as long as the resulting
// callable exists — the same lifetime guarantee that init capture provides.
// =============================================================================
void point5_cpp11_bind_emulates_move_capture()
{
    std::cout << "\n--- Point 5: C++11 workaround – move into std::bind ---\n";

    auto pw = std::make_unique<Widget>("Legacy");

    // std::bind moves 'pw' into the bind object.
    // The inner lambda receives a const reference to the stored unique_ptr.
    auto boundClosure = std::bind(
        [](const std::unique_ptr<Widget>& w) {          // lambda: param = stored object
            return w->isValidated() && w->isProcessed();
        },
        std::move(pw)                                    // move pw into the bind object
    );

    std::cout << "Local pw null after bind: " << (pw == nullptr ? "yes" : "no") << "\n";
    std::cout << "Bound closure result: " << boundClosure() << "\n";

    // NOTE: the resulting callable is NOT a real lambda — it is a std::bind
    // expression — but it behaves identically: the Widget is owned by the
    // bind object and lives as long as 'boundClosure' does.
}

// =============================================================================
// POINT 5b — C++11 WORKAROUND: mutable lambda version with std::bind.
//
// If the lambda needs to MODIFY the captured object (i.e. would need a non-const
// member in C++14 init capture), drop the 'const' from the parameter.
// =============================================================================
void point5b_cpp11_bind_mutable()
{
    std::cout << "\n--- Point 5b: C++11 bind workaround for mutable closures ---\n";

    std::vector<int> data{1, 2, 3, 4, 5};

    // C++14 equivalent would be:
    //   auto closure = [data = std::move(data)]() mutable { data.push_back(6); ... };

    // C++11 workaround: non-const reference parameter allows mutation.
    auto closure = std::bind(
        [](std::vector<int>& d) {       // non-const: can mutate
            d.push_back(6);
            for (int v : d) std::cout << v << " ";
            std::cout << "\n";
        },
        std::move(data)
    );

    std::cout << "Local data empty after move: " << (data.empty() ? "yes" : "no") << "\n";
    closure();
}

// =============================================================================
// BONUS — Comparing C++11 bind workaround vs C++14 init capture side-by-side.
// =============================================================================
void bonus_side_by_side_comparison()
{
    std::cout << "\n--- Bonus: C++11 bind vs C++14 init capture side-by-side ---\n";

    // ---- C++14 init capture (preferred) ----
    {
        auto data = std::make_unique<Widget>("Modern");
        auto modern = [w = std::move(data)] {
            std::cout << "[C++14] " << w->name() << " validated: " << w->isValidated() << "\n";
        };
        modern();
    }

    // ---- C++11 std::bind emulation ----
    {
        auto data = std::make_unique<Widget>("Legacy");
        auto legacy = std::bind(
            [](const std::unique_ptr<Widget>& w) {
                std::cout << "[C++11] " << w->name() << " validated: " << w->isValidated() << "\n";
            },
            std::move(data)
        );
        legacy();
    }
}

// =============================================================================
// main
// =============================================================================
int main()
{
    point1_cpp11_cannot_capture_move_only();
    point2_init_capture_move_unique_ptr();
    point3_left_right_scopes();
    point4_expression_on_right_side();
    point5_cpp11_bind_emulates_move_capture();
    point5b_cpp11_bind_mutable();
    bonus_side_by_side_comparison();

    return 0;
}
