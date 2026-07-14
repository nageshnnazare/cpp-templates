# 06 — Variadic Templates & Parameter Packs

Variadic templates take **any number** of template arguments. They are how
`std::tuple`, `std::make_unique`, `printf`-safe formatting, and `std::variant`
are built. This chapter makes packs intuitive.

---

## 1. The three pack-related syntaxes

```
   template <class... Ts>   // 1. TEMPLATE parameter pack (declares Ts)
   void f(Ts... args);      // 2. FUNCTION parameter pack (declares args)
   //            args...    // 3. PACK EXPANSION (uses them, adds ...)
```

```
   template <class... Ts>        Ts   = {int, double, char}   (a list of TYPES)
   void f(Ts... args)            args = {1,   2.0,    'x'}     (a list of VALUES)

   ...on the LEFT of a name  -> "this is a pack"
   ...on the RIGHT (after use) -> "expand the pack here"
```

Mnemonic: **`...` before = "pack it"; `...` after = "unpack it".**

---

## 2. `sizeof...` — how many?

```cpp
template <class... Ts>
constexpr std::size_t count(Ts... /*args*/) {
    return sizeof...(Ts);     // number of elements in the pack (compile time)
}
count(1, 2.0, 'x');           // 3
```

```
   sizeof...(Ts)   -> 3     (count of the TYPE pack)
   sizeof...(args) -> 3     (count of the VALUE pack, same number)
```

---

## 3. Pack expansion: the mental model

A pattern followed by `...` is **repeated for each element**, separated by
commas.

```
   pattern...     where pattern mentions the pack

   f(args...)              ->  f(a0, a1, a2)
   f(g(args)...)           ->  f(g(a0), g(a1), g(a2))
   f(args + 1 ...)         ->  f(a0+1, a1+1, a2+1)
   std::tuple<Ts...>       ->  std::tuple<T0, T1, T2>
   h(std::forward<Ts>(args)...) -> h(fwd<T0>(a0), fwd<T1>(a1), fwd<T2>(a2))
```

The pattern can be arbitrarily complex, as long as the pack appears in it.

```cpp
template <class... Ts>
void print_all(Ts... args) {
    (std::cout << ... << args) << '\n';   // fold (see §5)
}
```

---

## 4. The classic recursion pattern (pre-C++17 and still useful)

Before fold expressions, you processed packs with **head/tail recursion** and a
base case:

```cpp
// base case: no arguments
void print() { std::cout << '\n'; }

// recursive case: peel off the FIRST argument, recurse on the REST
template <class First, class... Rest>
void print(const First& first, const Rest&... rest) {
    std::cout << first;
    if (sizeof...(rest) > 0) std::cout << ", ";
    print(rest...);          // recurse with one fewer argument
}

print(1, 2.5, "hi");         // 1, 2.5, hi
```

```
   print(1, 2.5, "hi")
     first=1, rest={2.5,"hi"}   -> prints 1, ->  print(2.5,"hi")
                                                   first=2.5, rest={"hi"}
                                                     prints 2.5, -> print("hi")
                                                        first="hi", rest={}
                                                          prints hi -> print()
                                                                        prints \n
   Each call strips one element until the base case.
```

```
   Argument list shrinks by 1 each recursion:
     [1, 2.5, "hi"]  ->  [2.5, "hi"]  ->  ["hi"]  ->  []  (base case)
```

---

## 5. Fold expressions (C++17) — the modern way

Fold expressions collapse a pack with a **binary operator**, no recursion
needed. Four forms:

```
   ( pack op ... )            unary  RIGHT fold:  a0 op (a1 op (a2 op a3))
   ( ... op pack )            unary  LEFT  fold:  ((a0 op a1) op a2) op a3
   ( pack op ... op init )    binary RIGHT fold:  a0 op (a1 op (a2 op init))
   ( init op ... op pack )    binary LEFT  fold:  ((init op a0) op a1) op a2
```

```cpp
template <class... Ts>
auto sum(Ts... xs) { return (xs + ... + 0); }     // binary left fold, init 0

template <class... Ts>
bool all_true(Ts... xs) { return (xs && ...); }   // unary right fold

template <class... Ts>
void print(Ts... xs) { ((std::cout << xs << ' '), ...); } // comma fold
```

```
   sum(1,2,3,4)      = ((((0)+1)+2)+3)+4 ... = 10   (empty pack -> 0, safe)
   all_true(a,b,c)   = a && (b && c)
   print(1,'x',2.5)  = (cout<<1<<' '), (cout<<'x'<<' '), (cout<<2.5<<' ')
```

### Why the init value matters (empty packs)

```
   Unary fold on EMPTY pack is only valid for &&, ||, and comma:
      (xs && ...) on empty  -> true
      (xs || ...) on empty  -> false
      (xs , ...)  on empty  -> void()
   For anything else (+, *, ...), an empty pack is ILL-FORMED.
   -> use the BINARY form with an identity:  (xs + ... + 0)
```

### Comma fold: "do X for each element"

The comma-fold idiom runs a statement per element — the workhorse for side
effects:

```cpp
template <class... Ts>
void call_each(Ts&&... xs) {
    (void(some_action(std::forward<Ts>(xs))), ...);   // action per element, in order
}
```

---

## 6. Where packs can appear

```
   * function arguments:      f(Ts... args)
   * template argument lists:  std::tuple<Ts...>, Base<Ts>...
   * base class lists:         struct D : Bases... {};       (variadic bases!)
   * initializer lists:        int a[] = { xs... };
   * lambda captures (C++20):  [...xs = std::move(xs)]{}      (pack capture)
   * using-declarations:       using Bases::operator()...;    (C++17)
```

Variadic bases + `using` is the trick behind the **overload-set / overloaded
visitor** idiom for `std::variant`:

```cpp
template <class... Fs>
struct overloaded : Fs... {           // inherit from every lambda
    using Fs::operator()...;          // bring all their operator() into scope
};
template <class... Fs> overloaded(Fs...) -> overloaded<Fs...>;  // CTAD guide

// usage with std::visit:
// std::visit(overloaded{
//     [](int i){ ... },
//     [](const std::string& s){ ... },
// }, my_variant);
```

```
   overloaded{f1, f2, f3}
        inherits f1, f2, f3
        using f1::(), f2::(), f3::()  -> one object with 3 call operators
        std::visit picks the matching operator() for the active alternative
```

---

## 7. Indexing a pack (before C++26)

Packs have **no direct subscript** before C++26. Common workarounds:

```cpp
// (a) via std::tuple + std::get
template <std::size_t I, class... Ts>
decltype(auto) nth(Ts&&... xs) {
    return std::get<I>(std::forward_as_tuple(std::forward<Ts>(xs)...));
}
nth<1>(10, 20, 30);   // 20
```

C++26 finally adds **native pack indexing** — see
[15-cpp26-features.md](15-cpp26-features.md):

```cpp
// C++26:
template <class... Ts>
auto first(Ts... xs) { return xs...[0]; }     // direct pack indexing!
```

```
   Pre-C++26:  xs...[0]  did NOT exist -> tuple gymnastics
   C++26:      xs...[0], Ts...[0]  are built in.
```

---

## 8. Multiple packs & zipping

You can expand two packs together if they have the **same length**; the pattern
must mention both:

```cpp
template <class... As, class... Bs>
void zip_print(std::tuple<As...> a, std::tuple<Bs...> b);   // (illustrative)

// Common real pattern: index_sequence to iterate a tuple
template <class Tuple, std::size_t... Is>
void print_tuple_impl(const Tuple& t, std::index_sequence<Is...>) {
    ((std::cout << std::get<Is>(t) << ' '), ...);   // expand Is pack
}
template <class... Ts>
void print_tuple(const std::tuple<Ts...>& t) {
    print_tuple_impl(t, std::index_sequence_for<Ts...>{});
}
```

```
   std::index_sequence_for<int,char,double>{}  ==  index_sequence<0,1,2>
        |
   print_tuple_impl(t, index_sequence<0,1,2>)
        Is... = 0,1,2
        (cout<<get<0>(t)), (cout<<get<1>(t)), (cout<<get<2>(t))
```

`std::index_sequence` / `std::make_index_sequence` (C++14) are the standard tool
to turn a pack into compile-time indices — essential for tuple manipulation.

---

## 9. Variadic class template: a tiny tuple

```cpp
// Recursive tuple: a head element + a tuple of the rest.
template <class... Ts> struct Tuple;                 // primary decl

template <>                                          // base: empty tuple
struct Tuple<> {};

template <class Head, class... Tail>                 // recursive layout
struct Tuple<Head, Tail...> : Tuple<Tail...> {
    Head head;
    Tuple(Head h, Tail... t) : Tuple<Tail...>(t...), head(h) {}
};
```

```
   Tuple<int,char,double>
     : Tuple<char,double>          head=int
         : Tuple<double>           head=char
             : Tuple<>             head=double
   Layout is a chain of single-element bases (real std::tuple is flatter,
   but the idea is the same).
```

Runnable: [`examples/ch06_tuple.cpp`](examples/ch06_tuple.cpp) and
[`examples/ch06_fold_print.cpp`](examples/ch06_fold_print.cpp).

---

## 10. Perfect-forwarding a pack (preview)

The canonical variadic + forwarding combo (details in
[09-perfect-forwarding.md](09-perfect-forwarding.md)):

```cpp
template <class T, class... Args>
std::unique_ptr<T> make_unique_like(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
```

```
   Args&&... args     -> a pack of forwarding references
   std::forward<Args>(args)...   -> forwards each element preserving value cat.
   new T( fwd<A0>(a0), fwd<A1>(a1), ... )
```

---

## 11. Summary

<!--diagram
title: Variadic templates
box[green] Packs
  text: `template<class... Ts>` → type pack; `f(Ts... a)` → value pack
  text: `...` before = declare pack; `...` after = expand
  text: `sizeof...(Ts)` = element count
box[green] Process packs via
  text: Recursion (head/tail + base case) — classic
  text: Fold expressions (C++17) — **preferred**
  text: Comma fold for per-element side effects
  text: `index_sequence` for tuple iteration
  text: C++26: native pack indexing `xs...[i]`, `Ts...[i]`
-->
```
 +------------------------------------------------------------------+
 | template<class... Ts>  -> type pack.   f(Ts... a) -> value pack. |
 | '...' before = declare pack; '...' after = expand.               |
 | sizeof...(Ts) = element count.                                   |
 |                                                                  |
 | Process packs via:                                               |
 |   * recursion (head/tail + base case)  -- classic                |
 |   * fold expressions (C++17)           -- preferred              |
 |       (xs op ... op init) etc.                                   |
 |   * comma fold for per-element side effects                      |
 |   * index_sequence for tuple iteration                           |
 |                                                                  |
 | C++26: native pack indexing  xs...[i], Ts...[i].                 |
 +------------------------------------------------------------------+
```

Next: [07-sfinae.md](07-sfinae.md).
