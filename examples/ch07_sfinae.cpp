// ch07 — expression SFINAE selecting overloads by capability
// build: clang++ -std=c++20 -Wall -Wextra ch07_sfinae.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <type_traits>
#include <vector>
#include <list>

// (1) streamable scalars/strings: pick this when 'cout << x' is valid AND
//     x is not a range we want to expand.
template <class T>
auto print(const T& x) -> std::enable_if_t<std::is_arithmetic_v<T>> {
    std::cout << x << '\n';
}

// (2) containers with begin()/end(): expression SFINAE on c.begin()
template <class T>
auto print(const T& c) -> decltype(c.begin(), c.end(), void()) {
    std::cout << "[ ";
    for (const auto& e : c) std::cout << e << ' ';
    std::cout << "]\n";
}

int main() {
    print(42);                          // (1) 42
    print(3.14);                        // (1) 3.14
    print(std::vector<int>{1, 2, 3});   // (2) [ 1 2 3 ]
    print(std::list<int>{4, 5});        // (2) [ 4 5 ]
}
