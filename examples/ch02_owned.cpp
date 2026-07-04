// ch02 — a minimal unique_ptr-like class template
// build: clang++ -std=c++20 -Wall -Wextra ch02_owned.cpp -o /tmp/d && /tmp/d
#include <iostream>
#include <utility>

template <class T>
class Owned {
    T* ptr_;
public:
    explicit Owned(T* p = nullptr) : ptr_(p) {}
    ~Owned() { delete ptr_; }

    Owned(const Owned&)            = delete;
    Owned& operator=(const Owned&) = delete;

    Owned(Owned&& o) noexcept : ptr_(o.ptr_) { o.ptr_ = nullptr; }
    Owned& operator=(Owned&& o) noexcept {
        if (this != &o) { delete ptr_; ptr_ = o.ptr_; o.ptr_ = nullptr; }
        return *this;
    }

    T&  operator*()  const { return *ptr_; }
    T*  operator->() const { return  ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }
};

int main() {
    Owned<int> a(new int(7));
    std::cout << *a << '\n';                       // 7
    Owned<int> b = std::move(a);
    std::cout << (a ? "a full" : "a empty") << '\n';// a empty
    std::cout << *b << '\n';                        // 7
}
