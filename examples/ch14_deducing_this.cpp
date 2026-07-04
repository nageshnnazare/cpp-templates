// ch14 — deducing this (C++23): one getter for all cv/ref qualifications
// build: clang++ -std=c++23 -Wall -Wextra ch14_deducing_this.cpp -o /tmp/d && /tmp/d
// (needs a C++23 compiler: GCC 14+, Clang 18+)
#include <iostream>
#include <utility>
#include <string>

class Config {
    std::string name_ = "default";
public:
    template <class Self>
    auto&& name(this Self&& self) {
        return std::forward<Self>(self).name_;
    }
};

// bonus: a recursive lambda via deducing this
int main() {
    Config c;
    c.name() = "runtime";
    std::cout << c.name() << '\n';               // runtime

    const Config cc;
    std::cout << cc.name() << '\n';              // default

    auto fib = [](this auto self, int n) -> int {
        return n < 2 ? n : self(n - 1) + self(n - 2);
    };
    std::cout << "fib(10) = " << fib(10) << '\n';// 55
}
