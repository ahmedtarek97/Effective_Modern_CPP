#include <iostream>
#include <list>

// in both cases FP is a synonym for a pointer to a function taking an int and a string& and return nothing
typedef void (*FP1)(int, std::string&);
using FP2 = void(*) (int, const std::string&);

// the usage of using to create an alias for a linked list that uses the standard allocator
template<typename T>
using MyAllocList = std::list<T, std::allocator<T>>;

// to do the same using typedef
template<typename T>
struct MyAllocList2 {
    typedef std::list<T, std::allocator<T>> type;
};

template<typename T>
class Widget {
    private:
     // usage of MyAllocList in Widget
    MyAllocList<T> myList;

    // usage of MyAllocList2 in Widget
    typename MyAllocList2<T>::type myList2; // we must use typename here because myList2 is dependent on the template parameter T as the compiler can't guarntee MyAllocList2<T>::type is a type or not without instantiating the template with a specific type,
                                             // and it needs to know that before it can compile the code. By using typename we are telling the compiler that myList2 is a type and it should treat it as such.
                                            // in the other hand the compiler guarntees that MyAllocList<T> is a type because it is defined using 'using', which is more straightforward for the compiler to understand.
};




int main() {
    FP1 f1 = [](int, std::string&) {};
    FP2 f2 = [](int, const std::string&) {};

    MyAllocList<Widget<int>> widgets; // using the using MyAllocList  version
    MyAllocList2<Widget<int>>::type widgets2; // using the typedef MyAllocList2 version



    return 0;
}