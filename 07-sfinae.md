# 07 — SFINAE (Substitution Failure Is Not An Error)

SFINAE is the pre-C++20 technique for **conditionally enabling** templates based
on properties of their arguments. Concepts (chapter 13) largely replace it, but
you must understand SFINAE to read existing code, and it still has niche uses.

---

## 1. The one sentence definition

> When the compiler substitutes deduced template arguments into a candidate's
> **signature**, if that substitution produces an **invalid type or expression
> in the immediate context**, the candidate is silently **removed** from the
> overload set — instead of causing a hard error.

```
   Overload set: [f<T=A>, g<T=A>, ...]
        |
   substitute T=A into each signature
        |
   f<A>: signature is ill-formed  --> DROP f (not an error)
   g<A>: signature is fine        --> keep g
        |
   pick best of remaining viable candidates
```

"**Not an error**" is the whole point: a failed substitution just prunes the
candidate list. If *nothing* is left, *then* you get an error.

---

## 2. Minimal example: enable different overloads

```cpp
#include <type_traits>
#include <iostream>

// enabled only when T is integral
template <class T,
          std::enable_if_t<std::is_integral_v<T>, int> = 0>
void describe(T) { std::cout << "integral\n"; }

// enabled only when T is floating point
template <class T,
          std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
void describe(T) { std::cout << "floating\n"; }

int main() {
    describe(42);    // integral
    describe(3.14);  // floating
}
```

```
   describe(42), T=int:
     candidate 1: enable_if_t<true , int> = int    -> valid   -> KEEP
     candidate 2: enable_if_t<false, int> = ???     -> INVALID -> DROP
   Only candidate 1 remains -> "integral"
```

---

## 3. How `enable_if` works (build it yourself)

`std::enable_if<Cond, T>` has a member `type` **only when `Cond` is true**.
Asking for `::type` when it's absent is a substitution failure.

```cpp
template <bool Cond, class T = void>
struct enable_if {};                       // NO 'type' member

template <class T>
struct enable_if<true, T> { using type = T; };   // has 'type' only when true

template <bool Cond, class T = void>
using enable_if_t = typename enable_if<Cond, T>::type;
```

```
   enable_if<true , X>::type  = X       (specialization has 'type')
   enable_if<false, X>::type  = ERROR   (primary has no 'type' -> SFINAE)
```

So `enable_if_t<false, ...>` is a *type that doesn't exist* -> substitution
fails -> candidate dropped.

---

## 4. Where to put the `enable_if` — three placements

```cpp
// (a) as a defaulted non-type template parameter (preferred, most flexible)
template <class T, std::enable_if_t<cond<T>, int> = 0>
void f(T);

// (b) as the return type
template <class T>
std::enable_if_t<cond<T>, void> g(T);

// (c) as a function parameter (rare, changes signature)
template <class T>
void h(T, std::enable_if_t<cond<T>, int> = 0);
```

```
   Placement (a): parameter list      -> keeps return type clean, works on ctors
   Placement (b): return type         -> can't use on constructors (no ret type)
   Placement (c): extra param         -> pollutes the call signature
   -> (a) is the modern SFINAE-with-enable_if default.
```

Note: two overloads that differ *only* by the *type* used inside `enable_if`
default-value can clash (they look identical to the compiler). Use distinct
conditions so exactly one is viable.

---

## 5. Expression SFINAE — "does this operation compile?"

The most powerful form: put an **expression** in the signature (usually via
`decltype`). If the expression is ill-formed for `T`, the candidate drops.

```cpp
#include <utility>   // std::declval

// enabled only if T has a member .size()
template <class T>
auto get_size(const T& c) -> decltype(c.size()) {
    return c.size();
}
```

```
   get_size(vec)     decltype(vec.size()) is valid   -> return type = size_t
   get_size(42)      decltype((42).size()) invalid   -> candidate DROPPED
                     -> if no other overload: "no matching function" error
```

`std::declval<T>()` gives you a "fake value of type T" usable **only** inside
unevaluated contexts (`decltype`, `sizeof`, `noexcept`) to probe operations
without needing a real constructible object:

```cpp
template <class T, class U>
auto add(T a, U b) -> decltype(std::declval<T>() + std::declval<U>());
```

---

## 6. `void_t` — the detection Swiss-army knife (C++17)

`std::void_t<...>` maps any valid types to `void`. If any type inside is
ill-formed, SFINAE kicks in. This gives a clean way to detect member presence.

```cpp
#include <type_traits>

template <class T, class = void>
struct has_value_type : std::false_type {};        // primary: "no"

template <class T>
struct has_value_type<T, std::void_t<typename T::value_type>>
    : std::true_type {};                           // matches only if T::value_type exists
```

```
   has_value_type<std::vector<int>>
       does typename vector<int>::value_type exist? YES
       -> void_t<...> = void -> partial spec matches -> true_type

   has_value_type<int>
       does int::value_type exist? NO -> void_t ill-formed
       -> partial spec dropped -> primary -> false_type
```

```
                has_value_type<T>
                       |
        +--------------+---------------+
        | T::value_type valid?         |
       yes                             no
        |                              |
   partial spec (true_type)      primary (false_type)
```

---

## 7. The Detection Idiom (a reusable generalization)

`void_t` boilerplate is repetitive; the detection idiom packages it. A common
hand-rolled version:

```cpp
#include <type_traits>

namespace detail {
    template <class Default, class AlwaysVoid,
              template <class...> class Op, class... Args>
    struct detector : std::false_type { using type = Default; };

    template <class Default, template <class...> class Op, class... Args>
    struct detector<Default, std::void_t<Op<Args...>>, Op, Args...>
        : std::true_type { using type = Op<Args...>; };
}

template <template <class...> class Op, class... Args>
using is_detected =
    detail::detector<void, void, Op, Args...>;

template <template <class...> class Op, class... Args>
inline constexpr bool is_detected_v = is_detected<Op, Args...>::value;

// Define an "operation" to detect:
template <class T> using has_push_back_t =
    decltype(std::declval<T&>().push_back(std::declval<typename T::value_type>()));

// Query:
// static_assert( is_detected_v<has_push_back_t, std::vector<int>>);
// static_assert(!is_detected_v<has_push_back_t, std::array<int,3>>);
```

```
   is_detected_v<Op, Args...>
        try to form Op<Args...>
          success -> true       (and ::type is the result type)
          failure -> false
   You write Op as "the expression you want to test" wrapped in decltype.
```

> In C++20, you'd just write a `requires`-expression / concept instead. The
> detection idiom is the C++17 way and appears in many libraries.

---

## 8. `if constexpr` vs SFINAE vs concepts — when to use which

```
  GOAL                                 BEST TOOL (modern -> legacy)
  ----------------------------------   -----------------------------------
  branch inside ONE function           if constexpr (C++17)
  enable/disable an overload           concepts (C++20) > enable_if (SFINAE)
  detect a member/operation            requires-expr (C++20) > void_t (C++17)
  constrain a class template           concepts / requires > enable_if
  read old library code                understand SFINAE (this chapter)
```

Same "integral vs floating" example in each style:

```cpp
// SFINAE (C++11/14/17)
template <class T, std::enable_if_t<std::is_integral_v<T>,int> = 0>
void f(T);

// if constexpr (C++17) — single function
template <class T> void f(T x) {
    if constexpr (std::is_integral_v<T>) { /* ... */ }
    else                                 { /* ... */ }
}

// concepts (C++20) — cleanest
template <std::integral T> void f(T);
void f(std::integral auto x);            // abbreviated form
```

---

## 9. SFINAE pitfalls

```
  * "Hard errors" outside the immediate context are NOT SFINAE.
      Example: an error inside a function BODY is a real error, not SFINAE.
      Only failures in the SIGNATURE (types/expressions in the declaration)
      participate.

  * Two enable_if overloads with the SAME defaulted-type parameter
      (both '... int = 0') are the SAME signature -> redefinition error.
      Make the CONDITIONS mutually exclusive but the SIGNATURES distinct,
      or vary the enable_if second argument type.

  * Diagnostic quality is poor: SFINAE removals are silent, so "no matching
      function" hides WHY. Concepts give far better messages.
```

Immediate-context example:

```cpp
template <class T>
auto f(T x) -> decltype(x.foo()) {   // SFINAE: dropped if no foo()
    x.bar();                         // if x has foo() but not bar(),
                                     // THIS is a HARD ERROR (body, not signature)
    return x.foo();
}
```

---

## 10. Worked example: pick print based on capability

```cpp
#include <iostream>
#include <type_traits>
#include <vector>

// (1) has operator<<  ->  print directly
template <class T>
auto print(const T& x) -> decltype(std::cout << x, void()) {
    std::cout << x << '\n';
}

// (2) otherwise, if iterable (has begin/end) -> print elements
template <class T>
auto print(const T& c) -> decltype(c.begin(), c.end(), void()) {
    std::cout << "[ ";
    for (const auto& e : c) std::cout << e << ' ';
    std::cout << "]\n";
}

int main() {
    print(42);                          // uses (1): 42
    print(std::vector<int>{1, 2, 3});   // (2) is chosen for containers
}
```

> Note: overloads (1) and (2) can be ambiguous for types that are *both*
> streamable and iterable (like `std::string`). Real code disambiguates with
> priorities/tags or concepts. This shows the mechanism; chapter 13 shows the
> clean fix.

Runnable: [`examples/ch07_sfinae.cpp`](examples/ch07_sfinae.cpp).

---

## 11. Summary

<!--diagram
title: SFINAE
box[green] Core idea
  text: SFINAE: a bad substitution in a **signature** removes a candidate instead of erroring
box[green] Tools
  text: `enable_if_t<cond, T>` → gate overloads (put in template arg)
  text: `decltype(expr)` → expression SFINAE ("does it compile")
  text: `declval<T>()` → fake value in unevaluated context
  text: `void_t<...>` → member/type detection
  text: Detection idiom → reusable `is_detected<Op, Args...>`
box[orange] Caveat
  text: Only **immediate-context** (signature) failures count; body errors are hard errors
  text: Prefer **concepts** in C++20+
-->
```
 +------------------------------------------------------------------+
 | SFINAE: a bad substitution in a SIGNATURE removes a candidate    |
 |         instead of erroring.                                     |
 |                                                                  |
 | Tools:                                                           |
 |   enable_if_t<cond, T>   -> gate overloads (put in template arg) |
 |   decltype(expr)         -> expression SFINAE ("does it compile")|
 |   declval<T>()           -> fake value in unevaluated context    |
 |   void_t<...>            -> member/type detection                |
 |   detection idiom        -> reusable is_detected<Op, Args...>    |
 |                                                                  |
 | Only IMMEDIATE-CONTEXT (signature) failures count; body errors  |
 | are hard errors. Prefer CONCEPTS in C++20+.                     |
 +------------------------------------------------------------------+
```

Next: [08-type-traits.md](08-type-traits.md).
