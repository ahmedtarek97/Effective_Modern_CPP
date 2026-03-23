#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Item 24: Distinguish universal references from rvalue references.
//
// Core rules demonstrated below:
// 1) If you see T&& (or auto&&) and T/auto is deduced, it is a universal reference.
// 2) If there is no type deduction, && is an rvalue reference.
// 3) const T&& is never a universal reference.
// 4) In class templates, T&& in non-template member functions is usually rvalue reference,
//    because T was deduced when the class was instantiated, not at the call site.

struct Widget {
	std::string name;

	Widget() : name("default") {}
	explicit Widget(std::string n) : name(std::move(n)) {}
};

void consume(const Widget& w) {
	std::cout << "consume(const Widget&) -> lvalue-like path: " << w.name << '\n';
}

void consume(Widget&& w) {
	std::cout << "consume(Widget&&) -> rvalue path: " << w.name << '\n';
}

// T is deduced, so T&& is a universal reference.
template <typename T>
void universalParam(T&& param) {
	std::cout << "universalParam(T&&): ";
	if constexpr (std::is_lvalue_reference_v<T>) {
		std::cout << "T deduced as lvalue reference\n";
	} else {
		std::cout << "T deduced as non-reference (rvalue case)\n";
	}

	// Perfect forwarding preserves caller value category.
	consume(std::forward<T>(param));
}

// No deduction for Widget here, so this is a true rvalue reference.
void pureRvalueParam(Widget&& param) {
	std::cout << "pureRvalueParam(Widget&&): only rvalues can call this\n";
	consume(std::move(param));
}

// const T&& is never universal; it is always an rvalue reference to const.
template <typename T>
void constRvalueParam(const T&& param) {
	std::cout << "constRvalueParam(const T&&): not universal, rvalue-only and const\n";
	std::cout << "value observed: " << param.name << '\n';
}

template <typename T>
class Holder {
public:
	explicit Holder(T value) : stored(std::move(value)) {}

	// T here belongs to class template; not deduced for this function call.
	// Therefore, this is an rvalue reference, not universal.
	void set(T&& value) {
		std::cout << "Holder::set(T&&): class T already known, so rvalue reference\n";
		stored = std::move(value);
	}

	// U is deduced at call site, so U&& is universal.
	template <typename U>
	void setAny(U&& value) {
		std::cout << "Holder::setAny(U&&): U deduced, so universal reference\n";
		stored = std::forward<U>(value);
	}

	void print() const {
		std::cout << "stored = " << stored << '\n';
	}

private:
	T stored;
};

int main() {
	std::cout << "--- 1) T&& with deduction => universal reference ---\n";
	Widget w1("lvalue-widget");
	universalParam(w1);                 // lvalue argument
	universalParam(Widget("temp"));    // rvalue argument

	std::cout << "\n--- 2) Non-deduced Type&& => rvalue reference ---\n";
	// pureRvalueParam(w1); // Uncommenting fails: cannot bind lvalue to Widget&&
	pureRvalueParam(Widget("rvalue-only"));

	std::cout << "\n--- 3) auto&& is also universal in deduction contexts ---\n";
	auto&& ref1 = w1;                   // deduces to Widget&
	auto&& ref2 = Widget("auto-temp"); // deduces to Widget&&
	consume(ref1);
	consume(std::move(ref2));

	std::cout << "\n--- 4) const T&& is never universal ---\n";
	constRvalueParam(Widget("const-rvalue"));
	// constRvalueParam(w1); // Uncommenting fails: lvalue cannot bind to const T&&

	std::cout << "\n--- 5) Class template member examples ---\n";
	Holder<std::string> h("initial");
	std::string s = "lvalue-string";
	// h.set(s); // Uncommenting fails: set expects std::string&&
	h.set(std::string("rvalue-string"));
	h.print();

	h.setAny(s);                         // lvalue accepted (universal reference)
	h.print();
	h.setAny(std::string("another-temp")); // rvalue accepted too
	h.print();

	std::cout << "\n--- 6) auto&& in range-for (universal-like deduction) ---\n";
	std::vector<std::string> words = {"one", "two", "three"};
	for (auto&& word : words) {
		word += "!"; // word behaves as std::string& here because container yields lvalues
	}
	for (const auto& word : words) {
		std::cout << word << ' ';
	}
	std::cout << '\n';

	return 0;
}
