#include <iostream>
#include <vector>

// the primary use of decltype is declaring function templates where the function's return type depends on its parameter types
// C++11 Example:
// Note the usage of -> decltype(c[i]) it says that the return type of the function is the same as the type of the expression c[i]
// we need it at the end after -> and not at the beginning of the function declaration because it will not compile as c[i] are not declared yet
template <typename Container, typename Index>
auto authAndAccess(Container&& c, Index&& i) -> decltype(c[i])
{
    return c[i];
}
// C++ 14 Example:
//in C++14 we can use decltype(auto) as the return type it deduces the return type based on the expression used in the return statement
template <typename Container, typename Index>
decltype(auto) authAndAccess(Container&& c, Index&& i)
{
    return c[i];
}

int main() 
{
    const int & x = 42;
    auto y = x; // auto is deduced as int it strips const and &
    decltype(x) z = x; // decltype(x) is deduced as const int& it keeps const and &

    // Note:
    decltype(y) w = y; // decltype(y) is deduced as int
    decltype((y)) u = y; // decltype((y)) is deduced as int& note the double parentheses is considered an expression not just a type and expression in c++ return an lvalue reference
    return 0;
}