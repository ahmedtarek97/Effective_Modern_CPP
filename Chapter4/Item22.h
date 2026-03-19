// Example of Pimpl idiom
#include <memory>

// The Pimpl Idiom (called an "idiom" rather than a "design pattern" because it
// exploits C++-specific details: separate compilation, header files, and the
// #include mechanism). It reduces compilation dependencies by moving
// implementation details (data members, their headers) from the header file
// into a source file, so clients don't need to recompile when the
// implementation changes.
class Widget {
public:
    Widget();
    ~Widget();
    Widget( Widget && rhs);
    Widget& operator=( Widget && rhs);
    Widget(const Widget& rhs);              // declarations
    Widget& operator=(const Widget& rhs);   // only

private:

    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

