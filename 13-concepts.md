# 13 — Concepts & Constraints (C++20)

Concepts are the biggest template feature since templates themselves. They let
you **name requirements**, **constrain** templates, get **great error messages**,
and **overload on properties**. If SFINAE was the assembly language of
constraints, concepts are the high-level language.

---

## 1. The problem concepts solve (recap)

```
   Unconstrained template + bad argument
   -------------------------------------
   sort(list_of_widgets)   // widget has no operator<
        |
        v  error appears DEEP inside <algorithm>, 40 lines, blaming the library

   Constrained template + bad argument
   -----------------------------------
   sort(list_of_widgets)
        |
        v  error at the CALL SITE:
           "constraint std::sortable not satisfied: Widget lacks operator<"
```

Concepts move the contract into the signature so the compiler checks it *before*
instantiating the body, and reports failures in human terms.

---

## 2. Defining a concept

A concept is a named compile-time bool predicate over template parameters.

```cpp
#include <concepts>

template <class T>
concept Integral = std::is_integral_v<T>;      // simplest form: a bool expression

template <class T>
concept Addable = requires (T a, T b) {         // requires-expression (see §4)
    a + b;                                       // "a + b must be valid"
};
```

```
   concept Name = <compile-time bool>;
   concept Name = requires(params) { ...requirements... };

   Use it like a type-constraint:  template<Integral T> ...
   Or as a bool:                   static_assert(Integral<int>);
```

---

## 3. Four ways to apply a constraint

```cpp
// (1) as the "type" in the template head
template <std::integral T>
T half(T x) { return x / 2; }

// (2) abbreviated function template with constrained auto
std::integral auto half2(std::integral auto x) { return x / 2; }

// (3) requires-clause after the template head
template <class T> requires std::integral<T>
T half3(T x) { return x / 2; }

// (4) trailing requires-clause (after the signature)
template <class T>
T half4(T x) requires std::integral<T> { return x / 2; }
```

```
   template<std::integral T>          <-- shorthand, most common
   void f(std::integral auto x)       <-- abbreviated, great for one-offs
   template<class T> requires C<T>    <-- when the constraint is complex
   ... requires C<T> { }              <-- trailing; can mention params/this
```

All four are equivalent for the simple case. Forms (3)/(4) handle arbitrary
boolean constraint expressions (`requires A<T> && (B<T> || C<T>)`).

---

## 4. `requires` expressions — the workhorse

A `requires`-expression is a compile-time bool that is `true` if all its
**requirements** are valid. Four kinds of requirement:

```cpp
template <class T>
concept Container = requires (T c, const T cc) {
    // (a) SIMPLE requirement: expression must be valid
    c.begin();
    c.end();

    // (b) TYPE requirement: a type must exist
    typename T::value_type;
    typename T::iterator;

    // (c) COMPOUND requirement: valid + noexcept + return-type constraint
    { c.size() } -> std::convertible_to<std::size_t>;
    { c.empty() } noexcept -> std::same_as<bool>;

    // (d) NESTED requirement: another compile-time bool must hold
    requires std::default_initializable<typename T::value_type>;
};
```

```
   simple:    expr;                        -> "expr compiles"
   type:      typename T::foo;             -> "T::foo names a type"
   compound:  { expr } -> Concept;         -> "expr compiles AND its type
                                              satisfies Concept"
              { expr } noexcept -> C;      -> "...and expr is noexcept"
   nested:    requires bool_expr;          -> "bool_expr is true"
```

Note the arrow: `{ expr } -> Concept` means "the type of `expr` must satisfy
`Concept`" — the type is implicitly the first argument. `std::same_as<bool>` etc.

---

## 5. The standard concepts library (`<concepts>`)

You rarely start from scratch — the standard provides building blocks:

```
  CORE LANGUAGE:
    same_as<T,U>, derived_from<D,B>, convertible_to<From,To>,
    common_with, common_reference_with, assignable_from,
    swappable, swappable_with

  COMPARISON:
    equality_comparable, equality_comparable_with,
    totally_ordered, totally_ordered_with,
    three_way_comparable (C++20)

  OBJECT / LIFETIME:
    destructible, constructible_from, default_initializable,
    move_constructible, copy_constructible,
    movable, copyable, semiregular, regular

  ARITHMETIC:
    integral, signed_integral, unsigned_integral, floating_point

  CALLABLE:
    invocable, regular_invocable, predicate,
    relation, equivalence_relation, strict_weak_order

  (ranges/iterator concepts live in <iterator> and <ranges>:)
    input_iterator, forward_iterator, random_access_iterator, ...
    range, view, sized_range, input_range, ...
```

`regular` and `semiregular` are especially useful:

```
   semiregular  = default_initializable + copyable   ("behaves like a value")
   regular      = semiregular + equality_comparable   ("...and comparable")
```

---

## 6. Subsumption: overloading on concepts

The killer feature: when two constrained overloads both match, the compiler
picks the one whose constraints **subsume** (are strictly stronger than) the
other. No tag dispatch, no priority tricks.

```cpp
#include <iterator>

template <std::input_iterator It>
void advance_it(It& it, int n) {           // general: O(n)
    while (n-- > 0) ++it;
}

template <std::random_access_iterator It>  // stronger constraint
void advance_it(It& it, int n) {           // fast: O(1)
    it += n;
}
```

```
   advance_it(vector_it, 5)
     both overloads viable (random_access IS input)
     random_access_iterator SUBSUMES input_iterator (it's stronger)
     -> the O(1) overload wins.

   advance_it(list_it, 5)
     only input_iterator matches -> O(n) overload.
```

```
   Constraint hierarchy (subsumption):
     random_access_iterator  ==>  bidirectional  ==>  forward  ==>  input
       (more requirements)                            (fewer requirements)
   Stronger constraint wins when both are satisfied.
```

This is the clean replacement for tag dispatch (chapter 05) and the whole
iterator-category machinery.

---

## 7. Constraining class templates & members

```cpp
template <class T>
concept Hashable = requires (T x) {
    { std::hash<T>{}(x) } -> std::convertible_to<std::size_t>;
};

template <Hashable Key, class Value>
class HashMap { /* ... */ };

// Constrain individual members (only available when the constraint holds):
template <class T>
struct Wrapper {
    T value;
    void print() const requires std::equality_comparable<T> {
        // this member exists only if T is equality_comparable
    }
};
```

```
   HashMap<std::string, int>   -> OK (string is Hashable)
   HashMap<Widget, int>        -> error at declaration: "Widget not Hashable"

   Wrapper<int>.print()        -> exists
   Wrapper<NoEq>.print()       -> not a member (constraint failed) -> clear error
```

---

## 8. Combining constraints: `&&`, `||`, negation

```cpp
template <class T>
concept Number = std::integral<T> || std::floating_point<T>;

template <class T>
concept SignedNumber = Number<T> && std::is_signed_v<T>;

template <class T>
concept NonBool = !std::same_as<T, bool>;
```

```
   Concepts compose with && and ||. For SUBSUMPTION to work correctly, prefer
   composing NAMED concepts (atomic constraints) rather than raw type-traits,
   because the compiler reasons about subsumption via the concept structure.
```

> Subsumption subtlety: `requires (std::is_integral_v<T>)` and
> `std::integral<T>` are *not* seen as related for subsumption even though both
> are "true for integers". Build overload hierarchies from **concepts**, not
> from ad-hoc `is_xxx_v` expressions, so the stronger/weaker relationship is
> visible to the compiler.

---

## 9. `requires requires` (and why it looks weird)

You'll see `requires requires`. The first `requires` introduces a **constraint**;
the second starts a **requires-expression**. It means "constrain this with an
ad-hoc inline requirement":

```cpp
template <class T>
    requires requires (T x) { x.serialize(); }   // ad-hoc, unnamed
void save(const T& x);
```

```
   requires   requires(T x){ x.serialize(); }
   \______/   \_______________________________/
   requires-  requires-EXPRESSION (the bool)
   CLAUSE
```

Prefer naming it as a concept for readability:

```cpp
template <class T> concept Serializable = requires (T x) { x.serialize(); };
template <Serializable T> void save(const T& x);   // much clearer
```

---

## 10. Concepts fix the greedy-forwarding-constructor bug (ch 09)

```cpp
struct Person {
    std::string name;
    template <class T>
        requires (!std::same_as<std::remove_cvref_t<T>, Person>)  // <-- guard
    explicit Person(T&& n) : name(std::forward<T>(n)) {}
    Person(const Person&) = default;
};

Person a("bob");
Person b(a);      // now the template is DISABLED for Person -> copy ctor used
```

```
   Person b(a):  the constrained ctor rejects T=Person& (constraint false)
                 -> copy constructor is selected. Bug fixed cleanly.
```

---

## 11. Worked example: a clean generic `print`

Recall the SFINAE version in chapter 07 was ambiguous. Concepts make it precise:

```cpp
#include <iostream>
#include <concepts>
#include <ranges>
#include <string>
#include <vector>

template <class T>
concept Streamable = requires (std::ostream& os, const T& x) {
    { os << x } -> std::same_as<std::ostream&>;
};

// prefer streaming when possible:
template <Streamable T>
void print(const T& x) { std::cout << x << '\n'; }

// otherwise, if it's a range whose elements are streamable:
template <std::ranges::input_range R>
    requires (!Streamable<R>) && Streamable<std::ranges::range_value_t<R>>
void print(const R& r) {
    std::cout << "[ ";
    for (const auto& e : r) std::cout << e << ' ';
    std::cout << "]\n";
}

int main() {
    print(42);                          // 42  (Streamable)
    print(std::string("hi"));           // hi  (string IS streamable -> not range branch)
    print(std::vector<int>{1, 2, 3});   // [ 1 2 3 ]  (range branch)
}
```

```
   print(std::string): string is Streamable -> Streamable overload wins,
        the range overload is disabled by (!Streamable<R>). No ambiguity!
   print(vector<int>): not streamable, is a range of streamable ints -> range.
```

Runnable: [`examples/ch13_concepts_print.cpp`](examples/ch13_concepts_print.cpp).

---

## 12. Migration cheat sheet: SFINAE -> concepts

```
   OLD (SFINAE / traits)                     NEW (concepts)
   ---------------------------------------   -------------------------------
   template<class T,                         template<std::integral T>
     enable_if_t<is_integral_v<T>,int> =0>
   void f(T);                                void f(T);

   template<class T>                         void f(std::integral auto);
     enable_if_t<is_integral_v<T>> f(T);

   void_t detection idiom                    requires { expr; } / concept

   tag dispatch on iterator_category         constrained overloads +
                                             subsumption
```

---

## 13. Summary

<!--diagram
title: Concepts
box[green] Key points
  text: Concept = named compile-time predicate on template params
  text: Apply: `template<Concept T>` | `void f(Concept auto x)`
  text: `template<class T> requires Expr` | `... requires Expr`
  text: `requires`-expr requirements: simple / type / compound / nested
  text: `{ e } -> C`; `{ e } noexcept -> C`; `typename T::x`; `requires b;`
  text: Standard: integral, regular, invocable, ranges/iterator concepts
  text: Subsumption: stronger constraint wins → replaces tag dispatch
  text: Build overloads from **named** concepts (for correct subsumption)
  text: Errors point at the **call site** in plain language
-->
```
 +------------------------------------------------------------------+
 | Concept = named compile-time predicate on template params.       |
 |                                                                  |
 | Apply:  template<Concept T>  |  void f(Concept auto x)           |
 |         template<class T> requires Expr  |  ... requires Expr    |
 |                                                                  |
 | requires-expr requirements: simple / type / compound / nested.   |
 | { e } -> C ;  { e } noexcept -> C ;  typename T::x ;  requires b;|
 |                                                                  |
 | Standard: integral, regular, invocable, ranges/iterator concepts.|
 | Subsumption: stronger constraint wins -> replaces tag dispatch.  |
 | Build overloads from NAMED concepts (for correct subsumption).   |
 | Errors point at the CALL SITE in plain language.                 |
 +------------------------------------------------------------------+
```

Next: [14-cpp23-features.md](14-cpp23-features.md).
