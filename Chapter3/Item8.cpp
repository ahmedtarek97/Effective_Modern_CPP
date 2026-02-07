#include <iostream>

void f(int i) {
    std::cout << "f called with int: " << i << std::endl;
}
void f(bool b) {
    std::cout << "f called with bool: " << std::boolalpha << b << std::endl;
}

void f(void* ptr) {
    std::cout << "f called with void*: " << ptr << std::endl;
}

int main() {

    f(0); // calls f(int)
    // f(NULL); // Error: call of overloaded ‘f(NULL)’ is ambiguous
    f(nullptr); //calls f(void*)

    return 0;
}