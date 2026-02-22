// The existence of the reference count has performance implications:

// std::shared_ptrs are twice the size of a raw pointer, because they internally contain a raw pointer to the resource as well as a raw pointer to the resource’s reference count.2

// Memory for the reference count must be dynamically allocated. Conceptually, the reference count is associated with the object being pointed to, but pointed-to objects know nothing about this. They thus have no place to store a reference count. (A pleasant implication is that any object—even those of built-in types—may be managed by std::shared_ptrs.) Item 21 explains that the cost of the dynamic allocation is avoided when the std::shared_ptr is created by std::make_shared, but there are situations where std::make_shared can’t be used. Either way, the reference count is stored as dynamically allocated data.

// Increments and decrements of the reference count must be atomic, because there can be simultaneous readers and writers in different threads. For example, a std::shared_ptr pointing to a resource in one thread could be executing its destructor (hence decrementing the reference count for the resource it points to), while, in a different thread, a std::shared_ptr to the same object could be copied (and therefore incrementing the same reference count). Atomic operations are typically slower than non-atomic operations, so even though reference counts are usually only a word in size, you should assume that reading and writing them is comparatively costly.

// The Performance Cost and std::make_shared
// Dynamic allocation (using new) is generally slow. If you write code like this:

// std::shared_ptr<Widget> sp(new Widget());

// You are actually forcing the program to do two dynamic memory allocations:

// new Widget() allocates memory for the object itself.

// The shared_ptr constructor silently allocates memory for the Control Block (the reference count).

// Meyers points out that you can avoid this double-cost by using std::make_shared:

// auto sp = std::make_shared<Widget>();

// When you use std::make_shared, the compiler does a clever optimization: it does one single dynamic allocation that is large enough to hold both the Widget and the Control Block right next to each other in memory. This is faster and prevents memory fragmentation.

#include <iostream>
#include <memory>

int main() {
    std::shared_ptr<int> p1 = std::make_shared<int>(42);
    std::shared_ptr<int> p2 = p1;

    std::cout << "Value: " << *p1 << ", Reference Count: " << p1.use_count() << std::endl;

    p1.reset();

    std::cout << "Value: " << *p2 << ", Reference Count: " << p2.use_count() << std::endl;

    // Using move constructors/assignments with std::shared_ptr doesn't increase the reference count
    // since incrementing the reference count is an atomic operation (which is costly), the move constructor simply transfers ownership without modifying the reference count is faster than copying.
    std::shared_ptr<int> p3 = std::move(p2);

    std::cout << "Value: " << *p3 << ", Reference Count: " << p3.use_count() << std::endl;
    // difference between unique_ptr and shared_ptr when using custom deleter
    auto deleter = [](int* p) {
        std::cout << "Custom deleter called for value: " << *p << std::endl;
        delete p;
    };
    std::shared_ptr<int> p4(new int(42), deleter); //deleter type is not part of the shared_ptr type
    std::unique_ptr<int, decltype(deleter)> p5(new int(42), deleter); //deleter type is part of the unique_ptr type

    // a Control block is a separate memory area that stores the reference count and other metadata (custom deleter, weak count, ...) for shared_ptr
    // using make_shared always creates a new control block
    std::shared_ptr<int> p6 = std::make_shared<int>(42);
    // constructing a shared pointer from a unique pointer also always creates a new control block
    std::unique_ptr<int> p7 = std::make_unique<int>(42);
    std::shared_ptr<int> p8 = std::shared_ptr<int>(std::move(p7));

    // constructing a shared pointer from a raw pointer also always creates a new control block
    int* x = new int(42);
    std::shared_ptr<int> p9 = std::shared_ptr<int>(x);
    // Note: creating multiple shared_ptr from the same raw pointer will lead to undefined behavior
    // because in that case the same data will be pointed do by multiple control blocks(which means multiple reference counts)
    // std::shared_ptr<int> p10 = std::shared_ptr<int>(x);

    return 0;
}