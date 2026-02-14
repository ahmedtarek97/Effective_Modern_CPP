// #include <iostream>
// #include <stdexcept>
// #include <string>

// // A simple class to announce when it is created and destroyed
// class Tracer {
//     std::string name;
// public:
//     Tracer(std::string n) : name(n) {
//         std::cout << "Constructing " << name << "\n";
//     }
//     ~Tracer() {
//         std::cout << "Destructing " << name << "\n";
//     }
// };

// void funcB() {
//     Tracer objB("Object B (in funcB)");
//     std::cout << "About to throw exception...\n";
//     throw std::runtime_error("Error in funcB");
//     // stack unwinding happens here, and objB's destructor will be called
//     // The following line is never executed
//     std::cout << "This will not print.\n"; 
// }

// void funcA() {
//     Tracer objA("Object A (in funcA)");
//     funcB();
//     // stack unwinding happens here, and objA's destructor will be called
//     // This line is also skipped due to unwinding
//     std::cout << "Back in funcA.\n"; 
// }

// int main() {
//     try {
//         funcA();
//     } catch (const std::exception& e) {
//         std::cout << "Caught exception: " << e.what() << "\n";
//     }
//     return 0;
// How Stack Unwinding Works:
// When you throw an exception, the runtime system takes the following steps:

// 1- Search for a Handler: The system looks in the current scope for a try...catch block that can handle the exception.

// 2- Unwind: If it doesn't find one, it leaves the current function scope.
//      - Crucially: Before leaving, it calls the destructors of all fully constructed automatic (local) variables in that scope.

//      - Destructors are called in the reverse order of their construction.
// 3- Repeat: This continues up the call stack (returning to the caller, then the caller's caller) until a matching catch block is found.
// 4- Terminate: If the stack is fully unwound (reaching main) and no handler is found, the program crashes (std::terminate).

#include <iostream>
#include <vector>
#include <utility>

// Class 1: Does NOT promise exception safety
class SlowSafe {
public:
    SlowSafe() {}
    // The compiler assumes this might throw!
    SlowSafe(const SlowSafe&) { std::cout << "Copying (Slow)\n"; }
    SlowSafe(SlowSafe&&) { std::cout << "Moving (Fast)\n"; }
};

// Class 2: PROMISES exception safety (noexcept)
class FastRisk {
public:
    FastRisk() {}
    FastRisk(const FastRisk&) { std::cout << "Copying (Slow)\n"; }
    // MARKED NOEXCEPT: The compiler trusts this!
    FastRisk(FastRisk&&) noexcept { std::cout << "Moving (Fast)\n"; }
};

int main() {
    std::cout << "--- Vector of SlowSafe (No noexcept) ---\n";
    std::vector<SlowSafe> vecSlow;
    vecSlow.push_back(SlowSafe()); 
    std::cout << "Triggering Resize:\n";
    // This resize forces a transfer. 
    // Because 'SlowSafe' move might throw, vector chooses COPY to be safe.
    vecSlow.push_back(SlowSafe()); 

    std::cout << "\n--- Vector of FastRisk (With noexcept) ---\n";
    std::vector<FastRisk> vecFast;
    vecFast.push_back(FastRisk());
    std::cout << "Triggering Resize:\n";
    // This resize forces a transfer.
    // Because 'FastRisk' guarantees no throwing, vector chooses MOVE.
    vecFast.push_back(FastRisk());

    return 0;
}