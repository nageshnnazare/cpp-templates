// ch08 — trait-driven dispatch with if constexpr
// build: clang++ -std=c++20 -Wall -Wextra ch08_traits.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <type_traits>
#include <string>

template <class T>
void show(const T& x) {
    if constexpr (std::is_integral_v<T>) {
        std::cout << "int(" << x << ")\n";
    } else if constexpr (std::is_floating_point_v<T>) {
        std::cout << "float(" << x << ")\n";
    } else if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::string>) {
        std::cout << "string(\"" << x << "\")\n";
    } else if constexpr (std::is_pointer_v<T>) {
        std::cout << "pointer(" << static_cast<const void*>(x) << ")\n";
    } else {
        std::cout << "other\n";
    }
}

int main() {
    show(7);
    show(2.5);
    show(std::string("hi"));
    int v = 3; show(&v);
}
