#include <iostream>
#include <string>
#include <type_traits>                     // for std::remove_reference type trait
#include <utility>

// Reimplementation of std::move to demonstrate how it works
// move() does NOT move anything by itself - it only casts to rvalue reference
// This allows move semantics to be triggered (move constructors, move assignment)

template<typename T>                       // Template parameter deduced from argument
                                           // If lvalue passed: T deduces to Type&
                                           // If rvalue passed: T deduces to Type

// Return type: the base type as an rvalue reference
// Example: if T=int& (lvalue arg), remove_reference makes int, then && makes int&&
//          if T=int (rvalue arg), remove_reference keeps int, then && makes int&&
// This ensures the result is ALWAYS an rvalue reference, regardless of input
typename std::remove_reference<T>::type&&
move(T&& param)                            // Forwarding reference parameter
                                           // Can accept both lvalue and rvalue arguments
                                           // Due to reference collapsing rules:
                                           // - lvalue int& + && = int& (collapses to &)
                                           // - rvalue int + && = int&&
{
  // Create a type alias for the return type to avoid repeating the long expression
  using ReturnType =                       // 'using' is alias declaration syntax (Item 9)
    typename std::remove_reference<T>::type&&;  // typedef alternative: typedef typename...

  // The core operation: cast param (a named variable/lvalue) to an rvalue reference
  // This is the ONLY thing move does - it converts an lvalue expression into an rvalue
  // Why cast is needed: named variables are always lvalues, even if their type is T&&
  // Example: move(x) returns x cast to rvalue, enabling move constructor to be called
  return static_cast<ReturnType>(param);
}

// same implementation but using c++14
template<typename T>                          
decltype(auto) move(T&& param)               
{
  using ReturnType = std::remove_reference_t<T>&&;
  return static_cast<ReturnType>(param);
}

class Person {
public:
  explicit Person(std::string n) : name(std::move(n)) {}

  // Move constructor
  // Parameter must be non-const (Person&&, not const Person&&):
  // 1) moving usually modifies the source (we clear other.name below), which const forbids,
  // 2) std::move(const T) gives const T&&, and most move operations cannot steal from const.
  Person(Person&& other) noexcept : name(std::move(other.name)) {
    other.name = "";
    std::cout << "move constructor called\n";
  }

  Person& operator=(Person&& other) noexcept {
    if (this != &other) {
      // calls string move assignment if other was const the copy assignment operator would be called instead
      // because an lvalue-reference-to-const is permitted to bind to a const rvalue.
      name = std::move(other.name);
      other.name = "";
      std::cout << "move assignment called\n";
    }
    return *this;
  }

  const std::string& getName() const {
    return name;
  }

private:
  std::string name;
};

void target(std::string& s)  { std::cout << "Lvalue (copy) called\n"; }
void target(std::string&& s) { std::cout << "Rvalue (move) called\n"; }

template <typename T>
void wrapper(T&& arg) { 
    target(std::forward<T>(arg));     // without std::forward<T> the overload target(std::string& s) will be called 
}

// Example for copy elision and return value optimization (RVO)
struct Heavy {
    Heavy()              { std::cout << "construct\n"; }
    Heavy(const Heavy&)  { std::cout << "copy\n"; }
    Heavy(Heavy&&)       { std::cout << "move\n"; }
};

Heavy make() {
    Heavy h;      // constructed directly in caller's slot
    return h;
}

int main() {
    Person p1("Ahmed");
    Person p2(std::move(p1));          // move constructor

    Person p3("Initial");
    p3 = std::move(p2);                // move assignment

    std::cout << "p1: '" << p1.getName() << "'\n";
    std::cout << "p2: '" << p2.getName() << "'\n";
    std::cout << "p3: '" << p3.getName() << "'\n";

    wrapper(std::string("hello"));

    Heavy x = make(); // if RVO is applied, only "construct" will be printed, no "copy" or "move"

    return 0;
}
