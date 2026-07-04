// ch06 — fold expressions
// build: clang++ -std=c++20 -Wall -Wextra ch06_fold_print.cpp -o /tmp/d && /tmp/d
#include <iostream>

template <class... Ts>
auto sum(Ts... xs) { return (xs + ... + 0); }          // binary left fold

template <class... Ts>
bool all_true(Ts... xs) { return (xs && ...); }         // unary right fold

template <class... Ts>
void print(Ts... xs) { ((std::cout << xs << ' '), ...); std::cout << '\n'; }

int main() {
    std::cout << sum(1, 2, 3, 4) << '\n';     // 10
    std::cout << sum() << '\n';               // 0 (empty-safe)
    std::cout << all_true(true, true, false) << '\n';  // 0
    print(1, 'x', 2.5, "hi");                 // 1 x 2.5 hi
}
