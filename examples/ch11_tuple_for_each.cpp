// ch11 — iterate a std::tuple with index_sequence + comma fold
// build: clang++ -std=c++20 -Wall -Wextra ch11_tuple_for_each.cpp -o /tmp/d && /tmp/d
#include <tuple>
#include <iostream>
#include <utility>

template <class Tuple, class F, std::size_t... Is>
void tuple_for_each_impl(Tuple&& t, F&& f, std::index_sequence<Is...>) {
    (f(std::get<Is>(std::forward<Tuple>(t))), ...);
}

template <class Tuple, class F>
void tuple_for_each(Tuple&& t, F&& f) {
    constexpr auto N = std::tuple_size_v<std::remove_reference_t<Tuple>>;
    tuple_for_each_impl(std::forward<Tuple>(t), std::forward<F>(f),
                        std::make_index_sequence<N>{});
}

int main() {
    auto t = std::make_tuple(1, 2.5, "hi");
    tuple_for_each(t, [](const auto& x) { std::cout << x << ' '; });
    std::cout << '\n';   // 1 2.5 hi
}
