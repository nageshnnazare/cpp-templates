# 08 — Type Traits (`<type_traits>`)

Type traits are compile-time **questions about types** ("is this an integer?
does it have a virtual destructor?") and **type transformations** ("remove the
const", "add a pointer"). They are the vocabulary of generic programming.

---

## 1. Two families

```
   PREDICATE traits  -> ask a yes/no question, expose ::value  (a bool)
       std::is_integral<T>::value          std::is_integral_v<T>     (C++17 _v)

   TRANSFORMATION traits -> produce a new type, expose ::type
       std::remove_const<T>::type          std::remove_const_t<T>    (C++14 _t)
```

```
   Trait<T>::value   is a compile-time bool/int   (predicates)
   Trait<T>::type    is a type                     (transformations)

   Shorthands:
     _v suffix  ->  ::value    e.g. is_pointer_v<T>
     _t suffix  ->  ::type     e.g. decay_t<T>
```

---

## 2. `integral_constant`: the atom of traits

Every predicate trait ultimately *is* an `integral_constant`.

```cpp
template <class T, T v>
struct integral_constant {
    static constexpr T value = v;
    using value_type = T;
    using type = integral_constant;
    constexpr operator T() const noexcept { return v; }   // implicit -> value
    constexpr T operator()() const noexcept { return v; } // callable -> value
};

using true_type  = integral_constant<bool, true>;
using false_type = integral_constant<bool, false>;
```

```
   std::is_integral<int>   inherits from   true_type   (== integral_constant<bool,true>)
        -> ::value is true
        -> is implicitly convertible to true
        -> usable as a "tag" type for tag dispatch (chapter 05)
```

So a trait "returns" a *type* (`true_type`/`false_type`) whose `::value` is the
answer. That's why traits double as dispatch tags.

---

## 3. Writing predicate traits (via specialization)

```cpp
// is_pointer
template <class T> struct is_pointer      : std::false_type {};
template <class T> struct is_pointer<T*>  : std::true_type  {};

// is_same
template <class A, class B> struct is_same       : std::false_type {};
template <class A>          struct is_same<A, A> : std::true_type  {};

// is_reference
template <class T> struct is_reference       : std::false_type {};
template <class T> struct is_reference<T&>   : std::true_type  {};
template <class T> struct is_reference<T&&>  : std::true_type  {};
```

```
   is_same<int,int>   -> matches is_same<A,A> (A=int)  -> true_type
   is_same<int,char>  -> only primary                   -> false_type

   Pattern-matching on the TYPE STRUCTURE is the trait technique.
```

---

## 4. Writing transformation traits

```cpp
template <class T> struct remove_const          { using type = T; };
template <class T> struct remove_const<const T> { using type = T; };  // peel const

template <class T> struct remove_reference      { using type = T; };
template <class T> struct remove_reference<T&>  { using type = T; };
template <class T> struct remove_reference<T&&> { using type = T; };

template <class T> struct add_pointer { using type = T*; };
```

```
   remove_const<const int>::type   -> int
   remove_reference<int&>::type     -> int
   add_pointer<int>::type           -> int*
```

`std::remove_reference_t<T>` is the piece that makes `std::move` work
(`static_cast<remove_reference_t<T>&&>(x)`).

---

## 5. `decay` — what pass-by-value does to a type

`std::decay_t<T>` mimics the transformation applied to a by-value function
parameter (see [04-argument-deduction.md](04-argument-deduction.md)): remove
reference, remove cv, arrays->pointers, functions->pointers.

```
   decay_t<const int&>   -> int
   decay_t<int[10]>       -> int*
   decay_t<void(int)>     -> void(*)(int)
   decay_t<const char[6]> -> const char*
```

Used everywhere you store "the value type" of something (e.g. `std::thread`,
`std::make_tuple`, `std::any`).

---

## 6. The trait catalog (what's in `<type_traits>`)

```
  PRIMARY CATEGORIES (mutually exclusive-ish):
    is_void, is_null_pointer, is_integral, is_floating_point,
    is_array, is_pointer, is_lvalue_reference, is_rvalue_reference,
    is_member_object_pointer, is_member_function_pointer,
    is_enum, is_union, is_class, is_function

  COMPOSITE CATEGORIES:
    is_reference, is_arithmetic (integral||floating),
    is_fundamental, is_object, is_scalar, is_compound, is_member_pointer

  TYPE PROPERTIES:
    is_const, is_volatile, is_trivial, is_trivially_copyable,
    is_standard_layout, is_empty, is_polymorphic, is_abstract,
    is_final, is_aggregate (C++17), is_signed, is_unsigned,
    has_unique_object_representations (C++17)

  SUPPORTED OPERATIONS:
    is_constructible, is_default_constructible, is_copy_constructible,
    is_move_constructible, is_assignable, is_copy_assignable,
    is_move_assignable, is_destructible, is_swappable (C++17),
    + trivially_/nothrow_ variants of each  (e.g. is_nothrow_move_constructible)

  RELATIONSHIPS:
    is_same, is_base_of, is_convertible, is_invocable (C++17),
    is_invocable_r, invoke_result_t (C++17), common_type, common_reference (C++20)

  TRANSFORMATIONS:
    remove_cv/const/volatile, add_cv/const/volatile,
    remove_reference, add_lvalue_reference, add_rvalue_reference,
    remove_pointer, add_pointer, remove_extent, remove_all_extents,
    make_signed, make_unsigned, decay, remove_cvref (C++20),
    conditional, enable_if, common_type, underlying_type,
    type_identity (C++20), void_t (C++17)

  MISC:
    conjunction, disjunction, negation (C++17) -- logical combinators
    is_constant_evaluated (C++20, function) -- see chapter 12
```

You don't memorize these — you learn the *naming pattern* (`is_*`, `remove_*`,
`add_*`, `*_v`, `*_t`, `nothrow_`, `trivially_`) and grep the reference.

---

## 7. `conditional` — compile-time ternary on types

```cpp
template <bool B, class T, class F> struct conditional      { using type = T; };
template <class T, class F>         struct conditional<false, T, F> { using type = F; };

template <bool B, class T, class F>
using conditional_t = typename conditional<B, T, F>::type;

// pick smallest int type that holds N bits (toy):
using X = std::conditional_t<(sizeof(void*) == 8), std::int64_t, std::int32_t>;
```

```
   conditional_t<true , A, B>  -> A
   conditional_t<false, A, B>  -> B
   (a compile-time  cond ? A : B  for TYPES)
```

---

## 8. Logical combinators (C++17)

`conjunction`/`disjunction`/`negation` combine traits **with short-circuiting**
(important because it avoids instantiating later traits when an earlier one
already decides the result).

```cpp
template <class T>
constexpr bool is_small_trivial_v =
    std::conjunction_v<std::is_trivially_copyable<T>,
                       std::bool_constant<(sizeof(T) <= 16)>>;
```

```
   conjunction<A, B, C>  ~  A::value && B::value && C::value  (short-circuits)
   disjunction<A, B, C>  ~  A::value || B::value || C::value  (short-circuits)
   negation<A>           ~  !A::value
```

Short-circuiting matters: `conjunction<always_false, uses_undefined<T>>` won't
instantiate the second trait if the first is false.

---

## 9. `invoke_result` & `is_invocable` (C++17)

Answer "what does calling `f(args...)` return?" and "can I call it at all?"

```cpp
#include <type_traits>

auto f = [](int x, double y) { return x + y; };

using R = std::invoke_result_t<decltype(f), int, double>;   // double
static_assert(std::is_invocable_v<decltype(f), int, double>);
static_assert(std::is_invocable_r_v<double, decltype(f), int, double>);
```

```
   invoke_result_t<F, Args...>   -> return type of F(Args...)
   is_invocable_v<F, Args...>    -> can F be called with Args?
   is_invocable_r_v<R,F,Args...> -> ...and result convertible to R?
```

These handle function pointers, member pointers, functors, and lambdas uniformly
(same rules as `std::invoke`).

---

## 10. Using traits: `static_assert` guardrails

Traits shine as **preconditions** that produce readable errors:

```cpp
template <class T>
class LockFreeQueue {
    static_assert(std::is_trivially_copyable_v<T>,
                  "LockFreeQueue requires trivially copyable T");
    static_assert(std::is_nothrow_move_constructible_v<T>,
                  "T's move constructor must be noexcept");
    // ...
};
```

```
   Instead of a 200-line template error deep inside the queue,
   the user sees:  "LockFreeQueue requires trivially copyable T"
```

---

## 11. `remove_cvref` (C++20) — the everyday cleanup

The extremely common "strip references and cv-qualifiers" operation finally got
a single trait:

```cpp
template <class T>
void store(T&& x) {
    using Clean = std::remove_cvref_t<T>;   // == remove_cv_t<remove_reference_t<T>>
    Clean copy = std::forward<T>(x);
    // ...
}
```

```
   T = const std::string&
   remove_cvref_t<T> = std::string
   (pre-C++20 you wrote remove_cv_t<remove_reference_t<T>> or decay_t)
```

---

## 12. Worked example: a trait-driven generic hash-ish printer

```cpp
#include <iostream>
#include <type_traits>
#include <string>

template <class T>
void show(const T& x) {
    if constexpr (std::is_integral_v<T>) {
        std::cout << "int(" << x << ")\n";
    } else if constexpr (std::is_floating_point_v<T>) {
        std::cout << "float(" << x << ")\n";
    } else if constexpr (std::is_same_v<std::remove_cvref_t<T>, std::string>) {
        std::cout << "string(\"" << x << "\")\n";
    } else if constexpr (std::is_pointer_v<T>) {
        std::cout << "pointer(" << static_cast<const void*>(x) << ")\n";
    } else {
        std::cout << "other\n";
    }
}

int main() {
    show(7);
    show(2.5);
    show(std::string("hi"));
    int v = 3; show(&v);
}
```

```
   Each if constexpr asks a TRAIT; only the true branch is compiled for T.
   is_integral -> is_floating_point -> is string? -> is_pointer -> else
```

Runnable: [`examples/ch08_traits.cpp`](examples/ch08_traits.cpp).

---

## 13. Summary

<!--diagram
title: Type traits
box[green] Key points
  text: Predicate traits: `::value` / `*_v` (compile-time bool)
  text: Transform traits: `::type` / `*_t` (a new type)
  text: Built from `integral_constant` + template specialization
  text: `decay_t` models pass-by-value; `remove_cvref_t` (C++20) strips ref+cv
  text: `conditional_t` = compile-time `?:` for types
  text: `conjunction`/`disjunction`/`negation` = short-circuit logic
  text: `invoke_result_t` / `is_invocable_v` = "call" introspection
  text: Use `static_assert(trait, "msg")` for friendly errors
-->
```
 +-------------------------------------------------------------------+
 | Predicate traits: ::value / *_v  (compile-time bool)              |
 | Transform traits: ::type  / *_t  (a new type)                     |
 |                                                                   |
 | Built from integral_constant + template specialization.           |
 | decay_t models pass-by-value; remove_cvref_t (C++20) strips ref+cv|
 | conditional_t = compile-time ?: for types.                        |
 | conjunction/disjunction/negation = short-circuit logic.           |
 | invoke_result_t / is_invocable_v = "call" introspection.          |
 | Use static_assert(trait, "msg") for friendly errors.              |
 +-------------------------------------------------------------------+
```

Next: [09-perfect-forwarding.md](09-perfect-forwarding.md).
