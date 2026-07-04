# 17 — C++ Templates Cheatsheet

One-page-ish quick reference. Assumes you've read the chapters; this is recall,
not teaching.

---

## Declaring

```cpp
template <class T> ...                    // type parameter
template <typename T> ...                 // same
template <int N> ...                      // non-type (NTTP)
template <auto V> ...                     // C++17: deduced-type NTTP
template <double D> ...                   // C++20: floating NTTP
template <FixedString S> ...              // C++20: class-type NTTP (structural)
template <template <class...> class C> ...// template-template parameter
template <class... Ts> ...                // parameter pack
template <class T = int> ...              // default argument
template <std::integral T> ...            // C++20: constrained parameter
```

## The five templatable things

```cpp
template <class T> T f(T);                          // function
template <class T> struct S { ... };                // class
template <class T> constexpr T pi = T(3.14159L);    // variable (C++14)
template <class T> using Vec = std::vector<T>;      // alias (C++11)
template <class T> concept C = ...;                 // concept (C++20)
```

## Deduction quick table

```
  PARAM      ARG            T            NOTES
  T          const int&     int          by value: decays
  T&         const int      const int    ref: preserves const
  const T&   anything       bare type    binds all; workhorse
  T&&        lvalue X       X&           forwarding ref (collapsing)
  T&&        rvalue X       X            forwarding ref
  T(&)[N]    int[10]        T=int,N=10   deduce array size
  auto       expr           like T       decays
  auto&&     expr           fwd ref      lvalue->&, rvalue->&&
  decltype(auto) expr       exact        no decay
```

## Specialization

```cpp
template <class T> struct S    { };        // primary
template <>        struct S<int>{ };        // full
template <class T> struct S<T*>{ };         // partial (classes/vars only)
template <class T> constexpr bool v = ...;  // variable template + specialize
```

## Variadic / packs

```cpp
template <class... Ts> void f(Ts... xs);   // declare
f(xs...);                                   // expand
sizeof...(Ts)                               // count
(xs + ...)          (... + xs)              // unary R / L fold
(xs + ... + 0)      (0 + ... + xs)          // binary R / L fold (empty-safe)
((cout << xs << ' '), ...);                 // comma fold (per-element effects)
xs...[0]   Ts...[0]                          // C++26 pack indexing
auto [head, ...tail] = t;                   // C++26 binding pack
template for (auto e : t) { ... }           // C++26 expansion statement
```

## Forwarding

```cpp
template <class... A> R f(A&&... a) { g(std::forward<A>(a)...); }  // perfect fwd
decltype(auto) wrap(F&& f, A&&... a) {                              // fwd result
    return std::forward<F>(f)(std::forward<A>(a)...);
}
// move (owned local): std::move(x)   |   forward (fwd-ref param): forward<T>(x)
```

## SFINAE (legacy) vs concepts (modern)

```cpp
// SFINAE
template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0> void f(T);
template <class T> auto g(T x) -> decltype(x.size());        // expression SFINAE
template <class T, class = void> struct has_x : std::false_type {};
template <class T> struct has_x<T, std::void_t<typename T::x>> : std::true_type {};

// Concepts (prefer)
template <std::integral T> void f(T);
void f(std::integral auto x);
template <class T> requires requires (T v){ v.size(); } void g(T);
template <class T> concept HasX = requires { typename T::x; };
```

## `requires` requirement kinds

```cpp
requires (T a) {
    a.foo();                              // simple: valid expression
    typename T::type;                     // type requirement
    { a.size() } -> std::convertible_to<std::size_t>;   // compound
    { a.f() } noexcept;                   // + noexcept
    requires std::copyable<T>;            // nested boolean
};
```

## constexpr family

```cpp
constexpr int f();        // compile-time IF possible, else runtime
consteval int g();        // MUST be compile-time (immediate)
constinit int x = f();    // compile-time INIT, still mutable
if constexpr (cond) {...} // discard the untaken branch
if consteval {...} else {...}          // C++23: branch on evaluation context
template <class...> constexpr bool always_false = false;   // for else->error
```

## Type traits (naming patterns)

```cpp
std::is_integral_v<T>          // predicate -> bool (_v)
std::remove_cvref_t<T>         // transform -> type (_t)  (C++20)
std::decay_t<T>                // pass-by-value transform
std::conditional_t<B, X, Y>    // compile-time ?: on types
std::conjunction_v<A,B,C>      // short-circuit &&
std::invoke_result_t<F, A...>  // return type of F(A...)
std::is_invocable_v<F, A...>   // callable?
std::type_identity_t<T>        // non-deduced context helper
static_assert(trait_v<T>, "message");   // guardrail
```

## Patterns

```cpp
// CRTP
template <class D> struct Base { void i(){ static_cast<D*>(this)->impl(); } };
struct X : Base<X> { void impl(); };

// Deducing this (C++23) — CRTP replacement
struct Base { template <class Self> void i(this Self&& s){ s.impl(); } };

// Policy
template <class T, class Policy = Default> class Host { /* Policy::act() */ };

// Type erasure: Wrapper{ unique_ptr<Concept> }, Model<T>: Concept
// overloaded visitor (variant):
template <class... F> struct overloaded : F... { using F::operator()...; };
template <class... F> overloaded(F...) -> overloaded<F...>;
```

## CTAD & guides

```cpp
std::vector v{1,2,3};                       // deduced vector<int>
template <class T> Box(T) -> Box<T>;         // deduction guide
```

## Disambiguators

```cpp
typename T::value_type x;      // dependent type
obj.template get<0>();         // dependent member template
this->member;  Base<T>::member;// dependent base member
```

## C++ version -> feature map

```
  C++11: templates, variadics, alias templates, decltype, trailing return,
         extern template, forwarding refs, std::forward/move
  C++14: variable templates, generic lambdas, constexpr(loops),
         make_index_sequence, decltype(auto), auto return
  C++17: CTAD, auto NTTP, fold expressions, if constexpr, void_t,
         class-template auto, inline variables, constexpr lambda
  C++20: CONCEPTS, requires, abbreviated fn templates (auto params),
         class-type NTTP, consteval/constinit, operator<=>, remove_cvref,
         ranges, is_constant_evaluated, constexpr new/vector/string
  C++23: deducing this, if consteval, auto(x), multidim operator[],
         mdspan, expected, static operator(), constexpr unique_ptr
  C++26: static reflection (^^ / [::]), pack indexing (pack...[i]),
         expansion statements (template for), structured-binding packs,
         variadic friends, = delete("reason"), constexpr bindings,
         '_' placeholder, user-generated static_assert messages, contracts
```

## The decision cheat

```
  value at compile time?          -> constexpr/consteval function
  branch on type in a template?   -> if constexpr
  constrain / overload on props?  -> concepts (subsumption)
  detect capability?              -> requires-expression
  compute a TYPE?                 -> traits / type lists (-> C++26 reflection)
  runtime heterogeneous storage?  -> type erasure
  zero-overhead reuse of methods? -> CRTP / deducing this
  configurable behavior?          -> policy parameters
  forward args untouched?         -> T&&... + std::forward
```

---

Back to the [README](README.md).
