# 16 — Pitfalls, Error-Message Survival & Best Practices

The knowledge that turns "I can write templates" into "I ship robust template
code." This chapter is a field guide to the traps and the habits that avoid them.

---

## 1. `typename` and `template` disambiguators

Covered in chapter 00; here's the checklist.

```cpp
template <class T>
void f() {
    typename T::value_type x;        // dependent TYPE -> need 'typename'
    T::static_value;                 // dependent VALUE -> no keyword
    obj.template get<0>();           // dependent member TEMPLATE -> 'template'
    typename T::template rebind<U>::other z;  // both, together
}
```

```
   Rule:
     dependent name that is a TYPE      -> prefix 'typename'
     dependent name that is a TEMPLATE  -> prefix 'template' (after . -> ::)
   (C++20 relaxed some cases where 'typename' is now optional, e.g. in return
    types and trailing types, but writing it is always safe.)
```

---

## 2. Two-phase lookup surprises (names not found)

Names in a base class template are **not** visible unqualified in a derived
template — because at phase 1 the compiler doesn't look into dependent bases.

```cpp
template <class T>
struct Base { void helper() {} protected: int data = 0; };

template <class T>
struct Derived : Base<T> {
    void f() {
        // helper();      // ERROR: not found (dependent base not searched)
        this->helper();   // OK: 'this->' makes it dependent -> phase 2 lookup
        Base<T>::helper();// OK: explicit qualification
        // data = 1;      // ERROR for the same reason
        this->data = 1;   // OK
    }
};
```

```
   Unqualified 'helper()' in a template with a DEPENDENT base:
       phase 1: base not examined -> "not declared" error.
   Fix: this->helper()  or  Base<T>::helper()  (make it dependent).
```

---

## 3. Deep, scary error messages — how to read them

```
   Strategy:
   1. Read the FIRST error, not the last. The rest are cascade.
   2. Look for "required from here" / "in instantiation of" lines — they trace
      the CALL that triggered it. Find YOUR line near the top of that trace.
   3. The real cause is usually "no match for operator X" or "no member Y".
   4. Compile with -ferror-limit=1 (clang) / -fmax-errors=1 (gcc) to cut noise.
   5. Use static_assert with a message to catch the problem early with a
      readable diagnostic (see §8).
   6. In C++20, add concepts -> the error moves to the call site in plain words.
```

```
   error: no match for 'operator<' (operands 'Widget', 'Widget')
   note: in instantiation of 'T std::max(...) [with T = Widget]'
   note: required from 'void my_sort<...>' 
   note: required from here          <-- YOUR call is just above/below this
```

---

## 4. Missing `#include` / incomplete types

Because instantiation happens at point of use, forgetting a header often shows
up as a template error rather than "unknown identifier."

```
   * Instantiating std::vector<T>::push_back needs T to be complete at that point.
   * "invalid use of incomplete type 'struct X'" inside a template usually means
     you forward-declared X but the instantiation needs its full definition.
```

---

## 5. The `0`/`nullptr`/braces deduction traps

```cpp
template <class T> void f(T);
// f({1,2,3});          // ERROR: can't deduce T from a braced-init-list
f(std::vector{1,2,3});  // OK (CTAD makes a vector<int> first)

template <class T> void g(T*);
// g(0);                // deduces T from int literal 0? -> 0 is int, not ptr: ERROR
g(nullptr);             // T = ? nullptr_t is not a pointer: also tricky
int* p = nullptr; g(p); // OK: T = int
```

```
   * Templates can't deduce from {braces} (auto can).
   * '0' is an int; prefer nullptr for pointers.
   * A single T for two params needs matching arg types (chapter 01).
```

---

## 6. Greedy forwarding constructors / assignment

Chapter 09 §8: a `template<class T> C(T&&)` can shadow copy/move. Constrain it.

```cpp
template <class T>
    requires (!std::same_as<std::remove_cvref_t<T>, C>)   // C++20
C(T&&);
```

Also beware forwarding-reference overloads competing with derived-to-base
conversions and `initializer_list` constructors.

---

## 7. ODR & inline traps

```
   * A full specialization of a function template defined in a header must be
     'inline' (it's an ordinary function).
   * static data members of class templates: define inline (C++17) or out-of-line
     once; don't define in every TU.
   * Different template arguments that are "the same" in two TUs must produce the
     SAME instantiation — don't make instantiation depend on macros that differ
     between TUs (ODR violation, often undiagnosed).
   * NTTP with internal-linkage pointers can silently create distinct types.
```

---

## 8. `static_assert` early and often

Put contracts at the top of templates for readable failures:

```cpp
template <class T>
class RingBuffer {
    static_assert(std::is_nothrow_move_constructible_v<T>,
                  "RingBuffer<T>: T's move ctor must be noexcept");
    static_assert(!std::is_reference_v<T>,
                  "RingBuffer<T>: T cannot be a reference");
    // ...
};
```

In C++20 prefer **concepts** on the signature so the check happens at the call
site, not inside the class body.

---

## 9. Compile-time & binary-size costs

```
   * Each distinct instantiation = more code. Watch template bloat:
       - factor type-independent code into non-template base/helpers
         ("thin template" idiom: e.g. vector<T*> all share a vector<void*> core).
       - prefer 'const T&' params to reduce instantiations vs by-value.
   * Deep recursive TMP explodes compile time; prefer constexpr / fold / C++26
     expansion statements.
   * extern template (C++11) suppresses instantiation in a TU:
         extern template class std::vector<MyType>;   // don't instantiate here
       and instantiate once elsewhere:
         template class std::vector<MyType>;
```

```
   Thin-template idiom:
     template<class T> class Vec : VecBase {   // VecBase is non-template,
        ... };                                 // holds the heavy shared logic
   -> N instantiations share ONE copy of the bulk code.
```

---

## 10. Prefer the modern tool

```
   Instead of...                        Prefer...
   ----------------------------------   ------------------------------------
   enable_if SFINAE                     concepts / requires (C++20)
   tag dispatch                         constrained overloads + subsumption
   class specialization for branching   if constexpr
   recursive value TMP                  constexpr functions
   recursive type-list processing       C++26 reflection (when available)
   CRTP static_cast                     deducing this (C++23)
   manual index_sequence + fold         C++26 template for / binding packs
   hand-written per-struct code         C++26 reflection
```

---

## 11. Style & API guidelines

```
   * Constrain your templates. Unconstrained templates = bad errors + accidental
     matches. Even a light concept helps.
   * Name your concepts; compose overload sets from named concepts (subsumption).
   * Keep templates in headers; document required operations if not using concepts.
   * Provide deduction guides for your class templates when CTAD needs help.
   * Make move ctors noexcept (containers rely on it for strong guarantees).
   * Prefer 'typename' spelled out for clarity even when optional.
   * Don't over-genericize: if only two types are ever used, overloads may be
     simpler and compile faster than a template.
   * Test instantiation with representative types via static_assert / small mains.
```

---

## 12. Quick "why won't this compile?" flowchart

```
   Compile error in template code
        |
   Is it "expected type, got value" / "need typename"?   -> add 'typename' (§1)
        |
   "X was not declared" but X is in a base?               -> this->X / Base<T>::X (§2)
        |
   "no matching function"?  -> check deduction (ch 04), SFINAE removed all? (ch 07)
        |                       -> add explicit args f<T>(...) or fix constraints
        |
   "no member / no operator" deep in a lib?  -> your type misses an operation.
        |                       -> read FIRST error + "required from here" (§3)
        |
   "call to deleted / ambiguous"?  -> overload/constraint clash (ch 05/13)
        |
   Still stuck?  -> add static_assert probes / TypeShow<T> (ch 04 §9) / concepts.
```

---

## 13. Summary

<!--diagram
title: Pitfalls and best practices
box[green] Key points
  text: `typename` (dependent type) / `template` (dependent member template)
  text: Dependent base names need `this->` or `Base<T>::`
  text: Read the **first** error + "required from here" trace
  text: Can't deduce from `{braces}`; watch `0` vs `nullptr`; single-`T` params
  text: Constrain greedy forwarding ctors
  text: `inline` full specializations; define static members once
  text: `static_assert` / concepts for early, readable failures
  text: Fight bloat: thin-template idiom, `const&`, extern template
  text: Reach for the **modern** tool (concepts, `if constexpr`, C++23/26)
-->
```
 +------------------------------------------------------------------+
 | typename (dependent type) / template (dependent member template).|
 | Dependent base names need this-> or Base<T>::.                   |
 | Read the FIRST error + "required from here" trace.               |
 | Can't deduce from {braces}; watch 0 vs nullptr; single-T params. |
 | Constrain greedy forwarding ctors.                               |
 | inline full specializations; define static members once.         |
 | static_assert / concepts for early, readable failures.           |
 | Fight bloat: thin-template idiom, const&, extern template.       |
 | Reach for the MODERN tool (concepts, if constexpr, C++23/26).    |
 +------------------------------------------------------------------+
```

Next: [17-cheatsheet.md](17-cheatsheet.md).
