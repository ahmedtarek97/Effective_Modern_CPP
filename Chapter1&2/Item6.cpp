#include <iostream>
#include <vector>

// Just an example to a function returning a vector<bool>
std::vector<bool> func(const int n)
{
    std::vector<bool> v(n * 10); // just an example
    // ...
    return v;
}

int main()
{
    bool b = func(5)[2]; // will work fine
    auto ab = func(5)[2]; // will cause predictable behaviour because operator[] in the case of std::vector<bool> returns a proxy object that can be casted to bool

    // solution use The Explicitly Typed Initializer Idiom
    auto sol = static_cast<bool>(func(5)[2]);
}