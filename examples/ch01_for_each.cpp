// ch01 — a generic for_each clone (function template + deduction)
// build: clang++ -std=c++20 -Wall -Wextra ch01_for_each.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <vector>

template <class Iter, class Func>
Func my_for_each(Iter first, Iter last, Func fn) {
    for (; first != last; ++first)
        fn(*first);
    return fn;
}

int main() {
    std::vector<int> v{1, 2, 3, 4};
    my_for_each(v.begin(), v.end(),
                [](int x) { std::cout << x * x << ' '; });
    std::cout << '\n';   // 1 4 9 16
}
