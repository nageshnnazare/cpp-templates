// ch04 — deduction demonstration + the "reveal T" trick.
// The TypeShow line is COMMENTED so the file compiles; uncomment to see the
// deduced types printed inside a compiler error.
// build: clang++ -std=c++20 -Wall -Wextra ch04_deduction_probe.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <type_traits>
#include <utility>

template <class T> struct TypeShow;   // intentionally undefined

template <class T>
void probe(T&& x) {
    // Uncomment ONE of these to make the compiler print the deduced type:
    // TypeShow<T> a;                 // shows T
    // TypeShow<decltype(x)> b;       // shows the parameter type
    std::cout << "is T a reference? " << std::is_reference_v<T> << '\n';
    (void)x;
}

int main() {
    int i = 0;
    probe(i);    // lvalue -> T = int&   -> is_reference: 1
    probe(42);   // rvalue -> T = int    -> is_reference: 0

    auto        a = std::as_const(i);  // decays -> int
    auto&       b = i;                 // int&
    auto&&      c = 42;                // int&&
    std::cout << (std::is_same_v<decltype(a), int>)   << '\n'; // 1
    std::cout << (std::is_same_v<decltype(b), int&>)  << '\n'; // 1
    std::cout << (std::is_same_v<decltype(c), int&&>) << '\n'; // 1
}
