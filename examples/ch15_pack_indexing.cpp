// ch15 — C++26 pack indexing.
// build: clang++ -std=c++2c -Wall -Wextra ch15_pack_indexing.cpp -o /tmp/d && /tmp/d
// (needs a C++26-capable compiler: GCC 15+, Clang 19+. If unsupported, this
//  file will fail to compile — that is expected on older toolchains.)
#include <iostream>

template <class... Ts>
auto first(Ts... args) {
    return args...[0];             // value pack indexing
}

template <class... Ts>
auto last(Ts... args) {
    return args...[sizeof...(args) - 1];
}

template <class... Ts>
using first_type = Ts...[0];       // type pack indexing

int main() {
    std::cout << first(10, 20, 30) << '\n';   // 10
    std::cout << last(10, 20, 30) << '\n';    // 30
    static_assert(sizeof(first_type<char, int, double>) == sizeof(char));
    std::cout << "ok\n";
}
