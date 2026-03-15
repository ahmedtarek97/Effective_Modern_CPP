#include <iostream>
#include <memory>
#include <vector>

//simple make_unique implementation
template<typename T, typename... Ts>
std::unique_ptr<T> make_unique(Ts&&... params)
{
  return std::unique_ptr<T>(new T(std::forward<Ts>(params)...));
}

// an example to show how make_shared prevent memory leak
void foo(std::shared_ptr<int> ptr, int value)
{
    // Do something with ptr
}

// example of a function that returns int and can throw an exception
int computeValue(bool error)
{
    
    if(error)
        throw std::runtime_error("Error occurred");
    else return 5;
}

int main() {

    /*
    This can cause a memory leak because of compilers’ translation of source code into object code. 
    At runtime, the arguments for a function must be evaluated before the function can be invoked, so in the call to foo, 
    the following things must occur before foo can begin execution:

    The expression “new int” must be evaluated, i.e., an int must be created on the heap.

    The constructor for the std::shared_ptr<int> responsible for managing the pointer produced by new must be executed.

    computeValue must run.

    Compilers are not required to generate code that executes them in this order. 
    “new int” must be executed before the std::shared_ptr constructor may be called, 
    because the result of that new is used as an argument to that constructor, 
    but computeValue may be executed before those calls, after them, or, crucially, between them. 
    That is, compilers may emit code to execute the operations in this order:

    1. Perform “new int”.

    2. Execute computeValue.

    3. Run std::shared_ptr constructor.

    If such code is generated and, at runtime, computeValue produces an exception, the dynamically allocated int from Step 1 will be leaked, 
    because it will never be stored in the std::shared_ptr that’s supposed to start managing it in Step 3.
     */
    foo(std::shared_ptr<int>(new int(42)), computeValue(true));

    /* Using std::make_shared avoids this problem.
    At runtime, either std::make_shared or computeValue will be called first. 
    If it’s std::make_shared, the raw pointer to the dynamically allocated int is safely stored in the returned std::shared_ptr before computeValue is called. 
    If computeValue then yields an exception, the std::shared_ptr destructor will see to it that the int it owns is destroyed. 
    And if computeValue is called first and yields an exception, std::make_shared will not be invoked, and there will hence be no dynamically allocated int to worry about.
    */
    foo(std::make_shared<int>(42), computeValue(true));

    /*
    using new with shared_ptr entails a memory allocation, but it actually performs two. 
    Item 19 explains that every std::shared_ptr points to a control block containing, among other things, the reference count for the pointed-to object.
    Memory for this control block is allocated in the std::shared_ptr constructor. 
    Direct use of new, then, requires one memory allocation for the int and a second allocation for the control block.
    */
    std::shared_ptr<int> sp(new int(42));

    /*
    If std::make_shared is used instead,one allocation suffices.
    That’s because std::make_shared allocates a single chunk of memory to hold both the int object and the control block. 
    This optimization reduces the static size of the program, 
    because the code contains only one memory allocation call, and it increases the speed of the executable code, because memory is allocated only once. 
    Furthermore, using std::make_shared obviates the need for some of the bookkeeping information in the control block, potentially reducing the total memory footprint for the program.
    */
    std::shared_ptr<int> sp2 = std::make_shared<int>(42);

    // creates a unique_ptr for a vector of 10 elements each valued at 20 (make functions prefer parentheses initialization)
    auto upv = std::make_unique<std::vector<int>>(10, 20);

    // creates a shared_ptr for a vector of 10 elements each valued at 20 (make functions prefer parentheses initialization)
    auto spv = std::make_shared<std::vector<int>>(10, 20);

    return 0;
}
