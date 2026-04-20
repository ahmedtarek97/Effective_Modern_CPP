// =============================================================================
// Item 30: Familiarize yourself with perfect forwarding failure cases
//
// Perfect forwarding fails when template type deduction fails or deduces the
// wrong type for the forwarded argument. The five main failure cases are:
//   1. Braced initializers
//   2. 0 or NULL as null pointers
//   3. Declaration-only integral static const data members
//   4. Overloaded function names and function template names
//   5. Bitfields
// =============================================================================

#include <iostream>
#include <vector>
#include <cstdint>   // std::uint32_t, std::uint16_t
#include <cstddef>   // std::size_t, NULL
#include <utility>   // std::forward

// =============================================================================
// CASE 1: Braced Initializers
// =============================================================================
// A braced initializer ({1, 2, 3}) is a "non-deduced context" inside a
// forwarding template. The compiler cannot infer std::initializer_list<int>
// from it because the std::initializer_list type is never mentioned in the
// function template's parameter list. The direct call works because the
// function's declared parameter type provides the conversion context.

void takeVector(const std::vector<int>& v)
{
    std::cout << "takeVector: size = " << v.size() << "\n";
}

template<typename T>
void fwdToVector(T&& param)
{
    takeVector(std::forward<T>(param));
}

void case1_braced_initializers()
{
    std::cout << "\n=== Case 1: Braced Initializers ===\n";

    // Direct call — compiler constructs std::vector<int> from {1,2,3} directly
    takeVector({1, 2, 3});

    // fwdToVector({1, 2, 3});
    //   COMPILE ERROR: template type deduction cannot deduce T from a
    //   braced-init-list; there is no declared std::initializer_list parameter.

    // Workaround: use 'auto' to capture the initializer first.
    // 'auto' is special: it DOES deduce std::initializer_list<int> from {…}.
    auto il = {1, 2, 3};  // il has type std::initializer_list<int>
    fwdToVector(il);       // T is now deduced as std::initializer_list<int> — OK
    std::cout << "Workaround: auto il = {1,2,3}; fwdToVector(il);\n";
}

// =============================================================================
// CASE 2: 0 or NULL as Null Pointers
// =============================================================================
// When 0 or NULL is passed to a forwarding template, the deduced type is an
// integral type (int or long), NOT a pointer type. The wrapper then tries to
// forward an integer to a function that expects a pointer — compile error.

void takeIntPtr(int* ptr)
{
    std::cout << "takeIntPtr: " << (ptr ? "non-null" : "null") << "\n";
}

template<typename T>
void fwdToIntPtr(T&& param)
{
    takeIntPtr(std::forward<T>(param));
}

void case2_null_pointers()
{
    std::cout << "\n=== Case 2: 0 or NULL as Null Pointers ===\n";

    // Direct calls — 0 and NULL implicitly convert to int* at the call site
    takeIntPtr(0);
    takeIntPtr(NULL);
    takeIntPtr(nullptr);

    // fwdToIntPtr(0);
    //   COMPILE ERROR: T is deduced as int, but takeIntPtr expects int*.

    // fwdToIntPtr(NULL);
    //   COMPILE ERROR: T is deduced as long (or int), not int*.

    // Workaround: use nullptr. Its type is std::nullptr_t, which converts to
    // any pointer or pointer-to-member type — and it forwards correctly.
    fwdToIntPtr(nullptr);
    std::cout << "Workaround: use nullptr in place of 0 or NULL.\n";
}

// =============================================================================
// CASE 3: Declaration-only integral static const data members
// =============================================================================
// A static const integral member with an in-class initializer can be used as
// an integer constant expression (the compiler substitutes the literal).
// BUT perfect forwarding internally binds a reference to the argument, which
// ODR-uses the member and requires a real address (i.e., an out-of-class
// definition). Without that definition, the linker cannot resolve the symbol.

class Widget {
public:
    static const std::size_t MinVals = 28; // declaration + in-class initializer
};

// Out-of-class definition (exactly one translation unit must provide this).
// Without it, fwdToTakeSize(Widget::MinVals) compiles but causes a LINKER
// ERROR because the reference binding in the template requires an address.
// Note: In C++17, inline static const members don't need this, but in
// C++11/14 (the book's target) this definition is required for ODR-use.
const std::size_t Widget::MinVals; // no initializer here; it lives in the class

void takeSize(std::size_t n)
{
    std::cout << "takeSize: " << n << "\n";
}

template<typename T>
void fwdToTakeSize(T&& param)
{
    takeSize(std::forward<T>(param));
}

void case3_static_const_member()
{
    std::cout << "\n=== Case 3: Declaration-only static const member ===\n";

    // Direct call — compiler substitutes the literal 28; no address needed
    takeSize(Widget::MinVals);

    // Without the out-of-class definition above, the next line would LINK but
    // the reference binding inside fwdToTakeSize would trigger a linker error:
    // "undefined reference to Widget::MinVals"
    fwdToTakeSize(Widget::MinVals); // OK because the definition exists above
    std::cout << "Fix: provide an out-of-class definition for MinVals.\n";
}

// =============================================================================
// CASE 4: Overloaded Function Names and Function Template Names
// =============================================================================
// A forwarding template deduces T purely from the argument expression, before
// considering what the eventual target function expects. Given an overloaded
// function name the compiler has no context to select an overload; given a
// bare function template name it has no basis to choose an instantiation.
// In both cases the fix is to cast to the exact function-pointer type first.

int processVal(int value)
{
    std::cout << "processVal(int=" << value << ")\n";
    return value;
}

int processVal(int value, int priority)
{
    std::cout << "processVal(int=" << value << ", priority=" << priority << ")\n";
    return value;
}

template<typename T>
T workOnVal(T param) // function template — represents many functions
{
    std::cout << "workOnVal called\n";
    return param;
}

// Target function that expects a specific function-pointer signature
void executeWithInt(int (*f)(int))
{
    std::cout << "executeWithInt result: " << f(10) << "\n";
}

template<typename T>
void fwdToExecute(T&& func)
{
    executeWithInt(std::forward<T>(func));
}

void case4_overloads_and_templates()
{
    std::cout << "\n=== Case 4a: Overloaded Function Names ===\n";

    // Direct call — the parameter type int(*)(int) resolves the overload
    executeWithInt(processVal);

    // fwdToExecute(processVal);
    //   COMPILE ERROR: 'processVal' is overloaded; compiler cannot deduce T.

    // Workaround: cast to the exact overload type so T can be deduced
    using ProcFn = int (*)(int);
    fwdToExecute(static_cast<ProcFn>(processVal)); // T deduced as int(*)(int)
    std::cout << "Workaround: static_cast to the desired overload type.\n";

    std::cout << "\n=== Case 4b: Function Template Names ===\n";

    // Direct call — int(*)(int) context forces workOnVal<int> instantiation
    executeWithInt(workOnVal<int>);

    // fwdToExecute(workOnVal);
    //   COMPILE ERROR: 'workOnVal' is a template, not a function; no
    //   instantiation can be deduced from the forwarding call alone.

    // Workaround: cast the template name to the desired instantiation's type;
    // this forces the compiler to instantiate workOnVal<int>.
    using WorkFn = int (*)(int);
    fwdToExecute(static_cast<WorkFn>(workOnVal)); // selects workOnVal<int>
    std::cout << "Workaround: static_cast template name to its instantiation type.\n";
}

// =============================================================================
// CASE 5: Bitfields
// =============================================================================
// A bitfield may occupy sub-byte storage and has no individual address.
// C++ therefore forbids forming a non-const reference (or pointer) to a
// bitfield. Because perfect forwarding binds a (possibly rvalue) reference to
// its argument, it is equally forbidden for bitfields.

struct IPv4Header {
    std::uint32_t version     : 4,
                  IHL         : 4,
                  DSCP        : 6,
                  ECN         : 2,
                  totalLength : 16;
};

void takeSz(std::size_t sz)
{
    std::cout << "takeSz: " << sz << "\n";
}

template<typename T>
void fwdToTakeSz(T&& param)
{
    takeSz(std::forward<T>(param));
}

void case5_bitfields()
{
    std::cout << "\n=== Case 5: Bitfields ===\n";

    IPv4Header h{};
    h.totalLength = 1480;

    // Direct call — bitfield is copied/converted to std::size_t at the call site
    takeSz(h.totalLength);

    // fwdToTakeSz(h.totalLength);
    //   COMPILE ERROR: cannot bind a reference (non-const or rvalue) to a
    //   bitfield — "cannot bind bitfield to non-const reference" / similar.

    // Workaround: copy the bitfield value into a plain local variable, then
    // forward that. The copy is trivial; the local sits in addressable memory.
    auto len = static_cast<std::uint16_t>(h.totalLength);
    fwdToTakeSz(len); // OK: forwarding a regular local variable
    std::cout << "Workaround: copy bitfield to a local variable, then forward.\n";
}

// =============================================================================
int main()
{
    case1_braced_initializers();
    case2_null_pointers();
    case3_static_const_member();
    case4_overloads_and_templates();
    case5_bitfields();

    std::cout << "\nAll five perfect forwarding failure cases demonstrated.\n";
    return 0;
}
