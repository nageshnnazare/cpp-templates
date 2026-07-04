// ch10 — CRTP: static polymorphism + a comparison mixin
// build: clang++ -std=c++20 -Wall -Wextra ch10_crtp.cpp -o /tmp/d && /tmp/d
#include <iostream>

template <class Derived>
struct Shape {
    double area() const { return static_cast<const Derived&>(*this).area_impl(); }
};

struct Circle : Shape<Circle> {
    double r;
    explicit Circle(double r) : r(r) {}
    double area_impl() const { return 3.14159265 * r * r; }
};

struct Square : Shape<Square> {
    double s;
    explicit Square(double s) : s(s) {}
    double area_impl() const { return s * s; }
};

// comparison mixin: define == and <, get the rest for free
template <class D>
struct Comparable {
    friend bool operator!=(const D& a, const D& b) { return !(a == b); }
    friend bool operator> (const D& a, const D& b) { return b < a; }
    friend bool operator<=(const D& a, const D& b) { return !(b < a); }
    friend bool operator>=(const D& a, const D& b) { return !(a < b); }
};

struct Version : Comparable<Version> {
    int major, minor;
    Version(int mj, int mn) : major(mj), minor(mn) {}
    friend bool operator==(const Version& a, const Version& b) {
        return a.major == b.major && a.minor == b.minor;
    }
    friend bool operator<(const Version& a, const Version& b) {
        return a.major < b.major || (a.major == b.major && a.minor < b.minor);
    }
};

int main() {
    Circle c(2); Square s(3);
    std::cout << "circle area " << c.area() << '\n';   // 12.566
    std::cout << "square area " << s.area() << '\n';   // 9

    Version a{1, 2}, b{1, 5};
    std::cout << (a < b) << (a != b) << (b >= a) << '\n';  // 111
}
