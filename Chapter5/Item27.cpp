#include <iostream>
#include <string>
#include <set>
#include <type_traits>
#include <utility>

// =====================================================================
// Item 27: Familiarize yourself with alternatives to overloading on
//          universal references
// Effective Modern C++ by Scott Meyers
// =====================================================================
// Item 26 showed WHY overloading on universal references is problematic:
//   - T&& matches nearly everything, often beating more specific overloads.
//   - Perfect-forwarding constructors hijack copy construction of non-const
//     lvalues (template deduces exact match, copy ctor needs const).
//   - Derived class copy/move constructors call the wrong base ctor.
//
// Item 27 provides five alternatives:
//
//   1. Abandon overloading      — use distinct function names
//   2. Pass const T& / T&&      — two-overload split (old-school)
//   3. Pass by value            — good when the arg is stored anyway
//   4. Tag dispatch             — route internally via true/false_type
//   5. Enable-if (enable_if_t)  — disable the template for unwanted types
//
// BONUS: static_assert inside enable_if templates for clear error messages.
// =====================================================================


// =====================================================================
// Shared state used by the logAndAdd demos
// =====================================================================
std::multiset<std::string> names;


// =====================================================================
// ALTERNATIVE 1: Abandon overloading — use distinct function names
// =====================================================================
// The problem (Item 26): logAndAdd(T&&) and logAndAdd(int) conflict
// because T&& matches `short` exactly, beating the int overload.
//
// Simplest fix: give the two functions different names.
//   logAndAddName(T&&)        — handles string-like arguments
//   logAndAddNameFromIdx(int) — handles integer indices
//
// Short, long, unsigned etc. now correctly call logAndAddNameFromIdx via
// the standard int-promotion or conversion, since there is no competing T&&.
//
// Limitation: constructors always share the class name, so this fix
//             cannot be applied to constructors.

template<typename T>
void logAndAddName(T&& name) {
    names.emplace(std::forward<T>(name));
    std::cout << "  [logAndAddName T&&] forwarded into names\n";
}

void logAndAddNameFromIdx(int idx) {
    std::string n = "Person_" + std::to_string(idx);
    names.emplace(n);
    std::cout << "  [logAndAddNameFromIdx int] " << idx << " -> \"" << n << "\"\n";
}

void demonstrateAlternative1() {
    std::cout << "ALTERNATIVE 1: Abandon Overloading (distinct function names)\n";
    std::cout << "-------------------------------------------------------------\n";

    std::string s = "Alice";
    short       idx = 3;

    logAndAddName(s);                       // lvalue: T = string& -> forward, copies
    logAndAddName(std::string("Bob"));      // rvalue: T = string  -> forward, moves
    logAndAddName("Charlie");               // literal: T = const char* -> emplace in-place

    logAndAddNameFromIdx(idx);              // short -> int promotion -> correct function!
    logAndAddNameFromIdx(22);               // int literal -> correct function

    std::cout << "  short idx=3 correctly calls logAndAddNameFromIdx (not a greedy T&&).\n"
              << "  Limitation: cannot rename constructors.\n\n";
    names.clear();
}


// =====================================================================
// ALTERNATIVE 2: const T& and T&& overloads (old-school split)
// =====================================================================
// For constructors, before universal references existed you would write
// two overloads: one for lvalues (const string&) and one for rvalues (string&&).
// This is perfectly correct and efficient for a small number of parameters.
//
//   PersonOldSchool(const std::string& n)  — called for lvalues (copies)
//   PersonOldSchool(std::string&&      n)  — called for rvalues (moves)
//
// Unlike T&&, these two overloads are unambiguous in resolution.
//
// Scalability issue: for a function with N parameters, you need 2^N
// overloads to cover all lvalue/rvalue combinations — impractical beyond 2.

class PersonOldSchool {
public:
    std::string name;

    explicit PersonOldSchool(const std::string& n) : name(n) {
        std::cout << "  [PersonOldSchool const-ref ctor] copied: \"" << name << "\"\n";
    }

    explicit PersonOldSchool(std::string&& n) : name(std::move(n)) {
        std::cout << "  [PersonOldSchool rvalue-ref ctor] moved: \"" << name << "\"\n";
    }

    PersonOldSchool(const PersonOldSchool& rhs) : name(rhs.name) {
        std::cout << "  [PersonOldSchool copy ctor]\n";
    }

    PersonOldSchool(PersonOldSchool&& rhs) noexcept : name(std::move(rhs.name)) {
        std::cout << "  [PersonOldSchool move ctor]\n";
    }
};

void demonstrateAlternative2() {
    std::cout << "ALTERNATIVE 2: const T& and T&& Overloads (old-school split)\n";
    std::cout << "-------------------------------------------------------------\n";

    std::string s = "Diana";

    std::cout << "PersonOldSchool p1(s) [lvalue]:          \t"; PersonOldSchool p1(s);
    std::cout << "PersonOldSchool p2(std::move(s)) [rvalue]:\t"; PersonOldSchool p2(std::move(s));
    std::cout << "PersonOldSchool p3(string temp):          \t"; PersonOldSchool p3(std::string("Eva"));
    std::cout << "PersonOldSchool p4(p1) [copy]:            \t"; PersonOldSchool p4(p1);
    std::cout << "PersonOldSchool p5(std::move(p3)) [move]: \t"; PersonOldSchool p5(std::move(p3));

    std::cout << "\n  Pros: clear, efficient, no surprise overload resolution.\n"
              << "  Cons: 2^N overloads needed for N parameters (doesn't scale).\n\n";
    (void)p2; (void)p4; (void)p5;
}


// =====================================================================
// ALTERNATIVE 3: Pass by value
// =====================================================================
// Replace logAndAdd(T&& name) with logAndAdd(std::string name).
// The caller performs the copy or move when constructing the parameter;
// inside the function we always move from the local copy into the set.
//
// Cost vs. T&&:
//   lvalue arg: 1 copy (into param) + 1 move (param into set)  [T&&: 1 copy]
//   rvalue arg: 1 move (into param) + 1 move (param into set)  [T&&: 1 move]
//   -> one extra move per call — negligible for most types.
//
// Key benefit: the int overload coexists cleanly.
//   short -> cannot convert to std::string; can promote to int -> int overload.
//   There is no greedy T&& to steal the call.

void logAndAddByVal(std::string name) {
    names.emplace(std::move(name));   // move the local copy into the container
    std::cout << "  [logAndAddByVal string] emplaced via move\n";
}

void logAndAddByVal(int idx) {
    std::string n = "Person_" + std::to_string(idx);
    names.emplace(n);
    std::cout << "  [logAndAddByVal int] " << idx << " -> \"" << n << "\"\n";
}

void demonstrateAlternative3() {
    std::cout << "ALTERNATIVE 3: Pass by Value\n";
    std::cout << "-------------------------------------------------------------\n";

    std::string s = "Grace";
    short       idx = 7;

    logAndAddByVal(s);                      // copy into param, move into set
    logAndAddByVal(std::string("Henry"));   // move into param, move into set
    logAndAddByVal("Irene");                // construct string from literal, move into set
    logAndAddByVal(22);                     // int overload — exact match
    logAndAddByVal(idx);                    // short -> int promotion -> int overload! [fixed]

    std::cout << "  short idx=7 -> int overload (no greedy T&&).\n"
              << "  Pros: no overload collisions, efficient enough for stored values.\n"
              << "  Cons: one extra move vs T&&; pigeonholes parameter type.\n\n";
    names.clear();
}


// =====================================================================
// ALTERNATIVE 4: Tag dispatch
// =====================================================================
// Keep a single public logAndAdd(T&&) entry point for full forwarding
// efficiency. Inside, deduce a compile-time boolean tag from type traits
// and forward the call to one of two distinct (non-template) overloads.
//
//   is_integral<remove_reference_t<T>> == true  -> integral path
//   is_integral<remove_reference_t<T>> == false -> string-like path
//
// remove_reference_t is required because T may be deduced as int&, short&,
// etc. and is_integral on a reference type would return false.
//
// The two impl overloads differ only in their tag parameter type
// (std::true_type vs std::false_type), which are distinct types — so
// normal, unambiguous overload resolution picks the right one.
//
// Limitation: you cannot pass a tag as a second argument to a constructor,
//             so tag dispatch cannot be applied directly to constructors.

template<typename T>
void logAndAddImpl(T&& name, std::false_type) {   // non-integral: string-like path
    names.emplace(std::forward<T>(name));
    std::cout << "  [logAndAddImpl false_type] non-integral, forwarded\n";
}

void logAndAddImpl(int idx, std::true_type) {      // integral: index path
    std::string n = "Person_" + std::to_string(idx);
    names.emplace(n);
    std::cout << "  [logAndAddImpl true_type] integral " << idx << " -> \"" << n << "\"\n";
}

template<typename T>
void logAndAddTagDispatch(T&& name) {
    logAndAddImpl(
        std::forward<T>(name),
        std::is_integral<std::remove_reference_t<T>>{}   // the compile-time tag
    );
}

void demonstrateAlternative4() {
    std::cout << "ALTERNATIVE 4: Tag Dispatch\n";
    std::cout << "-------------------------------------------------------------\n";

    std::string s = "Julia";
    short       idx  = 5;
    long        lidx = 9;

    logAndAddTagDispatch(s);                      // false_type: non-integral -> string path
    logAndAddTagDispatch(std::string("Kevin"));   // false_type: non-integral -> string path
    logAndAddTagDispatch("Laura");                // false_type: const char* not integral
    logAndAddTagDispatch(22);                     // true_type:  int -> integral path
    logAndAddTagDispatch(idx);                    // true_type:  short is integral -> correct!
    logAndAddTagDispatch(lidx);                   // true_type:  long  is integral -> correct!

    std::cout << "  short/long correctly routed to the integral path.\n"
              << "  Pros: preserves full T&& forwarding efficiency.\n"
              << "  Cons: more complex; not directly usable for constructors.\n\n";
    names.clear();
}


// =====================================================================
// ALTERNATIVE 5: Constrain templates via std::enable_if
// =====================================================================
// For constructors (where tag dispatch is impractical), use enable_if
// to make the forwarding template invisible to the compiler for certain
// argument types. When the template is disabled, the compiler considers
// the next-best match — the copy or move constructor.
//
// Condition for the template to be ENABLED:
//   !is_base_of<PersonEnableIf, decay_t<T>>   -> T is not PersonEnableIf-like
//   && !is_integral<remove_reference_t<T>>     -> T is not an integer type
//
// decay_t<T> strips reference and cv-qualifiers, so:
//   T = PersonEnableIf&               -> decay = PersonEnableIf       -> DISABLED
//   T = const PersonEnableIf&         -> decay = PersonEnableIf       -> DISABLED
//   T = SpecialPersonEnableIf&        -> decay = SpecialPersonEnableIf -> DISABLED (derived!)
//   T = std::string                   -> decay = string               -> ENABLED
//   T = const char*                   -> decay = const char*          -> ENABLED
//
// When disabled for integer types, the separate int ctor handles them.
// When disabled for PersonEnableIf/derived, copy/move ctors take over.

class PersonEnableIf {
public:
    std::string name;

    // Template ctor — enabled only for string-like, non-PersonEnableIf types
    template<
        typename T,
        typename = std::enable_if_t<
            !std::is_base_of<PersonEnableIf, std::decay_t<T>>::value &&
            !std::is_integral<std::remove_reference_t<T>>::value
        >
    >
    explicit PersonEnableIf(T&& n) : name(std::forward<T>(n)) {
        std::cout << "  [PersonEnableIf template ctor]\n";
    }

    // Integer ctor — accepts int, short (via promotion), long, etc.
    explicit PersonEnableIf(int idx)
        : name("Person_" + std::to_string(idx)) {
        std::cout << "  [PersonEnableIf int ctor] index -> \"" << name << "\"\n";
    }

    PersonEnableIf(const PersonEnableIf& rhs) : name(rhs.name) {
        std::cout << "  [PersonEnableIf copy ctor]\n";
    }

    PersonEnableIf(PersonEnableIf&& rhs) noexcept : name(std::move(rhs.name)) {
        std::cout << "  [PersonEnableIf move ctor]\n";
    }
};

// Derived class — copy/move now correctly call the base copy/move ctor
// because the base template ctor is DISABLED for PersonEnableIf-derived types.
//
// Without enable_if (see Item 26), the base template would be an EXACT match
// and would beat the base copy/move ctors (which need a derived-to-base
// conversion). With enable_if the template is disabled, so the normal
// copy/move ctors win.

class SpecialPersonEnableIf : public PersonEnableIf {
public:
    explicit SpecialPersonEnableIf(int idx) : PersonEnableIf(idx) {}

    // Calling PersonEnableIf(rhs) where rhs : const SpecialPersonEnableIf&
    //   decay_t<const SpecialPersonEnableIf&> = SpecialPersonEnableIf
    //   is_base_of<PersonEnableIf, SpecialPersonEnableIf> = true -> DISABLED
    //   -> PersonEnableIf(const PersonEnableIf&)  copy ctor selected. Correct!
    SpecialPersonEnableIf(const SpecialPersonEnableIf& rhs)
        : PersonEnableIf(rhs) {}

    // Calling PersonEnableIf(std::move(rhs)) where rhs : SpecialPersonEnableIf&&
    //   decay_t<SpecialPersonEnableIf> = SpecialPersonEnableIf -> DISABLED
    //   -> PersonEnableIf(PersonEnableIf&&)  move ctor selected. Correct!
    SpecialPersonEnableIf(SpecialPersonEnableIf&& rhs) noexcept
        : PersonEnableIf(std::move(rhs)) {}
};

void demonstrateAlternative5() {
    std::cout << "ALTERNATIVE 5: Constrain Templates via std::enable_if\n";
    std::cout << "-------------------------------------------------------------\n";

    std::cout << "PersonEnableIf p1(\"Alice\"):              \t"; PersonEnableIf p1("Alice");
    std::cout << "PersonEnableIf p2(std::string(\"Bob\")):   \t"; PersonEnableIf p2(std::string("Bob"));

    short idx = 3;
    std::cout << "\nPersonEnableIf p3(short 3):              \t"; PersonEnableIf p3(idx); // int ctor
    std::cout << "PersonEnableIf p4(int 22):               \t"; PersonEnableIf p4(22);  // int ctor

    std::cout << "\n--- Non-const lvalue copy now uses the copy ctor [FIXED] ---\n";
    std::cout << "PersonEnableIf p5(p1) [non-const lvalue]:\t"; PersonEnableIf p5(p1);   // copy ctor!
    std::cout << "PersonEnableIf p6(std::move(p1)):        \t"; PersonEnableIf p6(std::move(p1)); // move ctor!

    const PersonEnableIf cp("Carol");
    std::cout << "PersonEnableIf p7(cp) [const lvalue]:    \t"; PersonEnableIf p7(cp);   // copy ctor

    std::cout << "\n--- Derived class copy/move calls base copy/move [FIXED] ---\n";
    std::cout << "SpecialPersonEnableIf sp1(10):            \t"; SpecialPersonEnableIf sp1(10);
    std::cout << "SpecialPersonEnableIf sp2(sp1) [copy]:   \t"; SpecialPersonEnableIf sp2(sp1);
    std::cout << "SpecialPersonEnableIf sp3(move(sp1)):    \t"; SpecialPersonEnableIf sp3(std::move(sp1));

    std::cout << "\n  p5(p1): non-const lvalue -> copy ctor [FIXED]\n"
              << "  sp2(sp1): derived copy -> base copy ctor [FIXED]\n"
              << "  sp3(move): derived move -> base move ctor [FIXED]\n\n";
    (void)p2; (void)p3; (void)p4; (void)p5; (void)p6; (void)p7;
    (void)sp2; (void)sp3;
}


// =====================================================================
// BONUS: static_assert inside enable_if templates
// =====================================================================
// enable_if rejections produce deep, cryptic compiler errors. A well-placed
// static_assert inside the template body emits a readable diagnostic.
//
// The static_assert fires ONLY when enable_if allows the call through
// but the argument is still not usable (e.g., not constructible to string).
// For truly incompatible types (Person-derived), enable_if blocks first
// and the static_assert is never reached — that is intentional.
//
// Together, they provide:
//   - enable_if: prevents hijacking copy/move constructors.
//   - static_assert: explains WHY a similarly-constrained call fails.

class PersonStaticAssert {
public:
    std::string name;

    template<
        typename T,
        typename = std::enable_if_t<
            !std::is_base_of<PersonStaticAssert, std::decay_t<T>>::value
        >
    >
    explicit PersonStaticAssert(T&& n) : name(std::forward<T>(n)) {
        // If T passes the enable_if check but cannot construct a string,
        // the user gets this message instead of pages of template errors.
        static_assert(
            std::is_constructible<std::string, T>::value,
            "PersonStaticAssert requires a string-like argument. "
            "Did you accidentally pass an integer or an incompatible type?"
        );
        std::cout << "  [PersonStaticAssert template ctor]\n";
    }

    PersonStaticAssert(const PersonStaticAssert& rhs) : name(rhs.name) {
        std::cout << "  [PersonStaticAssert copy ctor]\n";
    }

    PersonStaticAssert(PersonStaticAssert&& rhs) noexcept : name(std::move(rhs.name)) {
        std::cout << "  [PersonStaticAssert move ctor]\n";
    }
};

void demonstrateBonus() {
    std::cout << "BONUS: static_assert for user-friendly error messages\n";
    std::cout << "-------------------------------------------------------------\n";

    std::cout << "PersonStaticAssert pa(\"Alice\"):    \t"; PersonStaticAssert pa("Alice");
    std::cout << "PersonStaticAssert pb(pa) [copy]:  \t"; PersonStaticAssert pb(pa);

    // Uncommenting the line below triggers static_assert with a clear message:
    // PersonStaticAssert bad(42);
    //   => ERROR: PersonStaticAssert requires a string-like argument.
    //             Did you accidentally pass an integer or an incompatible type?

    std::cout << "  Uncomment 'PersonStaticAssert bad(42)' to see the friendly error.\n\n";
    (void)pb;
}


int main() {
    demonstrateAlternative1();
    demonstrateAlternative2();
    demonstrateAlternative3();
    demonstrateAlternative4();
    demonstrateAlternative5();
    demonstrateBonus();

    std::cout << "=======================================================\n";
    std::cout << "SUMMARY: Alternatives to Overloading on Universal References\n";
    std::cout << "=======================================================\n";
    std::cout
        << "1. Abandon overloading: use distinct function names.\n"
        << "   Simple and clear. Cannot be applied to constructors.\n\n"
        << "2. const T& + T&& overloads (old-school split):\n"
        << "   Correct and efficient; explodes to 2^N for N parameters.\n\n"
        << "3. Pass by value: accepts lvalues (copy) and rvalues (move).\n"
        << "   One extra move vs T&&; cleanly coexists with other overloads.\n\n"
        << "4. Tag dispatch: route internally via std::true_type/false_type.\n"
        << "   Keeps full T&& efficiency; not directly usable for constructors.\n\n"
        << "5. std::enable_if: disable the template for unwanted argument types.\n"
        << "   Works for constructors; use std::is_base_of + std::decay_t\n"
        << "   so that derived types disable the template too.\n"
        << "   Fixes non-const lvalue copy and derived-class copy/move.\n\n"
        << "BONUS: static_assert inside the template -> clear compile-time error\n"
        << "       messages when a mismatched type slips past enable_if.\n";

    return 0;
}
