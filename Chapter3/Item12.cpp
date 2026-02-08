#include <iostream>
class Base {
public:
    virtual void mf1() const;
    virtual void mf2(int x);
    virtual void mf3() &;
    void mf4() const;

};

class Derived : public Base {
public:
    // None of these functions override the base class versions
    virtual void mf1(); // different const qualifier
    virtual void mf2(unsigned int x); // different parameter type
    virtual void mf3() &&; // different reference qualifier
    void mf4() const; //mf4 is not virtual
};

class Widget {
public:
    Widget() {
        std::cout << "Widget created" << std::endl;
    }
    ~Widget() {
        std::cout << "Widget destroyed" << std::endl;
    }

    // This is example of functions with different reference qualifiers

    // 1- This version applies only if the widget object calling it is an lvalue
    void doWork() & {
        std::cout << "Doing work on lvalue Widget" << std::endl;
        
    }
    // 2- This version applies only if the widget object calling it is an rvalue
    void doWork() && {
        std::cout << "Doing work on rvalue Widget" << std::endl;
    }
    // for overriding to happen the two function's reference qualifiers must match

};


int main() {
   Widget w1;
   w1.doWork();
   std::move(w1).doWork();
   int override = 0; // the keyword override is only reserved in the context of being after a member function declaration
    return 0;
}