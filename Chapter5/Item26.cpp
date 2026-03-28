#include <iostream>
#include <string>
#include <set>

// =====================================================================
// Item 26: Avoid overloading on universal references
// Effective Modern C++ by Scott Meyers
// =====================================================================
// Key Points:
//  1. Universal reference overloads get called more often than expected —
//     they match nearly everything, often beating other overloads.
//  2. Perfect-forwarding constructors conflict with copy constructors
//     for non-const lvalue arguments because the template instantiates
//     an exact match (T&& = Class&) while the copy ctor needs const.
//  3. Perfect-forwarding ctors in base classes make derived class
//     copy/move constructors silently invoke the wrong (template) ctor.
// =====================================================================


// =====================================================================
// POINT 1: Universal references are too greedy
// =====================================================================
// When we have overloads T&& and int, calling with short, long, float etc.
// goes to T&& (exact match) rather than int (requires promotion).

template<typename T>
void processValue(T&&) {
    std::cout << "  [T&& universal reference overload]\n";
}

void processValue(int) {
    std::cout << "  [int overload]\n";
}

void demonstratePoint1() {
    std::cout << "POINT 1: Universal refs match more than expected\n";
    std::cout << "------------------------------------------------\n";

    int   i = 10;
    short s = 20;
    long  l = 30;
    float f = 1.5f;

    std::cout << "processValue(int variable):    "; processValue(i);   // int overload (exact)
    std::cout << "processValue(42)  [literal]:   "; processValue(42);  // int overload (exact)
    std::cout << "processValue(short variable):  "; processValue(s);   // T&& wins! (short->int needs promotion)
    std::cout << "processValue(long variable):   "; processValue(l);   // T&& wins! (long->int needs narrowing)
    std::cout << "processValue(float variable):  "; processValue(f);   // T&& wins! (not int at all)

    std::cout << "\n  Expected: all integral types -> int overload\n";
    std::cout << "  Reality:  only exact int -> int overload; all others -> T&&\n\n";
}


// =====================================================================
// POINT 2: Real-world case — logAndAdd with an int-index overload
// =====================================================================
// Goal: insert names by value/reference OR by an integer index that
//       maps to a name stored elsewhere.
// Attempt: add logAndAdd(int idx) alongside logAndAdd(T&&).
// Problem: calling with short/long routes to T&&, not int, causing
//          a compile error when T&& tries to emplace a short into
//          multiset<string> — cannot construct string from short.

std::multiset<std::string> names;

template<typename T>
void logAndAdd(T&& name) {
    names.emplace(std::forward<T>(name));
    std::cout << "  [logAndAdd T&&] forwarded into names set\n";
}

void logAndAdd(int idx) {
    std::string found = "Person_" + std::to_string(idx);
    names.emplace(found);
    std::cout << "  [logAndAdd int] index " << idx << " -> \"" << found << "\"\n";
}

void demonstratePoint2() {
    std::cout << "POINT 2: logAndAdd – overloading T&& with int\n";
    std::cout << "------------------------------------------------\n";

    std::string name = "Darla";
    logAndAdd(name);                      // T = std::string&      [OK]
    logAndAdd(std::string("Persephone")); // T = std::string&&     [OK, moved]
    logAndAdd("Patty Dog");               // T = const char*       [OK, constructs in-place]
    logAndAdd(22);                        // int overload          [OK, exact match]

    std::cout << "\n  --- The Problem ---\n";
    std::cout << "  short idx = 5; logAndAdd(idx);\n";
    std::cout << "    T&& overload: T=short -> logAndAdd(short&&)    EXACT MATCH\n";
    std::cout << "    int overload: short->int promotion required     NOT exact\n";
    std::cout << "    T&& WINS -> names.emplace(short(5))\n";
    std::cout << "    Cannot construct std::string from short -> COMPILE ERROR!\n";
    std::cout << "  Same failure for std::size_t, long, unsigned, etc.\n\n";
}


// =====================================================================
// POINT 3: Perfect-forwarding constructor vs copy/move constructor
// =====================================================================
// A class that has a perfect-forwarding ctor alongside a copy ctor
// behaves unexpectedly when copying non-const objects.
//
//   Person p1("Alice");      // template ctor: T=const char*  [expected, OK]
//   const Person cp("Bob");
//
//   Person p3(cp);           // Which ctor fires?
//     Template: T=const Person& -> Person(const Person&) — exact match
//     Copy ctor:               -> Person(const Person&) — identical
//     TIE -> non-template preferred -> copy ctor  [correct]
//
//   Person p4(p1);           // Which ctor fires?  p1 is non-const
//     Template: T=Person& -> Person(Person&)         EXACT MATCH
//     Copy ctor:           -> Person(const Person&)  needs 'const' qualifier
//     Template WINS -> tries: name(std::forward<Person&>(p1))
//                  -> tries to init std::string from Person -> COMPILE ERROR!
//
//   Person p5(std::move(p1));
//     Template: T=Person -> Person(Person&&)    — exact match
//     Move ctor:          -> Person(Person&&)   — identical
//     TIE -> non-template preferred -> move ctor  [correct]

class Person {
public:
    std::string name;

    template<typename T>
    explicit Person(T&& n) : name(std::forward<T>(n)) {
        std::cout << "  [Person template ctor]\n";
    }

    Person(const Person& rhs) : name(rhs.name) {
        std::cout << "  [Person copy ctor]\n";
    }

    Person(Person&& rhs) noexcept : name(std::move(rhs.name)) {
        std::cout << "  [Person move ctor]\n";
    }
};

void demonstratePoint3() {
    std::cout << "POINT 3: Perfect-forwarding ctor vs copy/move ctor\n";
    std::cout << "------------------------------------------------\n";

    std::cout << "Person p1(\"Alice\"):                \t"; Person p1("Alice");
    std::cout << "const Person cp(\"Bob\"):             \t"; const Person cp("Bob");

    std::cout << "\nPerson p3(cp) [const lvalue]: ";
    Person p3(cp);                     // copy ctor: tie -> non-template wins  [correct]

    std::cout << "\nPerson p5(std::move(p1)) [rvalue]: ";
    Person p5(std::move(p1));          // move ctor: tie -> non-template wins  [correct]

    std::cout << "\n  --- The Problem ---\n";
    std::cout << "  Person p4(p1);  // p1 is non-const\n";
    std::cout << "    Template: T=Person& -> Person(Person&)        EXACT MATCH\n";
    std::cout << "    Copy ctor:          -> Person(const Person&)  needs const\n";
    std::cout << "    Template WINS -> name(std::forward<Person&>(p1))\n";
    std::cout << "                  -> init string from Person -> COMPILE ERROR!\n\n";

    (void)p3; (void)p5;
}


// =====================================================================
// POINT 4: Derived classes make the problem worse
// =====================================================================
// When SpecialPerson's copy/move ctor delegates to the base, it passes
// a SpecialPerson. The base template ctor is a better match than the
// base copy/move ctor because no derived-to-base conversion is needed.
//
//   SpecialPerson sp2(sp1);    [copy]
//     Base template: T=const SpecialPerson& -> Base(const SpecialPerson&)  EXACT
//     Base copy ctor:                       -> Base(const Base&)   needs conversion
//     Template WINS  (fires template instead of copy ctor — wrong!)
//
//   SpecialPerson sp3(std::move(sp1));   [move]
//     Base template: T=SpecialPerson  -> Base(SpecialPerson&&)  EXACT
//     Base move ctor:                 -> Base(Base&&)           needs conversion
//     Template WINS  (fires template instead of move ctor — wrong!)
//
// We use a base class (Logger) whose template ctor accepts any argument
// but only stores an int, so it does NOT recurse through std::any.
// This lets us observe which constructor is called at runtime.

class Logger {
public:
    int id;

    // Perfect-forwarding ctor: accepts any argument, stores nothing from it.
    template<typename T>
    explicit Logger(T&&) : id(0) {
        std::cout << "  [Logger template ctor]\n";
    }

    Logger(const Logger& rhs) : id(rhs.id) {
        std::cout << "  [Logger copy ctor]\n";
    }

    Logger(Logger&& rhs) noexcept : id(rhs.id) {
        std::cout << "  [Logger move ctor]\n";
    }
};

class SpecialLogger : public Logger {
public:
    explicit SpecialLogger(int n) : Logger(n) {}

    // Intended: call Logger copy ctor.
    // Reality:  Logger(T&&) with T=const SpecialLogger is an exact match,
    //           while Logger(const Logger&) needs a derived-to-base conversion.
    //           Template WINS -> wrong ctor is called.
    SpecialLogger(const SpecialLogger& rhs) : Logger(rhs) {}

    // Intended: call Logger move ctor.
    // Reality:  Logger(T&&) with T=SpecialLogger is an exact match,
    //           while Logger(Logger&&) needs a derived-to-base conversion.
    //           Template WINS -> wrong ctor is called.
    SpecialLogger(SpecialLogger&& rhs) noexcept : Logger(std::move(rhs)) {}
};

void demonstratePoint4() {
    std::cout << "POINT 4: Derived classes — copy/move calls wrong base ctor\n";
    std::cout << "------------------------------------------------\n";

    std::cout << "SpecialLogger sl1(42):                  "; SpecialLogger sl1(42);

    std::cout << "\nCopying sl1 -> sl2 [should call copy ctor]:\n";
    SpecialLogger sl2(sl1);
    // Logger(const SpecialLogger&) via template = exact match
    // Logger(const Logger&) copy ctor           = needs derived-to-base
    // -> template wins (wrong!)

    std::cout << "\nMoving sl1 -> sl3 [should call move ctor]:\n";
    SpecialLogger sl3(std::move(sl1));
    // Logger(SpecialLogger&&) via template = exact match
    // Logger(Logger&&) move ctor           = needs derived-to-base
    // -> template wins (wrong!)

    std::cout << "\n  Both copy and move invoked the template ctor — not the copy/move ctor!\n\n";

    (void)sl2; (void)sl3;
}


// =====================================================================
// POINT 5: Even direct copies of the base class show the same issue
//          for non-const lvalues
// =====================================================================
// This applies to the base class itself (not just derived).
// Non-const lvalue copy resolves to the template (exact) over the
// copy ctor (needs const qualifier added). Only const sources and
// rvalues tie with the copy/move ctors, allowing the non-template to win.

void demonstratePoint5() {
    std::cout << "POINT 5: Non-const lvalue copy hits template on the base class too\n";
    std::cout << "------------------------------------------------\n";

    std::cout << "Logger l1(10)  [template ctor]:         \t"; Logger l1(10);
    std::cout << "const Logger cl(20) [template ctor]:   \t"; const Logger cl(20);

    std::cout << "\nLogger l2(cl) [const lvalue]:           \t"; Logger l2(cl);  // tie -> copy ctor wins
    std::cout << "Logger l3(std::move(cl)) [const rvalue]: \t"; Logger l3(std::move(cl));
    // T=const Logger -> Logger(const Logger&&) exact; copy ctor needs lvalue binding -> template wins
    std::cout << "Logger l4(std::move(l1)) [rvalue]:     \t"; Logger l4(std::move(l1)); // tie -> move ctor wins

    std::cout << "Logger l5(l1) [non-const lvalue]:      \t"; Logger l5(l1);
    // Template: T=Logger& -> Logger(Logger&)        EXACT MATCH
    // Copy ctor: Logger(const Logger&)              needs 'const' qualifier
    // -> template wins (wrong!)

    std::cout << "\n  l5(l1): non-const lvalue -> template wins over copy ctor.\n"
              << "  Same root cause as the derived class problem.\n\n";

    (void)l2; (void)l3; (void)l4; (void)l5;
}


int main() {
    demonstratePoint1();
    demonstratePoint2();
    demonstratePoint3();
    demonstratePoint4();
    demonstratePoint5();

    std::cout << "=======================================================\n";
    std::cout << "SUMMARY: Why to Avoid Overloading on Universal References\n";
    std::cout << "=======================================================\n";
    std::cout << "1. T&& is too greedy: it binds to nearly every type\n"
              << "   (short, long, float, derived types, non-const lvalues…)\n"
              << "   often beating more specific overloads.\n\n";
    std::cout << "2. For non-const lvalues, T&& deduces an exact match\n"
              << "   (e.g. Person&) that beats const-ref overloads\n"
              << "   including compiler-generated copy constructors.\n\n";
    std::cout << "3. Perfect-forwarding constructors silently prevent\n"
              << "   copy/move construction for non-const lvalue sources.\n\n";
    std::cout << "4. Derived class copy/move constructors that delegate\n"
              << "   to a base with a perfect-forwarding ctor will call\n"
              << "   the template, not the base copy/move ctor.\n\n";
    std::cout << "=> See Item 27: use std::enable_if / tag dispatch\n"
              << "   to constrain when the universal reference applies.\n";

    return 0;
}
