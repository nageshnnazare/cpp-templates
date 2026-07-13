# 00 — The Mental Model: What a Template *Really* Is

Before any syntax, install the correct mental model. Almost every confusing
template error and every "why won't this compile" moment comes from a wrong
mental model. Fix the model, and the rest is detail.

---

## 1. A template is a *recipe*, not code

A template is **not** a function or a class. It is a **pattern from which the
compiler generates** functions and classes. Think of it as a **cookie cutter**:
the cutter is not a cookie; you press it into dough (a type/value) and *out comes*
a cookie (a concrete function/class).

```
   TEMPLATE (recipe)                 INSTANTIATION (baked result)
 +-------------------+                +----------------------+
 | template<class T> |   T = int      | int max(int a,       |
 | T max(T a, T b){  | ------------>  |         int b){      |
 |   return a<b?b:a; |   "press"      |   return a<b?b:a;    |
 | }                 |                | }                    |
 +-------------------+                +----------------------+
          |               T = double   +----------------------+
          +------------------------->  | double max(double a, |
                          "press"      |            double b){|
                                       |   ...                |
                                       +----------------------+
```

Key consequence: **a template that is never instantiated generates no code.**
It is checked for basic syntax, but type-dependent errors inside it are only
diagnosed when you actually use it with concrete arguments.

---

## 2. Instantiation happens **on demand**

The compiler only "bakes a cookie" when you use the template with specific
arguments. This is called **implicit instantiation**.

```cpp
template <class T>
T square(T x) { return x * x; }

int main() {
    square(3);      // instantiates square<int>
    square(2.5);    // instantiates square<double>
    // square<std::string> is NEVER instantiated -> the fact that
    // std::string * std::string is ill-formed is NEVER checked.
}
```

```
 SOURCE                        WHAT THE COMPILER PRODUCES
 square(3)      --->   generate  int    square<int>(int)
 square(2.5)    --->   generate  double square<double>(double)
 (no other use) --->   nothing else generated
```

You can force instantiation with **explicit instantiation**:

```cpp
template int square<int>(int);   // "bake square<int> right now"
```

---

## 3. Two-phase compilation (the #1 source of surprises)

A template is compiled in **two phases**:

```
 PHASE 1: "Definition time"  (when the template is DEFINED)
 ---------------------------------------------------------
   * Syntax is checked.
   * NON-dependent names are looked up and bound NOW.
   * A "non-dependent" name is one that does not depend on a
     template parameter.

 PHASE 2: "Instantiation time" (when the template is USED with T=...)
 ---------------------------------------------------------
   * DEPENDENT names are looked up.
   * A "dependent" name depends on the template parameter T.
   * Type checking that involves T happens here.
```

### Why you need `typename` and `template` disambiguators

Because in phase 1 the compiler doesn't know what `T` is, it cannot tell whether
`T::something` is a **type** or a **value**. By default it assumes **value**.
You must tell it when it's a type:

```cpp
template <class T>
void f() {
    T::value_type * p;   // AMBIGUOUS: is this "declare pointer p"
                         // or "multiply T::value_type by p"?
                         // Compiler assumes VALUE -> multiplication -> error
    typename T::value_type * p2;  // OK: "value_type is a TYPE, declare p2"
}
```

```
        T::value_type * p;
                       ^
     Compiler in phase 1 sees a DEPENDENT name T::value_type.
     Rule: "a dependent qualified name is assumed to name a VALUE
            unless preceded by 'typename'."

   Without typename:  parsed as   (T::value_type) * (p)   <-- multiply
   With    typename:  parsed as   pointer-to (T::value_type) named p
```

Similarly, when a dependent name is a **member template**, you need the
`template` keyword:

```cpp
template <class T>
void g(T t) {
    t.template get<int>();   // 'template' tells parser get is a template
}
```

We revisit both in [16-pitfalls-and-best-practices.md](16-pitfalls-and-best-practices.md).

---

## 4. Dependent vs non-dependent names — the crux

```
  template <class T>
  struct Widget {
      int              a;   // non-dependent type (int)
      T                b;   // dependent type (depends on T)
      std::vector<T>   c;   // dependent (T appears)
      std::vector<int> d;   // non-dependent (no T)

      void method() {
          helper();         // non-dependent CALL -> looked up in PHASE 1
          use(b);           // argument b is dependent -> ADL in PHASE 2
      }
  };
```

Practical rule of thumb:

> **If a name involves `T` (directly or through a member), its meaning is decided
> at instantiation (phase 2). Otherwise it's decided at definition (phase 1).**

---

## 5. Templates are "duck typed" — until concepts

Classic templates place **implicit** requirements on their arguments. The
"contract" is whatever operations the body happens to use.

```cpp
template <class T>
T sum(const std::vector<T>& v) {
    T total{};                 // requires: T default-constructible
    for (const auto& x : v)
        total += x;            // requires: T supports +=
    return total;              // requires: T copyable/movable
}
```

Nothing in the signature states these requirements. If you pass a `T` that
lacks `+=`, you get a **deep, ugly error** pointing *inside* the template.

```
   Caller: sum(vec_of_widgets)
                 |
                 v  (deep instantiation)
   error: no operator+= for 'Widget'      <-- error blames the LIBRARY,
     in function sum<Widget>...                not the CALLER
```

**Concepts (C++20)** fix this by moving the contract into the signature so the
error blames the *caller* at the *call site*. See
[13-concepts.md](13-concepts.md). This single improvement is why modern template
code is so much friendlier.

---

## 6. The five kinds of templates

C++ has five things you can templatize:

```
 1. FUNCTION template     template<class T> T max(T,T);
 2. CLASS    template     template<class T> struct vector { ... };
 3. VARIABLE template     template<class T> constexpr T pi = T(3.14159L);   // C++14
 4. ALIAS    template     template<class T> using Vec = std::vector<T>;     // C++11
 5. CONCEPT               template<class T> concept Integral = ...;         // C++20
```

We cover each. Variable and alias templates are small; concept is a whole
chapter.

---

## 7. Where templates must live: headers

Because instantiation happens wherever the template is *used*, the **full
definition** must be visible at the point of use. This is why templates almost
always live entirely in **header files** (`.hpp`), not split into `.cpp`.

```
   main.cpp            util.hpp                 util.cpp
   #include "util.hpp"  template<class T>        // definition here is
   foo<int>(...)        T foo(T);   <-- decl     // INVISIBLE to main.cpp
        |               (no body)                 -> LINKER ERROR
        |                    ^
        +-- needs the BODY to instantiate foo<int>, but body is in .cpp
```

Workarounds: put the body in the header, use explicit instantiation in the
`.cpp` (only works if you know all types in advance), or C++20 **modules**
(which change the model — see notes in later chapters).

---

## 8. Mental model summary card

<!--diagram
title: Template mental model
box[green] Key points
  text: TEMPLATE = recipe. Generates code **on demand** (instantiation)
  text: Phase 1 (define): syntax + non-dependent names
  text: Phase 2 (use): dependent names + type checking with `T`
  text: `typename` before dependent TYPE; `template` before dependent member TEMPLATE
  text: Requirements are implicit (duck typing) unless you use concepts
  text: Definitions live in headers (needed at point of use)
-->
```
 +------------------------------------------------------------------+
 | TEMPLATE = recipe. Generates code ON DEMAND (instantiation).     |
 |                                                                  |
 | Phase 1 (define): syntax + non-dependent names.                  |
 | Phase 2 (use):    dependent names + type checking with T.        |
 |                                                                  |
 | 'typename' before dependent TYPE.  'template' before dependent   |
 |  member TEMPLATE.                                                |
 |                                                                  |
 | Requirements are implicit (duck typing) unless you use concepts. |
 |                                                                  |
 | Definitions live in headers (needed at point of use).            |
 +------------------------------------------------------------------+
```

Next: [01-function-templates.md](01-function-templates.md).
