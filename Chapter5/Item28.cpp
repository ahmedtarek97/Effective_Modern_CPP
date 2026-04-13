#include <iostream>
#include <string>
#include <type_traits>
#include <utility>

// =====================================================================
// Item 28: Understand reference collapsing
// Effective Modern C++ by Scott Meyers
// =====================================================================
//
// Reference collapsing is the mechanism that makes std::forward work and
// explains why T&& in a template is a "universal reference" rather than
// a pure rvalue reference.
//
// THE RULE:
//   If EITHER reference in a reference-to-reference pair is an lvalue
//   reference, the result is an lvalue reference.
//   Only rvalue-ref & rvalue-ref collapses to an rvalue reference.
//
//   T&  &   ->  T&     (lvalue + lvalue  -> lvalue)
//   T&  &&  ->  T&     (lvalue + rvalue  -> lvalue)
//   T&& &   ->  T&     (rvalue + lvalue  -> lvalue)
//   T&& &&  ->  T&&    (rvalue + rvalue  -> rvalue)
//
// Reference collapsing occurs in FOUR contexts:
//   1. Template instantiation
//   2. auto type deduction
//   3. typedef / using aliases
//   4. decltype
//
// =====================================================================


// =====================================================================
// CONTEXT 1: Template instantiation
// =====================================================================
// When a function template declares T&&, and T is deduced as U&, the
// parameter type becomes U& && which collapses to U&.
// When T is deduced as U (non-reference), the type stays U&&.
//
// This is exactly why T&& in a template is a "universal reference":
//   - lvalue argument: T deduced as X&  -> param = X& && -> X&   (lvalue ref)
//   - rvalue argument: T deduced as X   -> param = X&&           (rvalue ref)

template<typename T>
void show_deduced_type(T&& param) {
    constexpr bool is_lref = std::is_lvalue_reference<T>::value;
    constexpr bool param_is_lref = std::is_lvalue_reference<T&&>::value;
    std::cout << "  T is lvalue-ref? " << std::boolalpha << is_lref
              << "  |  T&& collapsed to lvalue-ref? " << param_is_lref << "\n";
    (void)param;
}

void demonstrate_context1_templates() {
    std::cout << "CONTEXT 1: Reference collapsing in template instantiation\n";
    std::cout << "-----------------------------------------------------------\n";

    int x = 10;

    // lvalue: T deduced as int& -> int& && -> int&
    std::cout << "  show_deduced_type(x)  [lvalue]:  ";
    show_deduced_type(x);

    // rvalue: T deduced as int  -> int&&         (no collapse needed)
    std::cout << "  show_deduced_type(10) [rvalue]:  ";
    show_deduced_type(10);

    std::cout << "\n  Rule applied:\n"
              << "    lvalue arg: T = int&  -> T&& = int& && -> int&\n"
              << "    rvalue arg: T = int   -> T&& = int&&\n\n";
}


// =====================================================================
// CONTEXT 2: auto type deduction
// =====================================================================
// auto&& follows the exact same deduction rules as template T&&.
// When auto deduces the type of an expression:
//   - lvalue expression: auto = T&  -> auto&& = T& && -> T&
//   - rvalue expression: auto = T   -> auto&& = T&&

void demonstrate_context2_auto() {
    std::cout << "CONTEXT 2: Reference collapsing in auto type deduction\n";
    std::cout << "-----------------------------------------------------------\n";

    int x = 42;

    auto&& lref = x;       // auto = int&  -> auto&& = int& && -> int&
    auto&& rref = 100;     // auto = int   -> auto&&           = int&&

    std::cout << "  int x = 42;\n"
              << "  auto&& lref = x;    -> auto = int&  -> int& && -> int&\n"
              << "  auto&& rref = 100;  -> auto = int   -> int&&\n";

    std::cout << "  lref is lvalue-ref? "
              << std::boolalpha << std::is_lvalue_reference<decltype(lref)>::value << "\n";
    std::cout << "  rref is rvalue-ref? "
              << std::boolalpha << std::is_rvalue_reference<decltype(rref)>::value << "\n";

    std::cout << "\n  auto&& is a perfect-forwarding variable: it can bind to anything.\n"
              << "  Range-for uses auto&& internally to avoid copies.\n\n";

    (void)lref; (void)rref;
}


// =====================================================================
// CONTEXT 3: typedef and using aliases
// =====================================================================
// If a typedef or using alias is instantiated with a type that creates
// a reference-to-reference, reference collapsing applies.

template<typename T>
struct MyWrapper {
    using RvalueRef = T&&;   // If T = int&, this becomes int& && -> int&
};

void demonstrate_context3_typedef() {
    std::cout << "CONTEXT 3: Reference collapsing in typedef / using aliases\n";
    std::cout << "-----------------------------------------------------------\n";

    // T = int:  RvalueRef = int&&
    using RR_int = MyWrapper<int>::RvalueRef;

    // T = int&: RvalueRef = int& && -> int&   (collapsed!)
    using RR_intref = MyWrapper<int&>::RvalueRef;

    std::cout << "  MyWrapper<int>::RvalueRef  = int&&  (rvalue-ref? "
              << std::is_rvalue_reference<RR_int>::value << ")\n";
    std::cout << "  MyWrapper<int&>::RvalueRef = int&   (lvalue-ref? "
              << std::is_lvalue_reference<RR_intref>::value << ")\n";

    std::cout << "\n  Even though the alias says T&&, instantiating with T=int&\n"
              << "  collapses int& && -> int&.\n\n";
}


// =====================================================================
// CONTEXT 4: decltype
// =====================================================================
// decltype can produce reference-to-reference in certain expressions,
// and reference collapsing resolves them.
// (In practice this context arises less often than the other three.)

void demonstrate_context4_decltype() {
    std::cout << "CONTEXT 4: Reference collapsing in decltype\n";
    std::cout << "-----------------------------------------------------------\n";

    int x = 5;

    // decltype((x)) is int& (parenthesized lvalue -> lvalue-ref)
    // std::add_rvalue_reference<int&>::type = int& && -> int&
    using T = std::add_rvalue_reference<decltype((x))>::type;  // int& && -> int&

    std::cout << "  decltype((x)) = int&\n"
              << "  add_rvalue_reference<int&>::type = int& && -> int&\n"
              << "  result is lvalue-ref? "
              << std::is_lvalue_reference<T>::value << "\n\n";
}


// =====================================================================
// CORE MECHANISM: How std::forward is implemented using reference collapsing
// =====================================================================
// std::forward<T>(param) conditionally casts param to T&&.
// When T = X&  (lvalue was passed): T&& = X& && = X&  -> cast to lvalue ref (no-op)
// When T = X   (rvalue was passed): T&& = X&&         -> cast to rvalue ref (moves)
//
// A simplified equivalent implementation:
//
//   template<typename T>
//   T&& my_forward(remove_reference_t<T>& param) noexcept {
//       return static_cast<T&&>(param);
//   }
//
// CASE A — lvalue was passed, T=Widget&:
//   return type: Widget& &&  ->  Widget&    (reference collapses to lvalue ref)
//   cast:        static_cast<Widget&>(param)   -> param returned as lvalue ref
//
// CASE B — rvalue was passed, T=Widget:
//   return type: Widget&&                       (pure rvalue ref, no collapse)
//   cast:        static_cast<Widget&&>(param)   -> param cast to rvalue ref

// Our own implementation to make the collapsing visible:
template<typename T>
T&& my_forward(std::remove_reference_t<T>& param) noexcept {
    return static_cast<T&&>(param);
}

struct Widget {
    Widget()                          { std::cout << "  [Widget default ctor]\n"; }
    Widget(const Widget&)             { std::cout << "  [Widget copy ctor]\n"; }
    Widget(Widget&&)                  { std::cout << "  [Widget move ctor]\n"; }
};

// Sink function: shows whether a Widget was forwarded as lvalue or rvalue
void process(const Widget&) { std::cout << "  -> process(const Widget&):  received LVALUE\n"; }
void process(Widget&&)      { std::cout << "  -> process(Widget&&):       received RVALUE\n"; }

template<typename T>
void relay(T&& param) {
    process(my_forward<T>(param));
}

void demonstrate_forward_mechanism() {
    std::cout << "CORE MECHANISM: How std::forward uses reference collapsing\n";
    std::cout << "-----------------------------------------------------------\n";

    Widget w;

    std::cout << "\n  relay(w)  [lvalue, T deduced as Widget&]:\n"
              << "    my_forward<Widget&>(param)\n"
              << "    return type: Widget& && -> Widget& (collapsed to lvalue ref)\n"
              << "    result: ";
    relay(w);

    std::cout << "\n  relay(Widget{})  [rvalue, T deduced as Widget]:\n"
              << "    my_forward<Widget>(param)\n"
              << "    return type: Widget&&   (no collapse needed)\n"
              << "    result: ";
    relay(Widget{});

    std::cout << "\n  Key insight:\n"
              << "    std::forward<T> returns T&&.\n"
              << "    Reference collapsing turns T=X&  -> X& && -> X&  (preserves lvalue).\n"
              << "    Reference collapsing turns T=X   -> X&&           (enables move).\n\n";
}


// =====================================================================
// UNIVERSAL REFERENCES — explained through reference collapsing
// =====================================================================
// T&& is a universal reference (forwarding reference) ONLY when T is
// deduced (template parameter or auto). It is NOT universal otherwise.
//
//   void f(Widget&&)             -> pure rvalue reference  (T fixed, no deduction)
//   template<class T> void f(T&&)-> universal reference   (T is deduced)
//   Widget&& w = Widget{};       -> pure rvalue reference  (no deduction)
//   auto&& w = expr;             -> universal reference    (auto deduced)
//
// The "universality" comes from the collapse rules:
//   - Bind an lvalue X to T&&: T deduced as X&, param = X& && = X&
//   - Bind an rvalue X to T&&: T deduced as X,  param = X&&

void demonstrate_universal_vs_rvalue_ref() {
    std::cout << "UNIVERSAL REFERENCES vs PURE RVALUE REFERENCES\n";
    std::cout << "-----------------------------------------------------------\n";

    // Pure rvalue reference: only rvalues can bind
    Widget w;
    Widget&& pure_rref = Widget{};       // OK: rvalue
    // Widget&& pure_rref2 = w;          // ERROR: lvalue cannot bind to &&

    // Universal reference (auto&&): anything can bind
    auto&& univ1 = w;        // auto = Widget& -> Widget& && -> Widget& (lvalue)
    auto&& univ2 = Widget{}; // auto = Widget  -> Widget&&              (rvalue)

    std::cout << "  Widget&& pure_rref = Widget{};  OK (rvalue only)\n";
    std::cout << "  auto&&   univ1     = w;         OK (lvalue -> collapses to Widget&)\n";
    std::cout << "  auto&&   univ2     = Widget{};  OK (rvalue -> Widget&&)\n\n";

    std::cout << "  univ1 is lvalue-ref? " << std::is_lvalue_reference<decltype(univ1)>::value << "\n";
    std::cout << "  univ2 is rvalue-ref? " << std::is_rvalue_reference<decltype(univ2)>::value << "\n\n";

    (void)pure_rref; (void)univ1; (void)univ2;
}


// =====================================================================
// ALL FOUR COLLAPSE RULES demonstrated explicitly
// =====================================================================
// C++ forbids reference-to-reference in user code, but we can observe
// the collapsed types through template instantiation and type aliases.

template<typename A, typename B>
struct CollapseDemo {
    // We apply A as the "inner" part and B as the "outer" part
    // The actual collapsed type depends on what A and B are.
};

// Helper to print whether a type is lvalue-ref, rvalue-ref, or neither
template<typename T>
void print_ref_kind(const char* expr) {
    if      (std::is_lvalue_reference<T>::value) std::cout << "  " << expr << " -> lvalue ref (&)\n";
    else if (std::is_rvalue_reference<T>::value) std::cout << "  " << expr << " -> rvalue ref (&&)\n";
    else                                          std::cout << "  " << expr << " -> value type\n";
}

void demonstrate_all_collapse_rules() {
    std::cout << "ALL FOUR REFERENCE COLLAPSE RULES\n";
    std::cout << "-----------------------------------------------------------\n";
    std::cout << "  Rule: if EITHER ref is lvalue-ref => lvalue-ref\n"
              << "        only && && => &&\n\n";

    // Rule 1: T& &  -> T&
    using R1 = std::add_lvalue_reference<int&>::type;    // int& & -> int&
    print_ref_kind<R1>("int& &   (T& &  )");

    // Rule 2: T& && -> T&
    using R2 = std::add_rvalue_reference<int&>::type;    // int& && -> int&
    print_ref_kind<R2>("int& &&  (T& &&)");

    // Rule 3: T&& & -> T&   (use wrapper template to form rvalue then add lvalue)
    using R3 = std::add_lvalue_reference<int&&>::type;   // int&& & -> int&
    print_ref_kind<R3>("int&& &  (T&& &)");

    // Rule 4: T&& && -> T&&
    using R4 = std::add_rvalue_reference<int&&>::type;   // int&& && -> int&&
    print_ref_kind<R4>("int&& && (T&& &&)");

    std::cout << "\n  Summary:\n"
              << "    int& &   -> int&   (lvalue wins)\n"
              << "    int& &&  -> int&   (lvalue wins)\n"
              << "    int&& &  -> int&   (lvalue wins)\n"
              << "    int&& && -> int&&  (both rvalue -> rvalue)\n\n";
}


// =====================================================================
// PRACTICAL EXAMPLE: Perfect-forwarding factory function
// =====================================================================
// This ties everything together. makeWidget<T>(args...) creates a Widget
// and perfectly forwards its arguments. The forwarded value category
// (lvalue/rvalue) is preserved because reference collapsing makes
// std::forward reconstruct the original value category from T alone.

struct WidgetWithLog {
    std::string label;

    explicit WidgetWithLog(const std::string& s)
        : label(s) { std::cout << "  [WidgetWithLog copy-string ctor] \"" << label << "\"\n"; }

    explicit WidgetWithLog(std::string&& s)
        : label(std::move(s)) { std::cout << "  [WidgetWithLog move-string ctor] \"" << label << "\"\n"; }

    WidgetWithLog(const WidgetWithLog& o)
        : label(o.label) { std::cout << "  [WidgetWithLog copy ctor]\n"; }

    WidgetWithLog(WidgetWithLog&& o) noexcept
        : label(std::move(o.label)) { std::cout << "  [WidgetWithLog move ctor]\n"; }
};

template<typename T, typename... Args>
T makeObject(Args&&... args) {
    // std::forward<Args>(args)... uses reference collapsing for each arg:
    //   If Arg_i = X&  -> X& && -> X&   (forwards as lvalue)
    //   If Arg_i = X   -> X&&           (forwards as rvalue)
    return T(std::forward<Args>(args)...);
}

void demonstrate_perfect_forward_factory() {
    std::cout << "PRACTICAL EXAMPLE: Perfect-forwarding factory\n";
    std::cout << "-----------------------------------------------------------\n";

    std::string s = "Hello";

    std::cout << "  makeObject<WidgetWithLog>(s) [lvalue s]:\n    ";
    auto w1 = makeObject<WidgetWithLog>(s);         // s forwarded as lvalue -> copy ctor

    std::cout << "  makeObject<WidgetWithLog>(std::move(s)) [rvalue]:\n    ";
    auto w2 = makeObject<WidgetWithLog>(std::move(s)); // moved -> move ctor

    std::cout << "  makeObject<WidgetWithLog>(std::string(\"World\")) [temporary]:\n    ";
    auto w3 = makeObject<WidgetWithLog>(std::string("World")); // rvalue -> move ctor

    std::cout << "\n  s is empty-after-move? " << std::boolalpha << s.empty() << "\n\n";

    (void)w1; (void)w2; (void)w3;
}


// =====================================================================
// SUMMARY TABLE printed at runtime
// =====================================================================
void print_summary() {
    std::cout << "=======================================================\n";
    std::cout << "SUMMARY: Item 28 — Understand Reference Collapsing\n";
    std::cout << "=======================================================\n";
    std::cout
        << "THE RULE:\n"
        << "  If EITHER reference is lvalue-ref, result is lvalue-ref.\n"
        << "  T&  &  -> T&     T&  && -> T&\n"
        << "  T&& &  -> T&     T&& && -> T&&\n\n"

        << "FOUR CONTEXTS WHERE COLLAPSING OCCURS:\n"
        << "  1. Template instantiation:\n"
        << "       T&& with T=X&  -> X& && -> X&  (universal ref binds lvalue)\n"
        << "       T&& with T=X   -> X&&           (universal ref binds rvalue)\n\n"
        << "  2. auto type deduction:\n"
        << "       auto&& lref = expr;  deduces like T&&\n\n"
        << "  3. typedef / using aliases:\n"
        << "       using Ref = T&&; when T=X&, Ref becomes X&\n\n"
        << "  4. decltype: similar rules on expression types.\n\n"

        << "HOW std::forward WORKS:\n"
        << "  template<typename T>\n"
        << "  T&& forward(remove_reference_t<T>& p) { return static_cast<T&&>(p); }\n\n"
        << "  T=Widget&  -> return Widget& && -> Widget& (preserves lvalue)\n"
        << "  T=Widget   -> return Widget&&              (enables move)\n\n"

        << "UNIVERSAL REFERENCE vs RVALUE REFERENCE:\n"
        << "  T&& is universal ONLY when T is deduced (template or auto).\n"
        << "  Widget&& is always a pure rvalue reference.\n";
}


int main() {
    demonstrate_context1_templates();
    demonstrate_context2_auto();
    demonstrate_context3_typedef();
    demonstrate_context4_decltype();
    demonstrate_forward_mechanism();
    demonstrate_universal_vs_rvalue_ref();
    demonstrate_all_collapse_rules();
    demonstrate_perfect_forward_factory();
    print_summary();

    return 0;
}
