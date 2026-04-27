// Item 34: Prefer lambdas to std::bind
//
// Key Points:
//  1. Lambdas are more readable than std::bind: argument roles are obvious from context.
//  2. std::bind evaluates all arguments at bind-call time (stored copies), not at
//     invocation time. Lambdas evaluate their body at invocation time.
//  3. std::bind always copies its arguments into the bind object. To pass by reference
//     you must wrap with std::ref() / std::cref(), which is easy to forget.
//     With lambdas, [&x] vs [x] is explicit and hard to get wrong.
//  4. Overloaded functions are ambiguous with std::bind — a manual cast to the
//     correct function-pointer type is required. Lambdas call the right overload
//     naturally from the call expression inside the body.
//  5. Nested std::bind calls become nearly unreadable; nested lambdas are clear.
//  6. In C++14, lambdas with auto parameters are polymorphic (generic), so the
//     one remaining C++11 advantage of std::bind (accepting arbitrary types without
//     a template) is gone. Prefer lambdas in all cases.

#include <iostream>
#include <functional>   // std::bind, std::ref, std::cref, std::placeholders
#include <algorithm>    // std::find_if
#include <chrono>
#include <string>
#include <vector>
#include <cassert>

using namespace std::placeholders;  // _1, _2, ...
using namespace std::chrono_literals;

// ============================================================================
// Point 1 — Readability
// Suppose we have a function that checks whether a value is in [lo, hi].
// ============================================================================

bool isBetween(int lo, int hi, int value)
{
    return lo <= value && value <= hi;
}

void demo_readability()
{
    std::cout << "\n--- Point 1: Readability ---\n";

    // std::bind version — which argument is _1? You have to mentally match
    // placeholders to the parameter list of isBetween.
    auto inRangeWithBind = std::bind(isBetween, 10, 20, _1);

    // Lambda version — immediately obvious what is happening.
    auto inRangeWithLambda = [](int value) { return isBetween(10, 20, value); };

    std::cout << "  bind  - is 15 in [10,20]? " << std::boolalpha << inRangeWithBind(15)  << '\n';
    std::cout << "  lambda- is 15 in [10,20]? " << std::boolalpha << inRangeWithLambda(15) << '\n';
    std::cout << "  bind  - is 25 in [10,20]? " << std::boolalpha << inRangeWithBind(25)  << '\n';
    std::cout << "  lambda- is 25 in [10,20]? " << std::boolalpha << inRangeWithLambda(25) << '\n';
}

// ============================================================================
// Point 2 — Argument evaluation timing
//
// std::bind copies all non-placeholder arguments eagerly (at bind time).
// If you pass a time-point to represent "now + 1 hour", std::bind captures
// the value of "now" when bind() is called, not when the resulting callable
// is eventually invoked.
// ============================================================================

using SteadyClock = std::chrono::steady_clock;
using TimePoint   = SteadyClock::time_point;
using Duration    = std::chrono::seconds;

// Simulates setting an alarm: logs the scheduled time.
void setAlarm(TimePoint t, const std::string& sound, Duration d)
{
    std::cout << "  Alarm set: sound=" << sound
              << ", duration=" << d.count() << "s"
              << ", time offset from epoch=" << t.time_since_epoch().count() << '\n';
}

void demo_argument_evaluation_timing()
{
    std::cout << "\n--- Point 2: Argument evaluation timing ---\n";

    // --- std::bind version ---
    // std::chrono::steady_clock::now() + 1h is evaluated HERE, at bind time,
    // not when the resulting object is later called.
    auto setSoundB = std::bind(setAlarm,
                               SteadyClock::now() + 1h,  // evaluated NOW
                               _1,
                               30s);

    // --- Lambda version ---
    // The now() + 1h expression is inside the lambda body, so it is evaluated
    // each time the lambda is called — correctly capturing "one hour from now
    // at the time of the call".
    auto setSoundL = [](const std::string& sound) {
        setAlarm(SteadyClock::now() + 1h,   // evaluated at call time
                 sound,
                 30s);
    };

    std::cout << "  calling bind version:\n";
    setSoundB("Chimes");

    std::cout << "  calling lambda version:\n";
    setSoundL("Chimes");
}

// ============================================================================
// Point 3 — Pass-by-value vs pass-by-reference clarity
//
// std::bind copies by default. Using std::ref makes the bind object hold a
// live reference, but this is easy to miss at a glance.
// Lambda capture syntax ([&x] / [x]) makes the intent unmistakable.
// ============================================================================

void append(std::string& dest, const std::string& suffix)
{
    dest += suffix;
}

void demo_value_vs_reference()
{
    std::cout << "\n--- Point 3: Value vs reference clarity ---\n";

    std::string s1 = "Hello";
    std::string s2 = "Hello";

    // std::bind: dest is COPIED unless you wrap with std::ref.
    // The copy is silent — easy to introduce a bug.
    auto appendViaBind_copy = std::bind(append, s1, ", World");
    appendViaBind_copy();
    std::cout << "  bind by copy: s1 = \"" << s1 << "\" (unchanged)\n";

    // Now with std::ref — the live string is modified.
    auto appendViaBind_ref  = std::bind(append, std::ref(s1), "!!!");
    appendViaBind_ref();
    std::cout << "  bind by ref:  s1 = \"" << s1 << "\" (modified)\n";

    // Lambda: capture-by-reference [&s2] makes intent crystal-clear.
    auto appendViaLambda = [&s2](const std::string& suffix) {
        append(s2, suffix);
    };
    appendViaLambda(", World");
    std::cout << "  lambda [&]:   s2 = \"" << s2 << "\" (modified)\n";
}

// ============================================================================
// Point 4 — Overloaded functions are ambiguous with std::bind
//
// std::bind cannot resolve an overloaded name on its own; you must cast to the
// exact function-pointer type, which is verbose and fragile.
// Lambdas simply call the function by name and let the compiler pick the right
// overload from the argument types at the call site — no casting needed.
// ============================================================================

// Two overloads of the same name:
void logMessage(const std::string& msg)
{
    std::cout << "  [string overload] " << msg << '\n';
}

void logMessage(int code)
{
    std::cout << "  [int overload]    code=" << code << '\n';
}

void demo_overloads()
{
    std::cout << "\n--- Point 4: Overloaded functions ---\n";

    // std::bind requires an explicit cast to resolve the overload.
    using LogStringFn = void(*)(const std::string&);
    auto bindLog = std::bind(static_cast<LogStringFn>(logMessage), _1);

    // Lambda: compiler resolves the overload naturally.
    auto lambdaLogStr = [](const std::string& msg) { logMessage(msg); };
    auto lambdaLogInt = [](int code)                { logMessage(code); };

    std::cout << "  via bind (explicit cast):\n";
    bindLog("bound call");

    std::cout << "  via lambdas (natural overload resolution):\n";
    lambdaLogStr("lambda string call");
    lambdaLogInt(42);
}

// ============================================================================
// Point 5 — Nested std::bind is nearly unreadable
//
// If you need to compose two or more bound functions, std::bind nesting grows
// unwieldy. A lambda that calls another lambda (or function) stays readable.
// ============================================================================

bool isEven(int n) { return n % 2 == 0;  }
bool isOdd (int n) { return n % 2 != 0;  }

// Negates the result of a predicate.
bool negate(bool b) { return !b; }

void demo_nested_bind()
{
    std::cout << "\n--- Point 5: Nested std::bind vs lambdas ---\n";

    // Nested bind: bind(negate, bind(isEven, _1)) — "not-even"
    // Even experienced programmers need a moment to parse this.
    auto notEvenBind = std::bind(negate, std::bind(isEven, _1));

    // Lambda equivalent: immediately obvious.
    auto notEvenLambda = [](int n) { return !isEven(n); };

    std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8};

    std::cout << "  Odd numbers (bind  ): ";
    for (int n : numbers) if (notEvenBind(n))   std::cout << n << ' ';
    std::cout << '\n';

    std::cout << "  Odd numbers (lambda): ";
    for (int n : numbers) if (notEvenLambda(n)) std::cout << n << ' ';
    std::cout << '\n';
}

// ============================================================================
// Point 6 — In C++14, auto-parameter lambdas replace bind's last advantage
//
// In C++11, std::bind produced a polymorphic callable: the bound argument could
// accept different types on each call (for non-placeholder arguments that are
// move-only, or for operator() that is itself a template).
//
// In C++14, a lambda with auto parameters is genuinely generic and covers every
// use case that motivated std::bind in C++11.
// ============================================================================

// A callable object whose operator() is a template (polymorphic function object).
struct Multiply
{
    template<typename T, typename U>
    auto operator()(T a, U b) const { return a * b; }
};

void demo_polymorphic()
{
    std::cout << "\n--- Point 6: C++14 generic (auto-parameter) lambdas ---\n";

    // std::bind with a polymorphic functor — the call can deduce different types.
    auto bindMul = std::bind(Multiply{}, _1, _2);

    // C++14 auto-parameter lambda — equally polymorphic, and more readable.
    auto lambdaMul = [](auto a, auto b) { return a * b; };

    std::cout << "  bind:   3   * 4   = " << bindMul(3,   4)   << '\n';
    std::cout << "  bind:   2.5 * 3.0 = " << bindMul(2.5, 3.0) << '\n';
    std::cout << "  lambda: 3   * 4   = " << lambdaMul(3,   4)   << '\n';
    std::cout << "  lambda: 2.5 * 3.0 = " << lambdaMul(2.5, 3.0) << '\n';
}

// ============================================================================
// Point 6 (C++14 init-capture) — Move-only types into closures
//
// In C++11, moving an object into a closure was impossible with a plain lambda;
// std::bind was used as a workaround. C++14 init-capture eliminates that need.
// ============================================================================

void demo_move_capture()
{
    std::cout << "\n--- Point 6b: Move-capture (C++14 init-capture) ---\n";

    auto makeLargeString = []() {
        return std::string(50, 'X');
    };

    // C++14: move a string directly into the lambda via init-capture.
    // No need for std::bind anymore.
    std::string largeStr = makeLargeString();
    auto lambdaWithMove = [s = std::move(largeStr)]() {
        std::cout << "  lambda holds moved string (first 10 chars): "
                  << s.substr(0, 10) << "...\n";
    };

    std::cout << "  original string after move, empty? "
              << std::boolalpha << largeStr.empty() << '\n';
    lambdaWithMove();

    // The std::bind workaround from C++11 for comparison (kept as comment
    // since C++14 makes it unnecessary, but shown for illustration):
    //
    //   auto bindWithMove = std::bind(
    //       [](const std::string& s) { /* use s */ },
    //       std::move(someStr));
    //
    // The C++14 lambda above is clearly the better choice.
}

// ============================================================================
// main
// ============================================================================

int main()
{
    demo_readability();
    demo_argument_evaluation_timing();
    demo_value_vs_reference();
    demo_overloads();
    demo_nested_bind();
    demo_polymorphic();
    demo_move_capture();
    return 0;
}
