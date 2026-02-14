#include <vector>
#include <algorithm>
#include <iostream>

int main()
{
    std::vector<int> vec{1, 2, 3, 4, 5};
    auto it = std::find(vec.cbegin(), vec.cend(), 3);
    std::cout << *it << std::endl;
    vec.insert(it, 6);
    //Note using it after insertion is undefined behavior because the iterator may be invalidated 
    // and the part in memory it points to may be reallocated
    std::cout << *it << std::endl; //undefined behavior
    for (const auto& elem : vec) {
        std::cout << elem << " ";
    }
    std::cout << std::endl;
}