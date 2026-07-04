// ch06 — a tiny recursive tuple (variadic class template)
// build: clang++ -std=c++20 -Wall -Wextra ch06_tuple.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <string>

template <class... Ts> struct Tuple;

template <>
struct Tuple<> {};

template <class Head, class... Tail>
struct Tuple<Head, Tail...> : Tuple<Tail...> {
    Head head;
    Tuple(Head h, Tail... t) : Tuple<Tail...>(t...), head(h) {}
};

// get<0> returns head; get<N> recurses into the base.
template <std::size_t I, class Head, class... Tail>
auto& get(Tuple<Head, Tail...>& t) {
    if constexpr (I == 0) return t.head;
    else                  return get<I - 1>(static_cast<Tuple<Tail...>&>(t));
}

int main() {
    Tuple<int, double, std::string> t(1, 2.5, "hi");
    std::cout << get<0>(t) << ' '
              << get<1>(t) << ' '
              << get<2>(t) << '\n';   // 1 2.5 hi
}
