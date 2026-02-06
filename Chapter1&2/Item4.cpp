#include <iostream>

template <typename T>
class TD; // only declaration can't instantiate variable of type TD

int main() {

    const int ans = 42;
    // hovering over the variable name shows the type
    auto x = ans;
    auto y = &ans;
    // TD<decltype(x)> xType;  // will cause compilation error but in the error message it will show the deduced type of decltype(x)
    // TD<decltype(y)> yType; // will cause compilation error but in the error message it will show the deduced type of decltype(y)
    return 0;
}