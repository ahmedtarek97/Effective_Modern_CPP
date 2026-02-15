class Widget {
    virtual ~Widget() = default; // 
    Widget(const Widget&) = default; // = default means the default copy constructor behaviour (shallow copy) is fine for this class
    Widget& operator=(const Widget&) = default; // Default copy assignment operator (shallow copy) is fine for this class

    
    Widget(Widget&&) = default;
    Widget& operator=(Widget&&) = default;

    // in c++11 the rule of five is enforced by the compilers so 
    // if you define one of the special member functions (copy constructor, copy assignment operator, move constructor, move assignment operator, or destructor),
    // the compiler will not generate the (Move operations) for you. for example if you define a destructor or copy constructor or copy assignment operator the compiler will not generate the move constructor and move assignment operator.
    // also if you declare a move constructor or move assignment operator the compiler will not generate the copy constructor and copy assignment operator.
    // Note if you didn't declare any of the special member functions the compiler will still generate default versions for all of them if they are used.
    // in c++11 and later, if you want the default behavior for all of them, you can explicitly default them as shown above.

};

int main()
{
    return 0;
}