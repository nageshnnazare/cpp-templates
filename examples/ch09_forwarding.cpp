// ch09 — perfect forwarding: copy for lvalues, move for rvalues
// build: clang++ -std=c++20 -Wall -Wextra ch09_forwarding.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <string>
#include <utility>

struct Heavy {
    std::string data;
    Heavy(std::string s) : data(std::move(s)) { std::cout << "ctor\n"; }
    Heavy(const Heavy& o) : data(o.data)      { std::cout << "COPY\n"; }
    Heavy(Heavy&& o) noexcept : data(std::move(o.data)) { std::cout << "MOVE\n"; }
};

void sink(Heavy h) { std::cout << "sink got: " << h.data << '\n'; }

template <class T>
void forward_to_sink(T&& x) {
    sink(std::forward<T>(x));
}

int main() {
    Heavy h("lvalue");
    std::cout << "-- forwarding lvalue --\n";
    forward_to_sink(h);               // COPY
    std::cout << "-- forwarding rvalue --\n";
    forward_to_sink(Heavy("rvalue")); // ctor then MOVE
}
