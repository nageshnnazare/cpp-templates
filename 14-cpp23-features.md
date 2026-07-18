# 14 — C++23 Template-Relevant Features

C++23 didn't add a headline "templates" feature, but several additions
significantly affect generic code. The two biggest are **deducing `this`** and
**`if consteval`**; we also cover the spaceship recap, `auto(x)`, multidimensional
`operator[]`, and library concepts additions.

---

## 1. Deducing `this` (explicit object parameter) — the star feature

A member function can take an **explicit object parameter** named `this`,
letting the function template deduce the *actual* type of the object it's called
on (including cv/ref-qualification and the derived type).

```cpp
struct Widget {
    template <class Self>
    void process(this Self&& self) {          // 'this Self&&' is the object
        // self is the real object; Self carries value category + derived type
    }
};
```

```
   Old: 4 overloads for  &  const&  &&  const&&
        void f() &; void f() const&; void f() &&; void f() const&&;
   New: ONE template deduces all of them:
        template<class Self> void f(this Self&& self);
```

### Benefit 1: deduplicate cv/ref overloads

```cpp
class Buffer {
    std::vector<int> data_;
public:
    // one function replaces 4 getter overloads; returns matching ref-ness:
    template <class Self>
    auto&& data(this Self&& self) {
        return std::forward<Self>(self).data_;
    }
};
```

```
   Buffer b;         b.data()        -> vector<int>&
   const Buffer cb;  cb.data()       -> const vector<int>&
   std::move(b).data()               -> vector<int>&&
   All from ONE definition. 'self' forwards the object's value category.
```

### Benefit 2: replaces CRTP (no more `static_cast`)

```cpp
struct Base {
    template <class Self>
    void interface(this Self&& self) {
        self.impl();          // 'self' IS the derived type -> no CRTP boilerplate
    }
};
struct Derived : Base {
    void impl() { /* ... */ }
};
Derived{}.interface();        // Self deduced as Derived
```

```
   CRTP (old):                         Deducing this (C++23):
   template<class D> struct Base {      struct Base {
     void f(){ static_cast<D*>(this)     template<class Self>
               ->impl(); } };            void f(this Self&& self){
   struct D : Base<D> {...};                self.impl(); } };
                                        struct D : Base {...};
   - needs Base<D>                       - plain inheritance
   - static_cast footgun                 - type-safe, deduced
```

### Benefit 3: recursive lambdas!

Lambdas can now recurse by naming themselves via the explicit object parameter:

```cpp
auto fib = [](this auto self, int n) -> int {
    return n < 2 ? n : self(n-1) + self(n-2);   // 'self' is the lambda itself
};
fib(10);   // 55
```

```
   Before C++23: lambdas couldn't reference themselves (needed std::function
                 or a Y-combinator trick).
   Now: [](this auto self, ...) { ... self(...) ... }
```

Gotchas: an explicit object parameter member can't be `static`, `virtual`, or
cv/ref-qualified in the usual way (the object param carries that instead), and
you don't write `this->` — use `self.`.

---

## 2. `if consteval` (C++23)

Covered in [12-constexpr-consteval.md](12-constexpr-consteval.md). Recap: branch
on whether you're in a constant-evaluation context, safely.

```cpp
constexpr int f(int x) {
    if consteval { return ct_impl(x); }
    else         { return rt_impl(x); }
}
```

---

## 3. `operator<=>` (three-way comparison) — C++20, but essential for generic code

The "spaceship" operator generates all six relational operators, killing a lot
of CRTP-comparison boilerplate (chapter 10).

```cpp
#include <compare>

struct Point {
    int x, y;
    auto operator<=>(const Point&) const = default;   // generates < <= > >= == !=
};
```

```
   a <=> b  yields one of:
     std::strong_ordering   (a<b, a==b, a>b — totally ordered, e.g. ints)
     std::weak_ordering     (equivalent but not identical, e.g. case-insensitive)
     std::partial_ordering  (may be unordered, e.g. floats with NaN)

   = default  -> member-wise lexicographic comparison, and synthesizes ==, !=,
                 <, <=, >, >= for you.
```

For templates, constrain on `std::three_way_comparable<T>`:

```cpp
template <std::three_way_comparable T>
T clamp3(const T& v, const T& lo, const T& hi) {
    return v < lo ? lo : (hi < v ? hi : v);
}
```

---

## 4. `auto(x)` and `auto{x}` — decay-copy in the language (C++23)

A concise way to make a **prvalue copy** with decayed type — useful in generic
code to detach from references/const or to force a copy before mutating.

```cpp
template <class T>
void pop_and_use(std::vector<T>& v) {
    auto last = auto(v.back());   // copy the element (decayed T), before pop
    v.pop_back();
    use(last);                    // safe: 'last' is independent of the vector
}
```

```
   auto(expr)   -> a prvalue of type decay_t<decltype(expr)>
   Like std::decay_copy, but built into the language. Handy in templates to
   avoid dangling references to container internals.
```

---

## 5. Multidimensional subscript `operator[](a, b, c)` (C++23)

`operator[]` can now take multiple arguments — great for generic matrix/tensor
templates:

```cpp
template <class T, std::size_t Rows, std::size_t Cols>
struct Matrix {
    std::array<T, Rows*Cols> data{};
    constexpr T& operator[](std::size_t r, std::size_t c) {
        return data[r * Cols + c];
    }
};
Matrix<double, 3, 3> m;
m[1, 2] = 5.0;                     // real 2D indexing, no operator()/proxy hacks
```

```
   Before C++23:  m[1][2] (needs proxy)  or  m(1,2) (operator())
   C++23:         m[1, 2]  directly.
   (std::mdspan in C++23 uses this.)
```

---

## 6. `std::mdspan` (C++23) — a template-heavy multidimensional view

`std::mdspan<T, Extents, LayoutPolicy, AccessorPolicy>` is a non-owning
multidimensional array view — a great real-world example of **policy-based
design** (chapter 10) in the standard library.

```cpp
// int buffer[12] viewed as a 3x4 matrix:
// std::mdspan m(buffer, 3, 4);
// m[i, j]   // uses multidim operator[]
```

```
   mdspan<T, Extents, Layout, Accessor>
                       ^^^^^^  ^^^^^^^^  <- policy parameters:
                       row/col-major     custom read (e.g. atomic, strided)
   Configure layout & access without changing the element type. Zero overhead.
```

---

## 7. Other C++23 bits that touch generic code

```
   * std::expected<T,E>          -> templated error-handling vocabulary type
   * std::print / std::println   -> <print>, format-based, uses variadic templates
   * ranges additions           -> zip, enumerate, chunk, slide, adjacent, ...
   * constexpr <cmath>, constexpr std::unique_ptr  -> more compile-time code
   * static operator() and static operator[]       -> stateless functors avoid
                                                       passing 'this'
   * deduction-guide & CTAD refinements
```

`static operator()` lets stateless function objects skip the implicit `this`,
which can matter for zero-overhead generic callables:

```cpp
struct Square {
    static constexpr int operator()(int x) { return x * x; }   // C++23: static
};
```

---

## 8. Compiler support (2026)

```
   Deducing this      : GCC 14+, Clang 18+, MSVC 19.32+
   if consteval       : GCC 12+, Clang 14+, MSVC 19.32+
   operator<=> default: GCC 10+, Clang 10+ (C++20)
   multidim operator[]: GCC 12+, Clang 15+ (as extension earlier)
   auto(x)            : GCC 12+, Clang 15+
   std::expected      : GCC 12+, Clang 16+ (libc++ 16)
   std::mdspan        : GCC 14+ / recent libstdc++, Clang w/ recent libc++
   std::print         : GCC 14+, Clang 18+ (partial), MSVC recent
```

Build C++23 examples with `-std=c++23` (or `-std=c++2b` on slightly older
toolchains).

---

## 9. Worked example: one getter for all qualifications

```cpp
#include <iostream>
#include <utility>
#include <string>

class Config {
    std::string name_ = "default";
public:
    // deducing this: single function serves &, const&, &&, const&&
    template <class Self>
    auto&& name(this Self&& self) {
        return std::forward<Self>(self).name_;
    }
};

int main() {
    Config c;
    c.name() = "runtime";                 // returns std::string&  (mutable)
    std::cout << c.name() << '\n';         // runtime

    const Config cc;
    std::cout << cc.name() << '\n';        // returns const std::string&
    // cc.name() = "x";                    // would not compile (const)

    std::string taken = std::move(Config{}).name();   // returns std::string&&
    std::cout << taken << '\n';            // default
}
```

Runnable: [`examples/ch14_deducing_this.cpp`](examples/ch14_deducing_this.cpp).

---

## 10. Summary

<!--diagram
title: C++23 template features
box[green] Key points
  text: **Deducing this:** `template<class Self> ret f(this Self&& self){...}`
  text: One function for `&`/`const&`/`&&`/`const&&`
  text: Replaces CRTP (no `static_cast`)
  text: Enables recursive lambdas `[](this auto self,...){...}`
  text: `if consteval`: branch on constant-evaluation context
  text: `operator<=> = default`: generate all comparisons
  text: `auto(x)`: language-level decay-copy (safe copies in templates)
  text: `m[i, j]`: multidim subscript (`mdspan`, matrices)
  text: `std::mdspan`/`expected`: policy-based & templated vocabulary types
-->
```
 +------------------------------------------------------------------+
 | Deducing this: template<class Self> ret f(this Self&& self){...} |
 |   - one function for &/const&/&&/const&&                         |
 |   - replaces CRTP (no static_cast)                               |
 |   - enables recursive lambdas [](this auto self,...){...}        |
 | if consteval: branch on constant-evaluation context.             |
 | operator<=> = default: generate all comparisons.                 |
 | auto(x): language-level decay-copy (safe copies in templates).   |
 | m[i, j]: multidim subscript (mdspan, matrices).                  |
 | std::mdspan/expected: policy-based & templated vocabulary types. |
 +------------------------------------------------------------------+
```

Next: [15-cpp26-features.md](15-cpp26-features.md).
