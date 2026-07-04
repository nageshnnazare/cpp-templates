# 09 — Perfect Forwarding, Forwarding References & Reference Collapsing

"Perfect forwarding" means: write a wrapper that passes its arguments to another
function **without changing their value category** (lvalue stays lvalue, rvalue
stays rvalue) and without extra copies. This is the machinery behind
`std::make_unique`, `emplace_back`, `std::invoke`, and every good generic
factory.

Prereqs: value categories, [04-argument-deduction.md](04-argument-deduction.md).

---

## 1. The problem it solves

```cpp
// A naive wrapper LOSES information:
template <class T>
void wrapper(T arg) {           // by value -> always copies
    real_function(arg);         // 'arg' is an lvalue here -> can't move
}

// We want: if the caller passed an rvalue (temporary), forward it as an rvalue
// (so real_function can MOVE from it). If lvalue, keep it an lvalue.
```

```
   caller: wrapper(make_big())     // rvalue -> we WANT a move
   caller: BigThing b; wrapper(b)  // lvalue -> we WANT a copy (no steal)

   Goal: the wrapper is "transparent" — real_function sees exactly what the
         caller passed.
```

---

## 2. Forwarding references (`T&&` on a deduced `T`)

A parameter of the form `T&&` where **`T` is deduced** is a *forwarding
reference* (a.k.a. "universal reference", Scott Meyers' term). It binds to
**both** lvalues and rvalues, and encodes which one in `T`.

```cpp
template <class T>
void wrapper(T&& arg);          // forwarding reference
```

```
   IMPORTANT: it is a forwarding reference ONLY when:
     * the form is exactly  T&&  (cv-unqualified), AND
     * T is being DEDUCED at this call.

   NOT forwarding references (these are plain rvalue references):
     void f(std::string&&);          // concrete type, not deduced
     template<class T> void f(const T&&);  // has const
     template<class T> void f(std::vector<T>&&);  // T not at top level
     auto&& x = ...;                 // THIS one IS a forwarding ref (auto deduced)
```

---

## 3. Reference collapsing — the rule that makes it work

When references-to-references arise (only in templates/typedefs), they
"collapse":

```
   T& &    ->  T&
   T& &&   ->  T&
   T&& &   ->  T&
   T&& &&  ->  T&&

   Mnemonic: "& is contagious." If any & is involved, result is &.
             Only && && stays &&.
```

Combine with the forwarding-ref deduction rule:

```
   argument is LVALUE of type X  ->  T deduced as X&    ->  param: X& && -> X&
   argument is RVALUE of type X  ->  T deduced as X     ->  param: X&&
```

```
   BigThing b;
   wrapper(b)           // lvalue: T = BigThing&,  arg type = BigThing&
   wrapper(make_big())  // rvalue: T = BigThing,   arg type = BigThing&&
```

So `T` itself tells you what the caller passed:

```
   T is a reference type (X&)   <=>  caller passed an LVALUE
   T is a non-reference (X)      <=>  caller passed an RVALUE
```

---

## 4. `std::forward` — conditional cast based on `T`

Inside the wrapper, `arg` is a *named variable*, so it is an **lvalue**
regardless of how it was passed. To restore the original value category we cast
with `std::forward<T>`:

```cpp
template <class T>
void wrapper(T&& arg) {
    real_function(std::forward<T>(arg));   // restores lvalue/rvalue-ness
}
```

```
   std::forward<T>(arg) does:
     if T is X&   (lvalue was passed) -> returns an LVALUE reference (no move)
     if T is X    (rvalue was passed) -> returns an RVALUE reference (enables move)
```

Its implementation is basically a conditional cast:

```cpp
template <class T>
constexpr T&& forward(std::remove_reference_t<T>& x) noexcept {
    return static_cast<T&&>(x);   // T&& collapses to T& or T&& per the rule
}
```

```
   forward<X&>(x)  -> static_cast<X& &&>(x)  -> static_cast<X&>(x)   (lvalue)
   forward<X>(x)   -> static_cast<X&&>(x)                             (rvalue)
```

### `std::move` vs `std::forward`

```
   std::move(x)      -> UNCONDITIONAL cast to rvalue. "I'm done with x."
   std::forward<T>(x)-> CONDITIONAL cast, depends on T. "Pass along as-is."

   Use move   for a variable you OWN and want to steal from.
   Use forward for a FORWARDING-REFERENCE parameter you're passing onward.
```

```
   template<class T> void f(T&& x) {
       g(std::move(x));       // BUG if caller passed an lvalue: you stole it!
       g(std::forward<T>(x)); // CORRECT: only steals if caller gave an rvalue
   }
```

---

## 5. The full picture, one diagram

```
                       caller side                    wrapper internals
   -----------------------------------------------    ---------------------------
   BigThing b;                                        template<class T>
   wrapper(b);       // lvalue                         void wrapper(T&& arg) {
        |  T = BigThing&                                 real( forward<T>(arg) );
        |  arg type: BigThing&                         }
        v
   forward<BigThing&>(arg) -> BigThing&  (lvalue) -> real() copies

   wrapper(make());  // rvalue
        |  T = BigThing
        |  arg type: BigThing&&
        v
   forward<BigThing>(arg) -> BigThing&&  (rvalue) -> real() MOVES
```

---

## 6. Forwarding a whole pack

The canonical variadic factory:

```cpp
template <class T, class... Args>
std::unique_ptr<T> make(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
```

```
   Args&&... args           -> pack of forwarding references
   std::forward<Args>(args)... -> forward EACH element with its own Args_i
        expands to:  new T( forward<A0>(a0), forward<A1>(a1), ... )
   Each argument keeps its own lvalue/rvalue-ness independently.
```

This is exactly `std::make_unique` / `emplace_back` / `std::make_shared`.

---

## 7. Perfect-forwarding the return value too

Use `decltype(auto)` so a wrapper returns *exactly* what the inner call
returns (value, `T&`, or `T&&`) — see
[04-argument-deduction.md](04-argument-deduction.md) §6.

```cpp
template <class F, class... Args>
decltype(auto) timed_call(F&& f, Args&&... args) {
    auto start = now();
    // perfectly forward BOTH the callable and its args, and the RESULT:
    decltype(auto) result = std::forward<F>(f)(std::forward<Args>(args)...);
    log(now() - start);
    return result;                          // preserves reference-ness
}
```

```
   If f returns int&   -> timed_call returns int&
   If f returns int    -> timed_call returns int
   'auto' return would DECAY int& to int (losing the reference). Use decltype(auto).
```

> Subtlety: if the inner call returns a *reference to a temporary you created*,
> forwarding it out is a dangling reference. `decltype(auto)` preserves the type
> faithfully; you must still ensure lifetimes are valid.

---

## 8. Forwarding references vs overload hijacking (a real gotcha)

A forwarding-reference constructor is *greedy* — it can outcompete copy/move and
intended overloads:

```cpp
struct Person {
    std::string name;
    template <class T>
    explicit Person(T&& n) : name(std::forward<T>(n)) {}  // greedy!
    Person(const Person&) = default;    // may be BYPASSED
};

Person a("bob");
Person b(a);     // OOPS: T&& matches Person& BETTER than const Person&
                 // -> tries to init std::string from a Person -> error
```

```
   Person b(a);   // a is a non-const lvalue Person
     candidate 1: Person(T&&) with T=Person&   -> EXACT match
     candidate 2: Person(const Person&)         -> needs const conversion
   -> template wins -> name(std::forward<Person&>(a)) -> string from Person: FAIL
```

Fixes:
- Constrain the template so it doesn't accept `Person`:
  `requires (!std::same_as<std::remove_cvref_t<T>, Person>)` (C++20), or
  `enable_if` in C++17.
- Prefer plain overloads when the set of types is small.

This is why constrained forwarding constructors matter — chapter 13.

---

## 9. `auto&&` and range-for

`auto&&` is a forwarding reference for *variables*. Range-for uses it to bind to
any element type without copying, whether the range yields references or
prvalues (like `vector<bool>`'s proxy):

```cpp
for (auto&& x : range) {
    use(std::forward<decltype(x)>(x));   // forward the element if needed
}
```

```
   auto&& x = *it;
     *it is lvalue T&    -> x is T&
     *it is prvalue T    -> x is T&&   (binds the temporary, extends lifetime)
   -> works uniformly for real refs and proxy objects.
```

---

## 10. Worked example: transparent logging wrapper

```cpp
#include <iostream>
#include <string>
#include <utility>

struct Heavy {
    std::string data;
    Heavy(std::string s) : data(std::move(s)) { std::cout << "ctor\n"; }
    Heavy(const Heavy&)  : data()             { std::cout << "COPY\n"; }
    Heavy(Heavy&& o) noexcept : data(std::move(o.data)) { std::cout << "MOVE\n"; }
};

void sink(Heavy h) { std::cout << "sink got: " << h.data << '\n'; }

template <class T>
void forward_to_sink(T&& x) {
    sink(std::forward<T>(x));       // copy if lvalue, move if rvalue
}

int main() {
    Heavy h("lvalue");
    std::cout << "-- forwarding lvalue --\n";
    forward_to_sink(h);              // expect COPY
    std::cout << "-- forwarding rvalue --\n";
    forward_to_sink(Heavy("rvalue"));// expect MOVE (ctor then MOVE)
}
```

```
   forward_to_sink(h):       T=Heavy&  -> forward -> lvalue -> sink COPIES
   forward_to_sink(Heavy()): T=Heavy   -> forward -> rvalue -> sink MOVES
```

Runnable: [`examples/ch09_forwarding.cpp`](examples/ch09_forwarding.cpp).

---

## 11. Summary

<!--diagram
title: Perfect forwarding
box[green] Key points
  text: Forwarding reference = `T&&` where `T` is **deduced** (or `auto&&`)
  text: lvalue arg → `T = X&`; rvalue arg → `T = X`
  text: Reference collapsing: only `&& &&` stays `&&`; anything with `&` → `&`
  text: `std::forward<T>(x)`: conditional cast → restores value category
  text: `std::move(x)`: unconditional cast to rvalue
  text: Use forward for fwd-ref params; move for owned locals
  text: Pack: `forward<Args>(args)...` forwards each element
  text: Return: `decltype(auto)` to forward the **result** faithfully
  text: Beware greedy forwarding ctors → constrain them
-->
```
 +------------------------------------------------------------------+
 | Forwarding reference = T&&  where T is DEDUCED (or auto&&).       |
 |   lvalue arg -> T = X&  ;  rvalue arg -> T = X.                  |
 | Reference collapsing: only && && stays &&; anything with & -> &. |
 |                                                                  |
 | std::forward<T>(x): conditional cast -> restores value category. |
 | std::move(x):       unconditional cast to rvalue.                |
 | Use forward for fwd-ref params; move for owned locals.           |
 | Pack: forward<Args>(args)... forwards each element.              |
 | Return: decltype(auto) to forward the RESULT faithfully.         |
 | Beware greedy forwarding ctors -> constrain them.                |
 +------------------------------------------------------------------+
```

Next: [10-crtp-and-patterns.md](10-crtp-and-patterns.md).
