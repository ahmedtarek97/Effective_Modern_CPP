#include <iostream>
#include <memory>
#include <functional>
#include <vector>

int main()
{
    // auto x; // Error: 'auto' requires an initializer
    std::function<bool(const std::unique_ptr<int>&, const std::unique_ptr<int>&)> f; // f can refer to any callable object that has the signature bool(const std::unique_ptr<int>&, const std::unique_ptr<int>&)
    f = [](const std::unique_ptr<int>& a, const std::unique_ptr<int>& b) {
        return *a < *b;
    };
    auto af = [](const std::unique_ptr<int>& a, const std::unique_ptr<int>& b) {
        return *a < *b; // it is almost always preferred to use auto over std::function when declaring a variable to hold a lambda.
    };
    std::vector<int> v;
    size_t sz = v.size(); //if you write unsigned by itself, the compiler treats it as unsigned int. This can cause problems in 64-bit systems because unsigned int is 32 bits on most platforms. but size_t is 64 bits on 64-bit platforms.
}