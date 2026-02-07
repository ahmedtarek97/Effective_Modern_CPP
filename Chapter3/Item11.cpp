#include <iostream>

class Widget {
public:
    Widget() {
        std::cout << "Widget created" << std::endl;
    }
    ~Widget() {
        std::cout << "Widget destroyed" << std::endl;
    }
    template <typename T>
    void processPointer1(T* ptr) {
        std::cout << "Processing pointer" << std::endl;
    }

private:
    Widget(const Widget& other); // old way to prevent the usage of copy constructor (private not defined)
    Widget& operator=(const Widget& other); // old way to prevent the usage of copy assignment operator (private not defined)
    // template<>
    // void Widget::processPointer1<void>(void* ptr); // Compilation error because we can't give a template specialization a different access modifier also because template specialization must be defined at namespace scope not inside the class scope so we can't make it private but we can delete it as shown in the next line

};

template<>
void Widget::processPointer1<void>(void* ptr) = delete; // we can delete the void* overload of processPointer to prevent it from being called with a void* argument this case can't happen using the make function private way

// any function can be deleted for example if we want to call a specific function overload:
bool isLuckyNumber(int number) {
    return number == 7;
}

// delete can also be used to avoid calling a template function with specific types
template <typename T>
void processPointer(T* ptr) {
    std::cout << "Processing pointer" << std::endl;
}
template <>
void processPointer<void>(void* ptr) = delete; // we can delete the void* overload of processPointer to prevent it from being called with a void* argument

bool isLuckyNumber(char) = delete; // we can delete the char overload of isLuckyNumber to prevent it from being called with a char argument

class WidgetNew {
public:
    WidgetNew() {
        std::cout << "Widget created" << std::endl;
    }
    ~WidgetNew() {
        std::cout << "Widget destroyed" << std::endl;
    }
    WidgetNew(const WidgetNew& other) = delete; // new way to prevent the usage of copy constructor
    WidgetNew& operator=(const WidgetNew& other) = delete; // new way to prevent the usage of copy assignment operator
    // Note: the deleted functions by convention are public because it will cause a better error message if some one used the deleted function
    // as the compiler checks the accessibility of the function before deleted status so if it is private some compilers may show that the function is private error
};

int main() {
   Widget w1;
   // Widget w2 = w1; // compilation error
   // w1 = w2; // compilation error
   isLuckyNumber(7); // ok
//    isLuckyNumber('a'); // compilation error: use of deleted function ‘bool isLuckyNumber(char)’
   return 0;
}
