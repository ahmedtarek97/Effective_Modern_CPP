#include <iostream>
#include <vector>

 void someFunction(int, double){
     std::cout << "Inside someFunction" << std::endl;
 }

 template<typename T>
 void f(T params)
 {
    std::cout << "Inside f" << std::endl;
 }


// For most of the cases auto type deduction is similar to template type deduction

int main() {   
    auto x = 27; // x's type is deduced to be int
    const auto cx = x; // cx's type is deduced to be const int auto is deduced to be int
    const auto& rx = x; // rx's type is deduced to be const int& auto is deduced to be int
    auto&& ux = x; // ux's type is deduced to be int& auto is deduced to be int&
    const char name[] = "John Doe"; // name's type is const char[9]
    auto arr1 = name; // arr1's type is deduced to be const char*
    auto& arr2 = name; // arr2's type is deduced to be const char (&)[9]
    auto func1 = someFunction; // func1's type is deduced to be void(*)(int, double)
    auto& func2 = someFunction; // func2's type is deduced to be void(&)(int, double)

    // the case that differs from template type deduction is in the case of initialization using {}
    //
    auto x1 = 27; // x1's type is deduced to be int
    auto x2(27); // x2's type is deduced to be int
    auto x3 = {27}; // x3's type is deduced to be std::initializer_list<int> in c++14
    auto x4{27}; // x4's type is deduced to be std::initializer_list<int> in c++14 but in c++17 x4's type is deduced to be int
    auto x5 = {27, 28}; // x5's type is deduced to be std::initializer_list<int> in c++14
    // f({27, 28}); // will get compilation error: couldn’t deduce template parameter ‘T’
    // The main difference is that by default auto assumes that braced initializers represent std::initializer_list while template type deduction does not.

    // for c++14 only
//     std::vector<int> v;
//     auto resetV = [&v] (const auto& newValue) {
//         v = newValue;
//     };  
//     resetV( {1, 2, 3, 4, 5} ); // will not compile as auto in lambda parameter implies template initialization
}

// This will not compile as auto in a function return type acts as template initialization
// auto createInitializerList() {
//     return {1, 2, 3, 4, 5}; // Returns std::initializer_list<int>
// }   
