#include <iostream>
#include <functional>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <memory>
#include <string>

using FilterContainer = std::vector<std::function<bool(int)>>;

FilterContainer filters;
int computeSomeValue1()
{
    // simulate runtime function (logic is not important just for compilation :D)
    return std::rand() % 10 + 1;
}

int computeSomeValue2()
{
    // simulate runtime function (logic is not important just for compilation :D)
    return std::rand() % 10 + 1;
}

int computeDivisor(int p1, int p2)
{
    // simulate runtime function (logic is not important just for compilation :D)
    return p1 + p2;
}

void addDivisorFilter()
{
    auto calc1 = computeSomeValue1();
    auto calc2 = computeSomeValue2();
    auto divisor = computeDivisor(calc1, calc2);

    filters.emplace_back(
        [&](int value){return value % divisor;});  // danger! reference to divisor will dangle
                                                   
 /*
    This code is a problem waiting to happen. 
    The lambda refers to the local variable divisor,
     but that variable ceases to exist when addDivisorFilter returns. 
     That’s immediately after filters.emplace_back returns, 
     so the function that’s added to filters is essentially dead on arrival. 
     Using that filter yields undefined behavior from virtually the moment it’s created.
 */
}
// --- POINT 1 FIX: capture divisor by VALUE with [=] or explicit [divisor] ---
// This makes the closure self-contained; no dangling reference.
void addDivisorFilterSafe()
{
    auto calc1 = computeSomeValue1();
    auto calc2 = computeSomeValue2();
    auto divisor = computeDivisor(calc1, calc2);

    // Safe: divisor is copied into the closure.
    filters.emplace_back(
        [=](int value){ return value % divisor == 0; });  // OK: divisor is captured by value

    // Alternatively, make the captured variable explicit to document the dependency:
    filters.emplace_back(
        [divisor](int value){ return value % divisor == 0; });  // even clearer intent
}

// =============================================================================
// POINT 2: Default [=] capture can be misleading in member functions
//          — it captures 'this', NOT the member variables directly.
// =============================================================================
class Widget
{
public:
    explicit Widget(int v) : divisor(v) {}

    void addFilter() const
    {
        // DANGER: [=] looks like it captures divisor by value,
        // but 'divisor' is actually 'this->divisor'.
        // The lambda captures 'this' (a pointer). If the Widget is destroyed
        // before the filter is called, this->divisor is a dangling access.
        filters.emplace_back(
            [=](int value){ return value % divisor == 0; }  // captures 'this', not divisor!
        );

        /*
        Equivalent and more honest spelling:
            [this](int value){ return value % this->divisor == 0; }
        Both capture the raw pointer — equally dangerous if Widget is destroyed.
        */
    }

    void addFilterSafe() const
    {
        // C++14 FIX: generalized lambda capture — copy the member into a local.
        // The closure is now truly self-contained and independent of Widget's lifetime.
        auto divisorCopy = divisor;                         // local copy
        filters.emplace_back(
            [divisorCopy](int value){ return value % divisorCopy == 0; }
        );

        // C++14 init-capture (even cleaner):
        filters.emplace_back(
            [divisor = divisor](int value){ return value % divisor == 0; }  // copy into closure
        );
    }

    // Demonstrates the shared_ptr idiom for keeping an object alive
    // across async operations that hold a lambda.
    void addFilterSharedThis()
    {
        // Capture a shared_ptr to *this (C++17: [self = shared_from_this()])
        // Here we just show the explicit local-copy pattern as the safe alternative.
        int d = divisor;    // plain copy; no pointer involved
        filters.emplace_back(
            [d](int value){ return value % d == 0; }
        );
    }

private:
    int divisor;
};

// =============================================================================
// POINT 3: Default [=] does NOT capture static local variables.
//          The lambda uses them directly (by reference under the hood), so [=]
//          gives a false sense of "everything is copied".
// =============================================================================
void addDivisorFilterStatic()
{
    static auto divisor = computeSomeValue1();  // static local — NOT captured

    // [=] here captures NOTHING (no non-static locals in the enclosing scope).
    // The lambda refers to the static 'divisor' directly.
    // Modifying the static after this call changes what the lambda computes!
    filters.emplace_back(
        [=](int value){ return value % divisor == 0; }  // silently depends on the static!
    );

    ++divisor;  // mutates the static — the already-added lambda is now different!

    /*
    The fix: do not use default [=] when statics are involved.
    Copy the static into a named local variable, then capture that local:
    */
    int divisorCopy = divisor;  // true by-value copy
    filters.emplace_back(
        [divisorCopy](int value){ return value % divisorCopy == 0; }  // truly independent
    );
}
template <typename C>
void workWithContainer(const C& container)
{
    auto calc1 = computeSomeValue1();
    auto calc2 = computeSomeValue2();
    auto divisor = computeDivisor(calc1, calc2);

    using ContElemT = typename C::value_type;


    if(std::all_of(begin(container), end(container),
                   [&](const ContElemT& value) { return value % divisor; }))
    {
        std::cout<<"All elements passed the filter\n";
    }
    else
    {
        std::cout<<"Some elements did not pass the filter\n";
    }

    /*
    this is safe, but its safety is somewhat precarious. 
    If the lambda were found to be useful in other contexts 
    (e.g., as a function to be added to the filters container) 
    and was copy-and-pasted into a context
     where its closure could outlive divisor, 
     you’d be back in dangle-city,
    and there’d be nothing in the capture clause to specifically
     remind you to perform lifetime analysis on divisor.
    */

}

int main()
{
    // --- Point 1: [&] default capture — dangling reference demo (UB, but shows the pattern) ---
    addDivisorFilter();         // UB: filter holds dangling ref to local divisor

    // Safe alternative
    addDivisorFilterSafe();

    // Verify the safe filters work
    std::cout << "=== Point 1: [&] dangling vs [=] safe capture ===\n";
    // filters[0] is UB — do NOT call it; filters[1] and [2] are safe
    std::cout << "Safe filter (index 1) on value 6: "
              << filters[1](6) << " (1 = passes, 0 = fails)\n";

    // --- Point 2: [=] in member functions captures 'this', not members by value ---
    std::cout << "\n=== Point 2: [=] in member functions — captures 'this' ===\n";
    {
        Widget w(3);
        w.addFilter();          // closure holds raw 'this' — dangerous if w goes out of scope first
        w.addFilterSafe();      // closure holds a true copy of divisor — safe
    }
    // After the block, Widget w is destroyed.
    // filters added by addFilter() now have a dangling 'this' — do NOT call them.
    // filters added by addFilterSafe() are still valid.
    // (We just show the pattern; calling the dangling-this filters would be UB.)
    std::cout << "Safe member filter on value 9: "
              << filters.back()(9) << "\n";

    // --- Point 3: [=] does NOT capture static locals ---
    std::cout << "\n=== Point 3: [=] and static local variables ===\n";
    filters.clear();
    addDivisorFilterStatic();   // adds two filters; second is the safe one

    // Both filters are in 'filters[0]' and 'filters[1]'.
    // filters[0] used [=] with a static — its value changed when the static was incremented.
    // filters[1] captured a true copy before the increment.
    int testValue = 12;
    std::cout << "Static-based filter (may reflect mutation) on " << testValue << ": "
              << filters[0](testValue) << "\n";
    std::cout << "True-copy filter on " << testValue << ": "
              << filters[1](testValue) << "\n";

    // --- workWithContainer: [&] is fine when the lambda doesn't outlive the scope ---
    std::cout << "\n=== Local [&] safe when lifetime is contained ===\n";
    std::vector<int> myVec{1, 2, 3, 4, 5};
    workWithContainer(myVec);

    return 0;
}