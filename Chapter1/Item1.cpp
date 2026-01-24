#include <iostream>
// // Item 1 : Understand template type deduction

// // Psudo code 
// // template<typename T>
// // void f(ParamType param);
// // f(expr);

// // Case 1 : ParamType is a reference or pointer, but not a universal reference
// // rules:
// // 1. If expr's type is a reference, ignore the reference part
// // 2. Then pattern-match expr's type against ParamType to determine T

// template<typename T>
// void f(T& param){
//     std::cout << param << std::endl;
// }

// int main() {
//     int x = 27; 
//     const int cx = x;
//     const int& rx = x; //rx is a reference to a const int

//     f(x); // T is deduced as int ParamType is int&
//     f(cx); // T is deduced as const int ParamType is const int& argument passed remain constant
//     f(rx); // T is deduced as const int& ParamType is const int& argument passed remain constant
// }

// ====================================================================================================================

// // Case 2 : ParamType is a universal reference
// // rules:
// // 1. If expr is an lvalue, both T and ParamType are deduced to be lvalue references.
// //(it’s the only situation in template type deduction where T is deduced to be a reference.)
// // although ParamType is declared using the syntax for an rvalue reference, its deduced type is an lvalue reference.
// // 2. Then pattern-match expr's type against ParamType to determine T
// template<typename T>
// void f(T&& param){
//     std::cout << param << std::endl;
// }

// int main() {
//     int x = 27;
//     const int cx = x;
//     const int& rx = x; //rx is a reference to a const int

//     f(x); // T is deduced as int& ParamType is int&
//     f(cx); // T is deduced as const int& ParamType is const int& argument passed remain constant
//     f(rx); // T is deduced as const int& ParamType is const int& argument passed remain constant
//     f(27); // T is deduced as int ParamType is int&&
// }

// ====================================================================================================================
// // Case 3 : ParamType is neither a reference nor a pointer
// // rules:
// // 1. if expr’s type is a reference, ignore the reference part.
// // 2. If, after ignoring expr’s reference-ness, expr is const or volatile, ignore that.
// template<typename T>
// void f(T param){
//     std::cout << param << std::endl;
// }

// int main() {
//     int x = 27;
//     const int cx = x;
//     const int& rx = x; //rx is a reference to a const int

//     f(x); // T is deduced as int ParamType is int
//     f(cx); // T is deduced as int ParamType is int a new copy is created and const is ignored
//     f(rx); // T is deduced as int ParamType is int a new copy is created and const is ignored    
// }
// ====================================================================================================================
// // Take care of these (Niche) cases:
// // 1. case where expr is a const pointer to a const object, and expr is passed to a by-value param
// template<typename T>
// void f(T param){
//     std::cout << param << std::endl;
// }

// int main() {
//     const char* const pstr = "Hello, World!";
//     f(pstr); // T is deduced as const char* ParamType is const char*
//     // only the constness of the pointer will be ignored
// }
// ====================================================================================================================
// // 2. Array Arguments
// template<typename T>
// void f(T param){
//     std::cout << param << std::endl;
// }
// template<typename T>
// void f1(T& param){
//     std::cout << param << std::endl;
// }

// int main() {
//     const char arr[] = "Hello, World!";
//     f(arr); // arr is an array but T is deduced as const char*
//     f1(arr); // T is deduced as const char[14] ParamType is const char (&)[14]
// }
// ====================================================================================================================
// 3. Function arguments
void someFuncton(int, double){
    std::cout << "Inside someFunction" << std::endl;
}
template<typename T>
void f(T param){
   param(1,2);
}
template<typename T>
void f1(T& param){
    param(1,2);
}

int main() {
    f(someFuncton); //param deduced as ptr-to-func; type is void (*)(int, double)
    f1(someFuncton); // param deduced as ref-to-func; type is void (&)(int, double)
}