# 11 — Template Metaprogramming (TMP)

Metaprogramming = **computing with types and compile-time values**. The compiler
becomes your interpreter. This chapter shows the classic "types as functional
programming" style, then the modern `constexpr` style that largely replaces it.

---

## 1. Two eras of compile-time computation

```
   CLASSIC TMP (C++98..)         MODERN (C++14/17/20..)
   -----------------------       --------------------------
   recursion via templates       constexpr functions (normal code!)
   ::value / ::type              return values / return types
   specialization = "if"         if / if constexpr
   type lists                    still templates, but simpler helpers
   HARD to read                  reads like ordinary C++
```

Rule of thumb: **for VALUES, use `constexpr` functions. For TYPES, use templates
(type traits, type lists).** Reach for classic recursive TMP only when you truly
manipulate types.

---

## 2. Values: the classic factorial (for contrast)

```cpp
// CLASSIC TMP: compute factorial with template recursion
template <unsigned N>
struct Factorial { static constexpr unsigned value = N * Factorial<N-1>::value; };
template <>
struct Factorial<0> { static constexpr unsigned value = 1; };   // base case

static_assert(Factorial<5>::value == 120);
```

```
   Factorial<5>::value
     = 5 * Factorial<4>::value
     = 5 * 4 * Factorial<3>::value
     = ... = 5*4*3*2*1 * Factorial<0>::value
     = 120 * 1
   Each Factorial<N> is a distinct TYPE instantiated by the compiler.
   Recursion = template instantiation chain. Base case = specialization.
```

### The modern way (just write a function!)

```cpp
constexpr unsigned factorial(unsigned n) {
    unsigned r = 1;
    for (unsigned i = 2; i <= n; ++i) r *= i;
    return r;
}
static_assert(factorial(5) == 120);   // runs at compile time
```

```
   No templates, no specialization, no ::value. Ordinary loop.
   -> ALWAYS prefer this for value computation. See chapter 12.
```

---

## 3. Types: the real domain of TMP

`constexpr` can't (before reflection) *produce types*. For type-level work,
templates remain the tool. Core operations:

```
   TYPE-LEVEL "FUNCTION"     realized as
   ----------------------    ------------------------------
   f(T) = U                  template<class T> struct F { using type = U; };
   apply                     typename F<T>::type   (or F_t<T>)
   branch                    specialization / conditional_t
   recursion                 F<T> refers to F<smaller>
   list of types             template<class...> struct TypeList {};
```

---

## 4. Type lists

A type list is just a variadic template used as a container of types:

```cpp
template <class... Ts> struct TypeList {};

using L = TypeList<int, double, char>;
```

### Length

```cpp
template <class L> struct Length;
template <class... Ts> struct Length<TypeList<Ts...>>
    : std::integral_constant<std::size_t, sizeof...(Ts)> {};

static_assert(Length<L>::value == 3);
```

### Front / PushFront

```cpp
template <class L> struct Front;
template <class H, class... T> struct Front<TypeList<H, T...>> { using type = H; };

template <class New, class L> struct PushFront;
template <class New, class... Ts>
struct PushFront<New, TypeList<Ts...>> { using type = TypeList<New, Ts...>; };
```

```
   TypeList<int,double,char>
     Front       -> int
     PushFront<bool>  -> TypeList<bool,int,double,char>
     Length      -> 3
   Pattern-matching on TypeList<Ts...> destructures the list.
```

### Map (transform every type)

```cpp
template <template <class> class F, class L> struct Map;
template <template <class> class F, class... Ts>
struct Map<F, TypeList<Ts...>> { using type = TypeList<typename F<Ts>::type...>; };

// apply std::add_pointer to every element:
using Ptrs = Map<std::add_pointer, L>::type;   // TypeList<int*, double*, char*>
```

```
   Map<add_pointer, TypeList<int,double,char>>
        F applied to each:  add_pointer<int>::type, add_pointer<double>::type, ...
        -> TypeList<int*, double*, char*>
   The pattern F<Ts>::type... is a pack expansion over the list.
```

### Contains / IndexOf (recursion + branch)

```cpp
template <class T, class L> struct Contains : std::false_type {};
template <class T, class H, class... Ts>
struct Contains<T, TypeList<H, Ts...>>
    : std::conditional_t<std::is_same_v<T, H>,
                         std::true_type,
                         Contains<T, TypeList<Ts...>>> {};   // recurse on tail
template <class T> struct Contains<T, TypeList<>> : std::false_type {}; // base

static_assert( Contains<double, L>::value);
static_assert(!Contains<float,  L>::value);
```

```
   Contains<double, TypeList<int,double,char>>
     H=int != double -> recurse Contains<double, TypeList<double,char>>
       H=double == double -> true_type
   Walk the list, branch with conditional_t, stop at empty base case.
```

---

## 5. `std::integer_sequence` — compile-time index generation

The standard's tool for turning a count into a pack of indices `0,1,...,N-1`.
Essential for tuple/array algorithms.

```cpp
#include <utility>

template <class T, T... Is>
void show(std::integer_sequence<T, Is...>) {
    ((std::cout << Is << ' '), ...);       // fold over the indices
}
show(std::make_integer_sequence<int, 5>{});   // 0 1 2 3 4
```

```
   std::make_index_sequence<5>  ==  index_sequence<0,1,2,3,4>
        |
   captured as a pack Is... = 0,1,2,3,4
        |
   fold/expand over Is to touch each index at compile time.
```

Turning a runtime array into a tuple, applying a function to each tuple element,
initializing an `std::array` from a generator — all use `index_sequence`.

```cpp
// call f(0), f(1), ..., f(N-1) at compile-time-unrolled positions
template <class F, std::size_t... Is>
void for_each_index(F&& f, std::index_sequence<Is...>) {
    (f(std::integral_constant<std::size_t, Is>{}), ...);
}
template <std::size_t N, class F>
void repeat(F&& f) { for_each_index(std::forward<F>(f), std::make_index_sequence<N>{}); }
```

---

## 6. Iterating a `std::tuple`

Combining `index_sequence` + fold to visit every element (heterogeneous!):

```cpp
#include <tuple>
#include <iostream>

template <class Tuple, class F, std::size_t... Is>
void tuple_for_each_impl(Tuple&& t, F&& f, std::index_sequence<Is...>) {
    (f(std::get<Is>(std::forward<Tuple>(t))), ...);   // comma fold over indices
}
template <class Tuple, class F>
void tuple_for_each(Tuple&& t, F&& f) {
    constexpr auto N = std::tuple_size_v<std::remove_reference_t<Tuple>>;
    tuple_for_each_impl(std::forward<Tuple>(t), std::forward<F>(f),
                        std::make_index_sequence<N>{});
}

int main() {
    auto t = std::make_tuple(1, 2.5, "hi");
    tuple_for_each(t, [](const auto& x){ std::cout << x << ' '; });  // 1 2.5 hi
}
```

```
   tuple_size = 3  -> index_sequence<0,1,2>
   fold:  f(get<0>(t)), f(get<1>(t)), f(get<2>(t))
   The lambda is a generic (templated) call operator -> handles int, double, char*.
```

Runnable: [`examples/ch11_tuple_for_each.cpp`](examples/ch11_tuple_for_each.cpp).

---

## 7. `constexpr` "type computation" is coming: reflection (C++26)

Historically, you *cannot* compute a type with ordinary `constexpr` code — hence
all the template metaprogramming above. **C++26 static reflection** changes this:
you can reflect a type into a `std::meta::info` value, manipulate it with normal
`constexpr` code, and *splice* it back into a type. This will replace much
classic TMP. Preview in [15-cpp26-features.md](15-cpp26-features.md):

```cpp
// C++26 (conceptual): iterate members with ordinary constexpr code
// template <class T> constexpr auto member_count() {
//     return std::meta::nonstatic_data_members_of(^^T).size();
// }
```

```
   Classic TMP:   types manipulated by RECURSIVE TEMPLATES (hard).
   C++26 reflect: types -> values (std::meta::info) -> normal constexpr -> types.
```

---

## 8. TMP performance & debugging notes

```
  * Template instantiation is COMPILE-TIME work. Deep recursion (e.g.
    Factorial<10000>) can blow compile time and hit instantiation-depth limits
    (typically ~900-1024). Prefer constexpr functions for values.
  * Each distinct instantiation is memoized by the compiler, but distinct
    argument sets multiply. Type lists with O(N^2) algorithms get slow fast.
  * Fold expressions & pack tricks are usually cheaper to compile than deep
    recursion (fewer instantiations).
  * Debug with: static_assert(std::is_same_v<Computed, Expected>);
    or the "incomplete type probe" from chapter 04.
```

---

## 9. When to use TMP vs alternatives

```
  Task                                Best tool
  ---------------------------------   -------------------------------------
  compute a NUMBER at compile time    constexpr/consteval function (ch 12)
  compute a TYPE                       type traits / type lists (this ch)
                                       -> C++26 reflection when available
  select code path on a trait          if constexpr (ch 12) / concepts (ch 13)
  detect capability                    requires / concepts (ch 13)
  generate indices                     std::index_sequence
  operate over a tuple/pack            index_sequence + fold (ch 06)
```

The modern lesson: **most of what needed classic TMP now has a friendlier tool.**
Learn TMP to read older/expert code and for genuine type-level work; write new
value logic as `constexpr`.

---

## 10. Summary

<!--diagram
title: Template metaprogramming
box[green] Key points
  text: TMP = computing with types & compile-time values
  text: **VALUES** → `constexpr` functions (preferred)
  text: **TYPES** → templates: traits, `TypeList<Ts...>`, recursion + specialization for branching/base cases
  text: TypeList ops: Length, Front, PushFront, Map, Contains...
  text: `std::index_sequence` turns N into a pack of 0..N-1
  text: `tuple_for_each` = `index_sequence` + comma fold
  text: C++26 reflection will replace much classic type-level TMP
  text: Watch instantiation depth & compile-time cost
-->
```
 +------------------------------------------------------------------+
 | TMP = computing with types & compile-time values.                |
 |   VALUES  -> constexpr functions (preferred).                    |
 |   TYPES   -> templates: traits, TypeList<Ts...>, recursion +     |
 |              specialization for branching/base cases.            |
 |                                                                  |
 | TypeList ops: Length, Front, PushFront, Map, Contains...         |
 | std::index_sequence turns N into a pack of 0..N-1.               |
 | tuple_for_each = index_sequence + comma fold.                    |
 | C++26 reflection will replace much classic type-level TMP.       |
 | Watch instantiation depth & compile-time cost.                   |
 +------------------------------------------------------------------+
```

Next: [12-constexpr-consteval.md](12-constexpr-consteval.md).
