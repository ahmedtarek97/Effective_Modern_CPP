// Item 33: Use decltype on auto&& parameters to std::forward them
//
// Key Points:
//  1. Generic lambdas (C++14) can take auto parameters, which deduce types like templates.
//  2. To perfectly forward an auto&& parameter, you cannot use std::forward<T> directly
//     because there is no explicit template parameter T — you must use
//     std::forward<decltype(param)>(param).
//  3. decltype on an lvalue auto&& gives T& (reference), and on an rvalue gives T&&,
//     which is exactly what std::forward needs to preserve value category.
//  4. This pattern enables generic lambdas to be perfectly forwarding equivalents
//     of variadic function templates.

#include <iostream>
#include <utility>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Helper: reports whether an argument was received as lvalue or rvalue ref
// ---------------------------------------------------------------------------
void process(const std::string& s)
{
    std::cout << "  process(lvalue): " << s << '\n';
}

void process(std::string&& s)
{
    std::cout << "  process(rvalue): " << s << '\n';
}

// ---------------------------------------------------------------------------
// Point 1 & 4 — templated function equivalent (for comparison)
// A classic perfectly-forwarding template function:
// ---------------------------------------------------------------------------
template<typename T>
void classicForward(T&& arg)
{
    process(std::forward<T>(arg));
}

// ---------------------------------------------------------------------------
// Point 1 — Generic lambda with auto parameter
// auto parameters in lambdas behave exactly like template type parameters.
// Each call to the lambda can deduce a different type for 'x'.
// ---------------------------------------------------------------------------
void demo_auto_lambda()
{
    std::cout << "\n--- Point 1: Generic lambda (auto parameter) ---\n";

    auto printType = [](auto x) {
        // x is a copy; passing by value — NOT perfect forwarding yet
        std::cout << "  value: " << x << '\n';
    };

    printType(42);
    printType(std::string("hello"));
    printType(3.14);
}

// ---------------------------------------------------------------------------
// Point 2 — Naive attempt: cannot write std::forward<T> in a lambda
// because there is no named template parameter T.
// The solution is std::forward<decltype(param)>(param).
// ---------------------------------------------------------------------------
void demo_forward_with_decltype()
{
    std::cout << "\n--- Point 2 & 3: std::forward<decltype(x)>(x) ---\n";

    // This lambda is the generic-lambda equivalent of classicForward<T>.
    auto perfectForwardLambda = [](auto&& x) {
        // std::forward<T>(x) is impossible here — no T.
        // Use decltype(x) to recover the deduced type *with* its reference
        // qualifier, which is exactly what std::forward requires.
        process(std::forward<decltype(x)>(x));
    };

    std::string s = "world";

    std::cout << "  Calling with lvalue std::string:\n";
    perfectForwardLambda(s);                   // lvalue → calls process(const string&)

    std::cout << "  Calling with rvalue std::string:\n";
    perfectForwardLambda(std::string("moved")); // rvalue → calls process(string&&)

    std::cout << "  Calling with string literal (deduced as const char*):\n";
    perfectForwardLambda("literal");            // deduced as const char*
}

// ---------------------------------------------------------------------------
// Point 3 — Why decltype gives the right type for std::forward
//
// Reference-collapsing rules recap:
//   auto&& x = lvalue  → x has type T&   (decltype(x) == T&)
//   auto&& x = rvalue  → x has type T&&  (decltype(x) == T&&)
//
// std::forward<T&>(x)  returns T&  (lvalue) — correct
// std::forward<T&&>(x) returns T&& (rvalue) — correct
// ---------------------------------------------------------------------------
void demo_decltype_behavior()
{
    std::cout << "\n--- Point 3: decltype preserves value category ---\n";

    // Demonstrate what decltype resolves to for auto&&
    // Use string so it matches the process() overloads.
    auto showCategory = [](auto&& x) {
        // If x is lvalue ref  : is_lvalue_reference is true
        // If x is rvalue ref  : is_rvalue_reference is true
        constexpr bool isLvalueRef  = std::is_lvalue_reference<decltype(x)>::value;
        constexpr bool isRvalueRef  = std::is_rvalue_reference<decltype(x)>::value;

        std::cout << "  lvalue_reference: " << std::boolalpha << isLvalueRef
                  << ",  rvalue_reference: " << isRvalueRef << '\n';

        // Perfect-forward using the decltype idiom
        process(std::forward<decltype(x)>(x));
    };

    std::string lv = "lvalue-string";

    std::cout << "  Pass lvalue string:\n";
    showCategory(lv);

    std::cout << "  Pass rvalue string:\n";
    showCategory(std::string("rvalue-string"));
}

// ---------------------------------------------------------------------------
// Point 4 — Variadic generic lambda for perfect forwarding multiple args
// Extends the single-argument pattern to variadic lambdas.
// ---------------------------------------------------------------------------
void multiProcess(const std::string& a, std::string&& b)
{
    std::cout << "  multiProcess(lvalue: " << a << ", rvalue: " << b << ")\n";
}

void demo_variadic_lambda()
{
    std::cout << "\n--- Point 4: Variadic generic lambda ---\n";

    // Variadic auto&& + fold-expression-style forwarding (each arg independently)
    auto callMulti = [](auto&&... args) {
        multiProcess(std::forward<decltype(args)>(args)...);
    };

    std::string a = "alpha";
    callMulti(a, std::string("beta"));   // a is lvalue, "beta" is rvalue
}

// ---------------------------------------------------------------------------
// Point 4 — Wrapping an arbitrary callable (like std::bind equivalent)
// A practical use-case: a generic lambda that perfectly forwards all its
// arguments to an underlying callable.
// ---------------------------------------------------------------------------
void demo_wrapper_lambda()
{
    std::cout << "\n--- Point 4: Perfect-forwarding dispatcher lambda ---\n";

    // A generic dispatcher: captures a callable and perfectly forwards
    // all call arguments to it.  The callable itself is also perfectly
    // forwarded into the inner lambda's capture via decltype.
    auto makeDispatcher = [](auto&& func) {
        // Store the callable by value (after forwarding into the capture).
        // Using a value capture avoids dangling-reference issues.
        return [f = std::forward<decltype(func)>(func)](auto&&... args) mutable {
            return f(std::forward<decltype(args)>(args)...);
        };
    };

    // Wrap process() via a generic lambda that accepts a string argument
    auto wrappedProcess = [](auto&& s) {
        process(std::forward<decltype(s)>(s));
    };

    auto dispatcher = makeDispatcher(wrappedProcess);

    std::string s = "dispatched";
    std::cout << "  Via dispatcher with lvalue:\n";
    dispatcher(s);

    std::cout << "  Via dispatcher with rvalue:\n";
    dispatcher(std::string("rvalue dispatched"));
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main()
{
    demo_auto_lambda();
    demo_forward_with_decltype();
    demo_decltype_behavior();
    demo_variadic_lambda();
    demo_wrapper_lambda();

    return 0;
}
