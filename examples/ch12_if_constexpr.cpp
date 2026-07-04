// ch12 — if constexpr branching + always_false idiom
// build: clang++ -std=c++20 -Wall -Wextra ch12_if_constexpr.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <string>
#include <type_traits>

template <class...> inline constexpr bool always_false = false;

template <class T>
std::string describe(const T& x) {
    if constexpr (std::is_integral_v<T>) {
        return "integer: " + std::to_string(x);
    } else if constexpr (std::is_floating_point_v<T>) {
        return "float: " + std::to_string(x);
    } else if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::string>) {
        return "string: " + x;
    } else {
        static_assert(always_false<T>, "describe: unsupported type");
        return {};
    }
}

int main() {
    std::cout << describe(42) << '\n';
    std::cout << describe(3.14) << '\n';
    std::cout << describe(std::string("hi")) << '\n';
    // describe(std::pair<int,int>{});  // would fire the static_assert
}
