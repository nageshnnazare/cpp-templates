// ch13 — concepts make overload selection unambiguous
// build: clang++ -std=c++20 -Wall -Wextra ch13_concepts_print.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <concepts>
#include <ranges>
#include <string>
#include <vector>

template <class T>
concept Streamable = requires (std::ostream& os, const T& x) {
    { os << x } -> std::same_as<std::ostream&>;
};

template <Streamable T>
void print(const T& x) { std::cout << x << '\n'; }

template <std::ranges::input_range R>
    requires (!Streamable<R>) && Streamable<std::ranges::range_value_t<R>>
void print(const R& r) {
    std::cout << "[ ";
    for (const auto& e : r) std::cout << e << ' ';
    std::cout << "]\n";
}

int main() {
    print(42);                          // 42
    print(std::string("hi"));           // hi  (string is Streamable)
    print(std::vector<int>{1, 2, 3});   // [ 1 2 3 ]
}
