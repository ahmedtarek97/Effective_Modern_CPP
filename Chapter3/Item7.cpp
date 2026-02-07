#include <iostream>

class Widget {
public:
    Widget() : data{0} {}  // Default constructor
    Widget(int x) : data{x} {}
    Widget(int i, bool b) : data{i}, flag{b} {std::cout << "Constructor with int and bool called\n"; }
    Widget(int i, double d) : data{i}, value{d} {std::cout << "Constructor with int and double called\n"; }
    Widget(std::initializer_list<long double> il){std::cout << "Constructor with initializer_list called\n"; }
    Widget(const Widget& other) : data{other.data} {std::cout << "Copy constructor called\n";}  // Copy constructor
    Widget(Widget&& other) noexcept : data{other.data} { std::cout << "Move constructor called\n"; }    
    operator float() const { return static_cast<float>(data); }   // convert to float
    Widget& operator=(const Widget& other) {  // Copy assignment operator
        if (this != &other) {
            data = other.data;
        }
        return *this;
    }
private:
    int data;
    bool flag;
    double value;
};

int main() {
    // 4 ways for initialization in c++
    int a = 5;          // 1. C-style initialization
    int b(10);          // 2. initialization with parentheses
    int c{15};          // 3. initialization with curly braces (C++11)
    int d = {20};    // 4. Copy initialization (C++11)  c++ treats it as 3

    Widget w1;          // Default constructor
    Widget w2 = w1;     // Copy constructor
    w1 = w2;             // Copy assignment operator

    // Curly brace initialization can be used everywhere the other ways are not
    // also it prevents narrowing conversions which is the compiler's way of avoiding potential data loss example:
    double x,y,z;
    // int sum1{ x + y + z }; // error: narrowing conversion from double to int
    int sum2 = x + y + z;  // compiles but may cause data loss
    int sum3(x+y+z); // compiles but may cause data loss

    Widget w3(10);
    // Widget w4(); // This is a function declaration, not an object instantiation. It declares a function named w4 that takes no parameters and returns a Widget.
    Widget w5{}; // This is an object instantiation using curly braces.

    Widget w6(10, true); //calls Widget(int i, bool b)
    Widget w7{10, true}; //calls initializer_list constructor (10 and true convert to long double)
    Widget w8(10, 5.0);  //calls Widget(int i, double d)
    Widget w9{10, 5.0};  //calls initializer_list constructor (10 and 5.0 convert to long double)
    Widget w10(w5);     //calls copy constructor
    Widget w11{w5};     //calls initializer_list constructor  (w5 converts to float, and float converts to long double)
    Widget w12(std::move(w5));  //calls move constructor
    Widget w13{std::move(w5)};  //calls initializer_list constructor  (w5 converts to float, and float converts to long double)

    return 0;
}