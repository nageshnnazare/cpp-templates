# 04 — Template Argument Deduction (TAD)

Deduction is the "magic" that lets you write `max(a, b)` instead of
`max<int>(a, b)`. Understanding the rules precisely is what separates people who
*use* templates from people who *master* them. This chapter is the compiler's
rulebook, translated to English + diagrams.

---

## 1. The core idea

For a function template, the compiler matches each **parameter type** `P`
against the corresponding **argument type** `A`, and solves for the template
parameters.

```
   template <class T> void f(P);      call:  f(expr);   // expr has type A
                          ^                        ^
                       parameter                argument
   Solve:  P  ==?==  A     -->     deduce T
```

Example:

```
   template<class T> void f(T x);       f(42);
                        P = T           A = int
   Match T with int  ->  T = int
```

---

## 2. Deduction strips top-level things — three canonical forms

The parameter's *form* controls what happens. Memorize these three:

```
  FORM A:  by value        template<class T> void f(T  x);
  FORM B:  by ref/const-ref template<class T> void f(T& x); / f(const T& x);
  FORM C:  by forwarding ref template<class T> void f(T&& x);   // special!
```

### FORM A — `f(T x)` (by value)

Deduction **decays** the argument: strips `const`/`volatile`, strips references,
arrays->pointers, functions->pointers.

```cpp
template <class T> void f(T x);

int         i = 0;
const int   ci = 0;
int&        ri = i;
const int&  cri = ci;
int         arr[5];

f(i);    // T = int
f(ci);   // T = int      (const stripped — you get a COPY, so who cares)
f(ri);   // T = int      (reference stripped)
f(cri);  // T = int      (both stripped)
f(arr);  // T = int*     (array decays to pointer)
f("hi"); // T = const char*  (array of const char decays)
```

```
   A (argument type)     ---decay--->   T
   const int&                            int
   int[5]                                int*
   const char[3]                         const char*
   void()                                void(*)()
```

Rule: **by-value parameters ignore top-level const and references.** Because you
copy anyway.

### FORM B — `f(T& x)` and `f(const T& x)`

References **preserve** const (they don't decay arrays either).

```cpp
template <class T> void g(T& x);
template <class T> void h(const T& x);

int i = 0; const int ci = 0; int arr[5];

g(i);    // T = int         -> param int&
g(ci);   // T = const int   -> param const int&   (const PRESERVED)
g(arr);  // T = int[5]      -> param int(&)[5]     (array NOT decayed!)
// g(42);  // ERROR: cannot bind non-const T& to rvalue 42

h(i);    // T = int         -> const int&
h(ci);   // T = int         -> const int&  (const is in the param, not in T)
h(42);   // T = int         -> const int&  (const ref binds to rvalue: OK)
```

```
   g(T&):   const in A  ->  const ends up IN T
   h(const T&): the const is already in the param, so T is the "bare" type
```

This is why `const T&` is the workhorse parameter for read-only generic code:
it binds to everything (lvalues, rvalues, const, non-const) and `T` comes out
as the clean underlying type.

### FORM C — `f(T&& x)` (forwarding / "universal" reference)

**Only when `T` is a template parameter being deduced**, `T&&` is a *forwarding
reference* with special rules:

```
   If argument is an LVALUE of type X:   T = X&    (reference!)
   If argument is an RVALUE of type X:   T = X
   Then reference collapsing gives the final parameter type.
```

```cpp
template <class T> void fwd(T&& x);

int i = 0; const int ci = 0;

fwd(i);    // lvalue int       -> T = int&        -> param int&
fwd(ci);   // lvalue const int -> T = const int&  -> param const int&
fwd(42);   // rvalue int       -> T = int         -> param int&&
```

```
   Reference collapsing table (how X& / X&& combine):
     T = X&   , T&&  ->  X&      (& wins)
     T = X&&  , T&&  ->  X&&
     T = X    , T&&  ->  X&&
   Rule: "& is contagious." Any & involved -> lvalue reference.
```

This is the machinery behind **perfect forwarding** — full treatment in
[09-perfect-forwarding.md](09-perfect-forwarding.md). The key deduction fact:
**for `T&&`, an lvalue makes `T` an lvalue reference type.**

---

## 3. Deduction with nested/compound parameters

Deduction descends into structure. The parameter pattern is matched piecewise.

```cpp
template <class T> void f(std::vector<T> v);
f(std::vector<int>{});      // T = int

template <class T> void g(T* p);
int i; g(&i);               // T = int

template <class K, class V> void h(std::map<K, V> m);
h(std::map<std::string, int>{});   // K = std::string, V = int

template <class T, std::size_t N> void arr(T (&a)[N]);   // deduce array size!
int data[10]; arr(data);    // T = int, N = 10
```

```
   Pattern:  std::map< K , V >
   Arg:      std::map<string,int>
   Align:    K<->string   V<->int    -> deduced.

   Pattern:  T (&a)[N]    (reference to array of N T)
   Arg:      int[10]
   Align:    T<->int, N<->10
```

The array-size trick (`T(&)[N]`) is how you write a `countof` that can't be
fooled by pointers:

```cpp
template <class T, std::size_t N>
constexpr std::size_t countof(const T (&)[N]) { return N; }
```

---

## 4. Non-deduced contexts

Some positions **cannot** be deduced; the type must come from elsewhere
(explicit argument or default).

```
  NON-DEDUCED CONTEXTS (common ones):
    * The left of ::   ->  typename Foo<T>::type    (T not deducible from ::type)
    * A default argument
    * A non-type parameter used in an expression:  std::array<T, N+1>
    * The type of a NTTP referenced in an expression
```

```cpp
template <class T> struct Id { using type = T; };

template <class T> void f(typename Id<T>::type x);   // T is NON-deduced here
// f(42);            // ERROR: can't deduce T
f<int>(42);          // OK: T given explicitly
```

```
   typename Id<T>::type
                 ^^^^
   You can't run "::type" backwards to find T (many T could yield the same
   ::type). So this position is non-deduced.
```

This is actually *useful*: it lets you make a parameter "identity" so it does not
participate in deduction, forcing the caller to specify or letting another
parameter drive deduction. `std::type_identity_t<T>` (C++20) does exactly this.

```cpp
template <class T>
void clamp_to(T& value, std::type_identity_t<T> lo, std::type_identity_t<T> hi);
// T is deduced ONLY from 'value'; lo/hi convert to T without fighting deduction.
```

---

## 5. `auto` uses template deduction rules

`auto` variable deduction follows the **same** rules as FORM A/B/C (with one
exception for braced-init).

```cpp
auto        x = ci;    // like FORM A: x is int (const stripped)
auto&       y = ci;    // like FORM B: y is const int&
auto&&      z = 42;    // like FORM C: forwarding ref -> int&&
const auto& w = 42;    // const int&
```

```
   auto   x = expr;   ~  template<class T> f(T)   -> decays
   auto&  x = expr;   ~  template<class T> f(T&)  -> preserves
   auto&& x = expr;   ~  template<class T> f(T&&) -> forwarding
```

**Exception:** braced initializer.

```cpp
auto a = {1, 2, 3};   // a is std::initializer_list<int>   (special auto rule)
auto b{1};            // b is int   (C++17 fixed this single-element case)
// template<class T> void f(T);  f({1,2,3});  // ERROR: can't deduce from braces
```

Template functions **cannot** deduce `T` from a braced-init-list, but `auto`
can. This asymmetry trips people up.

---

## 6. `decltype` vs `auto` (deduction that *doesn't* decay)

`decltype(expr)` reports the *declared* type exactly, including references and
const — it does **not** decay. `decltype(auto)` uses those rules for deduction.

```cpp
int i = 0; const int& cr = i;

auto           a = cr;      // int         (decays)
decltype(cr)   b = cr;      // const int&  (exact)
decltype(auto) c = cr;      // const int&  (exact, like decltype)
decltype(auto) d = i;       // int
decltype(auto) e = (i);     // int&   <-- '(i)' is an lvalue expression!
```

```
   decltype(name)   -> the declared type of the entity
   decltype( (expr) ) -> adds & for lvalue expressions:
        lvalue   -> T&
        xvalue   -> T&&
        prvalue  -> T
```

`decltype(auto)` return type is essential for perfect *return* forwarding — you
want to return exactly what the wrapped call returns (value or reference).

```cpp
template <class F, class... Args>
decltype(auto) invoke_log(F&& f, Args&&... args) {
    return std::forward<F>(f)(std::forward<Args>(args)...);  // preserves ref-ness
}
```

---

## 7. Overload resolution vs deduction (they're different steps!)

```
   STEP 1: DEDUCTION      (per candidate template)  -> produces a concrete
                            signature, or FAILS (SFINAE — chapter 07)
   STEP 2: OVERLOAD RES.  (among all viable candidates) -> pick best
```

A deduction **failure** is not an error if other candidates exist — it just
removes that candidate. This is the foundation of SFINAE
([07-sfinae.md](07-sfinae.md)).

---

## 8. Class Template Argument Deduction recap (C++17)

CTAD (chapter 02 §7) deduces *class* template args from the constructor call,
using the constructors (and deduction guides) as an implicit set of function
templates, then running normal function-template deduction + overload
resolution on them.

```
   std::pair p{1, 2.5};
        |
   synthesize function templates from pair's ctors + guides
        |
   deduce as if calling  make_pair(1, 2.5)   -> pair<int, double>
```

---

## 9. Debugging deduction: make the compiler tell you `T`

A classic trick — declare an incomplete template and instantiate it with the
type you want revealed; the error message prints the deduced type:

```cpp
template <class T> struct TypeShow;   // no definition on purpose

template <class T> void probe(T&& x) {
    TypeShow<T> _;                    // ERROR message contains T
    TypeShow<decltype(x)> __;         // and the exact param type
}
// probe(some_expr);  -> read the compiler error to see T and decltype(x)
```

```
   error: implicit instantiation of undefined template 'TypeShow<int&>'
                                                                 ^^^^^^
                                                        <- T was deduced int&
```

Runnable: [`examples/ch04_deduction_probe.cpp`](examples/ch04_deduction_probe.cpp).

---

## 10. Cheat table

```
  PARAMETER      ARGUMENT        DEDUCED T        FINAL PARAM TYPE
  -----------    ------------    -------------    ----------------
  T (by value)   int             int              int
  T (by value)   const int&      int              int          (decays)
  T (by value)   int[5]          int*             int*         (decays)
  T&             int             int              int&
  T&             const int       const int        const int&
  const T&       const int       int              const int&
  T&&            int lvalue      int&             int&         (fwd ref)
  T&&            int rvalue      int              int&&        (fwd ref)
  vector<T>      vector<int>     int              vector<int>
  T(&)[N]        int[10]         T=int, N=10      int(&)[10]
```

## 11. Summary

<!--diagram
title: Template argument deduction
box[green] Key points
  text: Deduction matches PARAMETER pattern P against ARGUMENT type A
  text: by value (`T`) → **decays** (drops const/ref/array)
  text: by ref (`T&`) → preserves const, no decay
  text: fwd ref (`T&&`) → lvalue ⇒ `T` is a reference (collapsing)
  text: Some positions are **non-deduced** (left of `::`, expressions)
  text: Templates can't deduce from `{braces}`; `auto` can (`init_list`)
  text: `decltype`/`decltype(auto)` preserve exact type (no decay)
  text: Deduction failure ≠ error (SFINAE)
-->
```
 +------------------------------------------------------------------+
 | Deduction matches PARAMETER pattern P against ARGUMENT type A.   |
 |                                                                  |
 | by value  (T)     -> DECAYS (drops const/ref/array).             |
 | by ref    (T&)    -> preserves const, no decay.                  |
 | fwd ref   (T&&)   -> lvalue => T is a reference (collapsing).    |
 |                                                                  |
 | Some positions are NON-deduced (left of ::, expressions).        |
 | Templates can't deduce from {braces}; auto can (init_list).      |
 | decltype/decltype(auto) preserve exact type (no decay).          |
 | Deduction failure != error (SFINAE).                             |
 +------------------------------------------------------------------+
```

Next: [05-specialization.md](05-specialization.md).
