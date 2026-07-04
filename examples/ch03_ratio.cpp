// ch03 — compile-time Ratio using NTTPs + alias templates
// build: clang++ -std=c++20 -Wall -Wextra ch03_ratio.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <numeric>

template <int Num, int Den = 1>
struct Ratio {
    static_assert(Den != 0, "denominator cannot be zero");
    static constexpr int num = Num / std::gcd(Num, Den);
    static constexpr int den = Den / std::gcd(Num, Den);
    static constexpr double value = static_cast<double>(num) / den;
};

template <class R1, class R2>
using RatioAdd = Ratio<R1::num * R2::den + R2::num * R1::den,
                       R1::den * R2::den>;

int main() {
    using Half  = Ratio<1, 2>;
    using Third = Ratio<1, 3>;
    using Sum   = RatioAdd<Half, Third>;
    std::cout << Sum::num << "/" << Sum::den
              << " = " << Sum::value << '\n';   // 5/6 = 0.833333
}
