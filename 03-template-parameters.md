# 03 — Template Parameters (Type, Non-Type, Template-Template)

Templates take three *kinds* of parameters. Knowing each unlocks a lot of design
power (and lets you read the STL).

```
  KIND                     EXAMPLE PARAMETER          EXAMPLE ARGUMENT
  ----------------------   ------------------------   ----------------
  1. type parameter        template<class T>          int, std::string
  2. non-type (NTTP)       template<int N>            42, sizeof(x)
  3. template-template     template<template<class>   std::vector
                            class Container>
```

---

## 1. Type parameters

The one you know: `class`/`typename` introduce a **type**.

```cpp
template <class T>            // T is a type
template <typename T>        // same
template <class T = int>     // with a default
template <class... Ts>       // a type PACK (chapter 06)
```

---

## 2. Non-type template parameters (NTTP)

An NTTP is a **compile-time constant value** baked into the type.

### Allowed NTTP types

```
  Allowed as NTTP:
    * integral & enum:   int, std::size_t, char, enum E
    * pointer / reference to object or function (with linkage)
    * std::nullptr_t
    * (C++20) floating point:  double, float
    * (C++20) "structural" literal class types (see below)
    * auto NTTP (C++17): let the compiler deduce the value's type
```

```cpp
template <int N>                 struct A {};       // classic
template <std::size_t N>         struct Array {};   // sizes
template <bool Debug>            struct Logger {};  // flags
template <auto V>                struct Const {};   // C++17: any value
template <double D>              struct Ratio {};   // C++20: floating point
```

### Why NTTPs matter — zero-cost configuration

```cpp
template <std::size_t N>
struct FixedBuffer {
    char data[N];
    static constexpr std::size_t capacity = N;
};

FixedBuffer<64>  small;   // capacity encoded in the TYPE, no runtime field
FixedBuffer<4096> big;
```

<!--diagram
title: Zero-cost configuration via NTTP
box[blue] FixedBuffer<64>
  text: `char[64]`, cap = 64
box[blue] FixedBuffer<4096>
  text: `char[4096]`, cap = 4096
box[gray] Note
  text: Different types. Size known at compile time → no dynamic alloc.
-->
```
   FixedBuffer<64>            FixedBuffer<4096>
   +---------------+          +--------------------+
   | char[64]      |          | char[4096]         |
   | cap = 64      |          | cap = 4096         |
   +---------------+          +--------------------+
   Different types. size known at compile time -> no dynamic alloc.
```

### `auto` NTTP (C++17): heterogeneous constants

```cpp
template <auto Value>
struct Constant {
    static constexpr auto value = Value;
    using type = decltype(Value);
};

Constant<42>      c1;   // value: int   42
Constant<'x'>     c2;   // value: char  'x'
Constant<3.14>    c3;   // value: double 3.14 (C++20)
```

```
   Constant<42>   ->  type=int,   value=42
   Constant<'x'>  ->  type=char,  value='x'
   The parameter's TYPE is deduced from the argument.
```

This is how `std::integral_constant`-style helpers become effortless.

### Structural types as NTTPs (C++20) — compile-time strings!

C++20 lets a *literal class type* be an NTTP if it is **structural** (all members
public, all bases/members are themselves structural, etc.). The famous use is a
compile-time string:

```cpp
template <std::size_t N>
struct FixedString {
    char data[N]{};
    constexpr FixedString(const char (&s)[N]) {
        for (std::size_t i = 0; i < N; ++i) data[i] = s[i];
    }
};

template <FixedString S>          // <-- a STRING as a template argument!
struct Named {
    static constexpr auto name = S;
};

Named<"hello"> h;   // S.data == "hello"
```

```
   Named<"hello">
          |
          v   "hello" -> FixedString<6>{ 'h','e','l','l','o','\0' }
   Named<FixedString<6>{...}>   (a class-type NTTP)
```

This powers compile-time reflection-ish libraries, named tuples, format-string
checking, etc.

### NTTP gotchas

```
  * Must be a CONSTANT EXPRESSION:  A<x> where x is a runtime int -> ERROR.
  * Two arguments that "equal" but differ structurally can be different types.
  * String literals are arrays; you cannot pass a raw string literal to a
    template<const char*> because of linkage rules. Use FixedString instead.
```

---

## 3. Template-template parameters

Sometimes you want to accept **a template itself**, not an instantiated type.

```cpp
template <template <class, class> class Container, class T>
class Stack {
    Container<T, std::allocator<T>> data_;   // apply the template to T
public:
    void push(const T& x) { data_.push_back(x); }
    T&   top()            { return data_.back(); }
};

Stack<std::vector, int> s;   // Container = std::vector (the TEMPLATE, not vector<int>)
```

```
   Stack<std::vector, int>
          |            |
          |            +-- T = int
          +--------------- Container = the template 'std::vector'
                            (arity must match: vector<T, Alloc>)

   inside:  Container<T, alloc>  ==  std::vector<int, alloc>
```

### The arity-matching pain, and the C++17 fix

Pre-C++17, the parameter list of the template-template argument had to match
*exactly*, which was brittle (e.g. `std::vector` actually has more defaulted
params). Two robust patterns:

```cpp
// (a) Accept a variadic template-template parameter (flexible arity):
template <template <class...> class Container, class T>
class Stack2 {
    Container<T> data_;      // let Container fill its own defaults
public:
    void push(const T& x) { data_.push_back(x); }
};

Stack2<std::vector, int> s;   // works cleanly
Stack2<std::deque,  int> d;   // and swap the container easily
```

```
   template<template<class...> class Container, class T>
                     ^^^^^^^^
        variadic -> matches vector, deque, list, ... regardless of
        how many defaulted params they really have.
```

Use template-template parameters when the *identity of the template* matters
(you want to re-apply it to different `T`s). If you only need a container type,
just take a plain `class Container` type parameter.

---

## 4. `typename` vs `class` in template-template parameters

Before C++17 you had to write `class` for the inner keyword; since C++17
`typename` is also allowed:

```cpp
template <template <class...> typename Container>   // C++17 OK
```

---

## 5. Mixing kinds & defaults

Parameters can be any mix; defaults follow the same "once defaulted, rest must
be" rule:

```cpp
template <class T,                    // type
          std::size_t N = 16,         // NTTP with default
          template <class...> class Alloc = std::allocator>  // template-template
class SmallVector { /* ... */ };

SmallVector<int> v;               // T=int, N=16, Alloc=std::allocator
SmallVector<int, 32> v2;          // override N
```

---

## 6. Parameter packs are a fourth "dimension"

Any of the three kinds can be a **pack** (`...`). This is variadic templates,
covered fully in [06-variadic-templates.md](06-variadic-templates.md):

```cpp
template <class... Ts>        struct Tuple {};        // type pack
template <int... Ns>          struct IntSeq {};       // NTTP pack
template <template <class...> class... Cs> struct X {}; // template-template pack
```

---

## 7. Decision guide

```
  Need to parameterize on...        Use...
  -------------------------------   ----------------------------------
  a type                            type parameter    <class T>
  a compile-time number/flag        NTTP              <int N> / <auto V>
  a compile-time string             FixedString NTTP  <FixedString S> (C++20)
  "the template itself" to reapply  template-template <template<class...> class C>
  just a container/collaborator     type parameter    <class Container>  (simpler!)
  variable count of any above       pack              <class... Ts>
```

> Rule of thumb: reach for template-template parameters **only** when you truly
> need to instantiate the passed template with your own arguments. Otherwise a
> plain type parameter is simpler and gives better error messages.

---

## 8. Worked example: a compile-time `Ratio`

```cpp
#include <iostream>
#include <numeric>   // std::gcd (C++17)

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
    using Sum   = RatioAdd<Half, Third>;         // 5/6
    std::cout << Sum::num << "/" << Sum::den      // 5/6
              << " = " << Sum::value << '\n';     // 0.833333
}
```

```
   Half = 1/2, Third = 1/3
   Sum  = (1*3 + 1*2) / (2*3) = 5/6   (all computed at COMPILE TIME)
```

Runnable: [`examples/ch03_ratio.cpp`](examples/ch03_ratio.cpp).

---

## 9. Summary

<!--diagram
title: Template parameters
box[green] Three parameter kinds
  text: **type** — `<class T>`
  text: **non-type** — `<int N>`, `<auto V>`, `<double D>` (C++20), `<FixedString S>` (C++20 class NTTP)
  text: **template-template** — `<template<class...> class C>`
box[green] Rules
  text: Any kind can be a pack (`...`). Defaults follow function rules.
  text: Prefer plain type params unless you must reapply the template.
-->
```
 +----------------------------------------------------------------+
 | 3 parameter kinds:                                             |
 |   type          <class T>                                       |
 |   non-type      <int N>, <auto V>, <double D>(C++20),          |
 |                 <FixedString S>(C++20 class NTTP)              |
 |   template-tmpl <template<class...> class C>                    |
 |                                                                |
 | Any kind can be a pack (...). Defaults follow function rules.  |
 | Prefer plain type params unless you must reapply the template. |
 +----------------------------------------------------------------+
```

Next: [04-argument-deduction.md](04-argument-deduction.md).
