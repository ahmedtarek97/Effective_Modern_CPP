#include <iostream>
#include <array>

// constexpr functions mean the function can be evaluated at compile time if all its arguments are known at compile time
// if not, they will be evaluated at runtime
// in c++11 constexpr should only be one statement
constexpr int pow(int base, int exp) {
    return (exp == 0) ? 1 : base * pow(base, exp - 1);
}

// in c++14 and later, constexpr functions can have multiple statements
// constexpr functions can only have a return type that is a literal type 
// which means types that can be evaluated at compile time
constexpr int powCPlusPlus14(int base, int exp) {
    int result = 1;
    for (int i = 0; i < exp; ++i) {
        result *= base;
    }
    return result;
}

// literal type is all built-in types (like int, float, double, ... ) except void, enums, and classes with constexpr constructors and no non-literal members

class Point {
    double x, y;
public:
    constexpr Point(double startX, double startY) : x(startX), y(startY) {}

    // These allow the compiler to modify 'x' and 'y' during compilation
    // setters can be evaluated at compile time in c++14 and later
    constexpr void setX(double newX) noexcept { x = newX; }
    constexpr void setY(double newY) noexcept { y = newY; }

    constexpr double getX() const { return x; }
};


int main()
{
    int sz;

    // constexpr auto arraySize1 = sz; // error: sz value is not known at compile time
    // std::array<int, arraySize1> arr1; // error: sz value is not known at compile time

    constexpr auto arraySize2 = 10; // OK: 10 is a compile-time constant
    std::array<int, arraySize2> arr2; // OK: arraySize2 is a compile-time constant

    const int arraySize3 = sz; // OK: arraySize3 is a const int, but its value is not known at compile time
    // std::array<int, arraySize3> arr3; // error: arraySize3 value is not known at compile time

    constexpr auto arraySize4 = pow(2, 3); // OK: pow(2, 3) is a compile-time constant
    std::array<int, arraySize4> arr4; // OK: arraySize4 is a compile-time constant

    std::array<int, powCPlusPlus14(3, 4)> arr5; // OK: powCPlusPlus14(3, 4) is a compile-time constant
    
    
     // 1. Compile-time usage


     constexpr Point p1(0, 0); // OK: Point constructor is constexpr
    // The compiler executes setX() and stores the result '10.5' directly in the binary.
    constexpr Point p = []{
        Point pt(0, 0);
        pt.setX(10.5); 
        return pt;
    }();

    static_assert(p.getX() == 10.5, "Value should be 10.5");

    // 2. Runtime usage
    // It also works just like a normal function!
    Point dynamicPoint(1.0, 1.0);
    dynamicPoint.setX(5.0); 
    
    return 0;
}