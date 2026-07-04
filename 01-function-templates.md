# 01 — Function Templates

Function templates are the gateway. Master these and class templates become
"the same idea, on a type."

Prereq: [00-mental-model.md](00-mental-model.md).

---

## 1. Anatomy

```cpp
template <typename T>     // <-- template parameter list (the "head")
T maximum(T a, T b) {     // <-- the templated function (the "body")
    return (a < b) ? b : a;
}
```

```
 template < typename T >   T   maximum ( T a , T b )
 \_______/  \__________/   |   \_____/   \_________/
   keyword   parameter    return  name    function
             (T is a       type           parameters
              type name)
```

`typename` and `class` are **interchangeable** here:

```cpp
template <class T>   T maximum(T, T);   // identical meaning
template <typename T> T maximum(T, T);  // preferred by many for readability
```

Use `typename` for "any type"; some teams reserve `class` only when the argument
must be a class. Purely stylistic — the compiler treats them the same.

---

## 2. Calling: deduction vs explicit

```cpp
maximum(3, 7);         // T deduced as int      -> maximum<int>
maximum(3.5, 2.5);     // T deduced as double   -> maximum<double>
maximum<double>(3, 7); // T explicitly double; ints converted to double
maximum<int>(3, 7);    // T explicit int
```

```
 CALL                      DEDUCTION                RESULT
 maximum(3,7)          T <- int (both args int)     maximum<int>
 maximum(3.5,2.5)      T <- double                  maximum<double>
 maximum(3, 7.0)       CONFLICT: int vs double  --> COMPILE ERROR*
 maximum<double>(3,7.0) T forced double; convert    maximum<double>
```

\* The classic beginner error: both parameters use the *same* `T`, so mixed
arguments can't deduce a single `T`. Fixes:

```cpp
// (a) two type parameters
template <class A, class B>
auto maximum(A a, B b) { return (a < b) ? b : a; }   // return type deduced

// (b) force the type
maximum<double>(3, 7.0);
```

We dive into deduction in [04-argument-deduction.md](04-argument-deduction.md).

---

## 3. What actually gets generated

```
 SOURCE                            GENERATED (conceptually)
 --------------------------------  -----------------------------------
 maximum(3, 7);                    int    maximum(int a, int b){...}
 maximum(1.0, 2.0);                double maximum(double a,double b){...}
 maximum('a', 'b');                char   maximum(char a, char b){...}
```

Each distinct `T` yields a **separate function** in the binary (unless the
linker/optimizer merges identical code — "COMDAT folding"). Templates trade
**binary size** for **type generality** and **inlining opportunities**.

---

## 4. Multiple template parameters & return-type deduction

```cpp
template <class T, class U>
auto add(T a, U b) -> decltype(a + b) {   // trailing return type (C++11)
    return a + b;
}

template <class T, class U>
auto add2(T a, U b) {   // C++14: just 'auto', deduced from return statement
    return a + b;
}
```

```
  add(1, 2.5)
       |  T=int, U=double
       v
  decltype(a+b) = decltype(int + double) = double
       |
       v
  returns double (3.5)
```

`decltype(a+b)` in the trailing position can reference the parameters `a` and
`b` (they are in scope there). A leading `decltype(a+b) add(...)` cannot —
`a`/`b` aren't declared yet. That's the whole reason trailing return types exist.

---

## 5. Non-type template parameters (NTTP) on functions

Template parameters aren't only types — they can be **compile-time values**:

```cpp
template <int N, class T>
T power(T base) {
    T result = 1;
    for (int i = 0; i < N; ++i) result *= base;
    return result;
}

power<3>(2);   // N=3, T deduced int -> 8
```

```
 power<3>(2)
        |  |
        |  +-- runtime-ish arg 'base' = 2 (deduces T=int)
        +----- compile-time constant N = 3  (must be a constant expression)
```

More in [03-template-parameters.md](03-template-parameters.md).

---

## 6. Overloading function templates

You can overload:
- template with template,
- template with non-template.

```cpp
template <class T> void f(T)         { std::puts("1: T"); }
template <class T> void f(T*)        { std::puts("2: T*"); }
void                     f(int)      { std::puts("3: int (non-template)"); }
```

Resolution ranking (simplified):

```
 CALL         CANDIDATES that match         WINNER      WHY
 f(42)        1:T (T=int), 3:int            3:int       non-template beats
                                                        equally-good template
 f(4.2)       1:T (T=double)                1:T         only viable
 int* p; f(p) 1:T(T=int*), 2:T*(T=int)      2:T*        more SPECIALIZED template
```

Two rules to remember:

1. Between a function template and a non-template that are **equally good
   matches**, the **non-template wins**.
2. Between two function templates, the **more specialized** one wins (partial
   ordering of function templates). `f(T*)` is more specialized than `f(T)`.

```
   Overload resolution pipeline
   ----------------------------
   1. Gather candidates (templates get deduced first).
   2. Discard non-viable (args don't fit).
   3. Rank by conversions (exact > promotion > conversion...).
   4. Tie-break: non-template > template.
   5. Tie-break among templates: more specialized wins.
   6. Still tied -> ambiguous (error).
```

---

## 7. Explicit specialization of a function template

You can provide a special implementation for a specific type:

```cpp
template <class T> void print(T v) { std::cout << v << '\n'; }

template <>                        // full specialization
void print<bool>(bool v) { std::cout << (v ? "true" : "false") << '\n'; }
```

```
 print(42)     -> primary template  -> "42"
 print(true)   -> specialization     -> "true"
```

> **Best practice:** For functions, prefer **overloading** over explicit
> specialization. Specializations of function templates don't participate in
> overload resolution the way you'd expect and interact badly with overloads
> (see Sutter's "Why Not Specialize Function Templates?"). Overloads are cleaner.
> Full story in [05-specialization.md](05-specialization.md).

---

## 8. `inline`, ODR, and templates

Function templates (and their full specializations that are defined in headers)
are implicitly fine to appear in multiple translation units — instantiations are
**inline-like** for ODR purposes. You generally **don't** need to write
`inline`, but explicit *full specializations* defined in a header **do** need
`inline` (they are ordinary functions, not templates):

```cpp
// header.hpp
template <class T> void g(T) {}         // OK in many TUs (template)
template <> inline void g<int>(int) {}  // needs 'inline' (ordinary function)
```

---

## 9. `auto` parameters = abbreviated function templates (C++20)

Since C++20, a function with `auto` parameters **is** a function template:

```cpp
auto maximum(auto a, auto b) {          // == template<class T,class U>
    return (a < b) ? b : a;
}
```

```
 auto f(auto x, auto y)
   is EXACTLY
 template<class T, class U> auto f(T x, U y)
```

This is the "abbreviated function template" syntax. Great for short generic
lambdas and helpers. Combine with concepts: `void f(std::integral auto x)`.

---

## 10. Generic lambdas are function templates too

```cpp
auto add = [](auto a, auto b) { return a + b; };
add(1, 2);      // instantiates operator()<int,int>
add(1.0, 2.0);  // instantiates operator()<double,double>
```

Under the hood the closure type has a **templated `operator()`**:

```
 [](auto a, auto b){ return a+b; }
        desugars to
 struct __lambda {
     template<class T,class U>
     auto operator()(T a, U b) const { return a+b; }
 };
```

---

## 11. Worked example: a generic `for_each` clone

```cpp
#include <iostream>
#include <vector>

template <class Iter, class Func>
Func my_for_each(Iter first, Iter last, Func fn) {
    for (; first != last; ++first)
        fn(*first);
    return fn;   // standard library returns the functor
}

int main() {
    std::vector<int> v{1, 2, 3, 4};
    my_for_each(v.begin(), v.end(),
                [](int x){ std::cout << x * x << ' '; });
    std::cout << '\n';   // 1 4 9 16
}
```

```
 my_for_each(begin, end, lambda)
   Iter = std::vector<int>::iterator (deduced)
   Func = closure type of the lambda   (deduced)

   loop:  *first --> lambda(1) --> prints 1
                     lambda(2) --> prints 4
                     ...
```

Runnable: [`examples/ch01_for_each.cpp`](examples/ch01_for_each.cpp).

---

## 12. Summary

<!--diagram
title: Function templates
box[green] Key points
  text: `template<class T> ret f(params) { body }`
  text: `T` deduced from args, or given explicitly `f<T>(...)`
  text: Same `T` in two params → args must match (or force `f<T>`)
  text: Return type: trailing `decltype` (C++11) or `auto` (C++14)
  text: Overload > specialize for functions
  text: `auto` params (C++20) == abbreviated function template
  text: Definitions go in headers
-->
```
 +---------------------------------------------------------------+
 | template<class T> ret f(params) { body }                       |
 |                                                               |
 | * T deduced from args, or given explicitly f<T>(...).         |
 | * Same T in two params -> args must match (or force f<T>).    |
 | * Return type: trailing decltype (C++11) or auto (C++14).    |
 | * Overload > specialize for functions.                        |
 | * auto params (C++20) == abbreviated function template.       |
 | * Definitions go in headers.                                   |
 +---------------------------------------------------------------+
```

Next: [02-class-templates.md](02-class-templates.md).
