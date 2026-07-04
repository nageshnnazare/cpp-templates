// ch12 — compile-time lookup table with constexpr
// build: clang++ -std=c++20 -Wall -Wextra ch12_constexpr_table.cpp -o /tmp/d && /tmp/d
#include <array>
#include <iostream>

constexpr std::array<int, 10> make_squares() {
    std::array<int, 10> a{};
    for (int i = 0; i < 10; ++i) a[i] = i * i;
    return a;
}

constexpr auto squares = make_squares();
static_assert(squares[4] == 16);
static_assert(squares[9] == 81);

int main() {
    for (int x : squares) std::cout << x << ' ';
    std::cout << '\n';   // 0 1 4 9 16 25 36 49 64 81
}
