# 12 — `constexpr`, `consteval`, `constinit`, and `if constexpr`

Compile-time evaluation is the modern replacement for a lot of template
metaprogramming. This chapter covers the whole "constant-evaluation" family and,
critically, `if constexpr` — the feature that made generic code dramatically
simpler.

---

## 1. The family at a glance

```
   constexpr   "CAN run at compile time (and at runtime)."   [C++11, grew a lot]
   consteval   "MUST run at compile time." (immediate fn)     [C++20]
   constinit   "MUST be initialized at compile time."         [C++20]
                (but the variable is NOT const)
   if constexpr "compile-time branch; false branch discarded" [C++17]
   constexpr if same thing (informal name)
   std::is_constant_evaluated()  "am I running at compile time?" [C++20]
```

```
   constexpr int f();       // may be constant OR runtime
   consteval int g();       // ALWAYS constant; calling with runtime args = error
   constinit int x = f();   // init happens at compile time; x still mutable
   if constexpr(cond){...}  // picks one branch at compile time
```

---

## 2. `constexpr` functions

A `constexpr` function *can* be evaluated at compile time **if** its arguments
are constant expressions; otherwise it runs at runtime like a normal function.

```cpp
constexpr int square(int x) { return x * x; }

constexpr int a = square(5);   // compile-time: a is a constant (25)
int n = read_int();
int b = square(n);             // runtime: n isn't constant -> ordinary call
```

```
   square(5)   -> arguments constant -> evaluated at COMPILE time -> 25
   square(n)   -> n runtime          -> evaluated at RUNTIME
   Same function, both worlds. "constexpr" = "ELIGIBLE for compile-time."
```

### What constexpr functions can do (it grew over standards)

```
   C++11: single return, very restricted.
   C++14: loops, local vars, multiple statements, if/switch.  (usable!)
   C++17: constexpr lambdas, if constexpr.
   C++20: try/catch (no throw at ct), virtual constexpr, dynamic_cast,
          constexpr allocation (new/delete) IF freed before ct ends,
          std::vector / std::string usable in constexpr,
          constexpr std::is_constant_evaluated().
   C++23: constexpr std::unique_ptr, more <cmath>, 'if consteval',
          relaxed rules (goto/labels, static constexpr locals, etc.).
   C++26: even more of the library becomes constexpr.
```

```cpp
// C++20: build and use a vector entirely at compile time
constexpr int sum_first_n(int n) {
    std::vector<int> v;                 // constexpr allocation (C++20)
    for (int i = 1; i <= n; ++i) v.push_back(i);
    int s = 0;
    for (int x : v) s += x;
    return s;                           // vector freed before ct ends -> OK
}
static_assert(sum_first_n(100) == 5050);
```

### `constexpr` variables

A `constexpr` variable is a compile-time constant (implies `const`):

```cpp
constexpr double pi = 3.141592653589793;
constexpr int table[3] = { square(1), square(2), square(3) };  // {1,4,9}
```

---

## 3. `consteval` — immediate functions (C++20)

`consteval` forces compile-time evaluation. If you can't evaluate it at compile
time, it's an **error**. Use it when a runtime call would be a bug (e.g.
compile-time-only checks, generating constants).

```cpp
consteval int next_power_of_two(int n) {
    int p = 1; while (p < n) p <<= 1; return p;
}

constexpr int cap = next_power_of_two(100);   // OK -> 128 at compile time
// int n = read(); int x = next_power_of_two(n);  // ERROR: not a constant
```

```
   constexpr: compile-time IF POSSIBLE, else runtime.
   consteval: compile-time ALWAYS, else COMPILE ERROR.

   next_power_of_two(100)  -> 128 (compile time, fine)
   next_power_of_two(runtime_n) -> error: "call to consteval function is not
                                           a constant expression"
```

Great use: **compile-time-checked factory** (e.g. validate a format string, or a
color hex) so bad literals fail to compile.

```cpp
consteval std::uint32_t hex_color(const char* s) {
    // ... parse; if invalid, call a non-constexpr function to force an error
    return /* parsed value */ 0;
}
```

---

## 4. `constinit` (C++20) — solve static init order, keep mutability

`constinit` guarantees a variable is initialized during **constant
initialization** (at compile time), avoiding the "static initialization order
fiasco" — but unlike `constexpr`, the variable can still be **modified** at
runtime.

```cpp
constinit int counter = 0;     // guaranteed no dynamic init; counter is mutable
// counter = 5;                // OK at runtime (not const)

// constexpr int c2 = f();     // c2 is const, cannot change
```

```
   constexpr x  = init  -> compile-time init AND const (read-only)
   constinit x  = init  -> compile-time init, but MUTABLE at runtime
   (plain)   x  = init  -> may be dynamically initialized (order fiasco risk)
```

Use `constinit` for global/`thread_local` state that must be ready before any
runtime code but that you still write to.

---

## 5. `if constexpr` (C++17) — the game-changer

A compile-time branch: the compiler **keeps** the taken branch and **discards**
the other. The discarded branch is *not instantiated* for the current template
arguments, so it may contain code that would be ill-formed for that `T`.

```cpp
template <class T>
auto to_string_generic(const T& x) {
    if constexpr (std::is_arithmetic_v<T>) {
        return std::to_string(x);          // valid only for numbers
    } else if constexpr (requires { x.to_string(); }) {
        return x.to_string();              // valid only if member exists
    } else {
        return std::string("<?>");
    }
}
```

```
   to_string_generic(42):    is_arithmetic -> TRUE
        keep:    return std::to_string(x);
        DISCARD: x.to_string()  <-- never instantiated -> no error even though
                                    int has no .to_string()

   to_string_generic(obj):   is_arithmetic false, has to_string() -> that branch
```

### Why it beats SFINAE/tag-dispatch/specialization

```
   BEFORE (C++14):                     AFTER (C++17):
   - 2+ overloads with enable_if       - one function
   - or tag dispatch helpers           - straight-line if constexpr
   - or class template specializations - readable top to bottom
```

### The "discarded but still parsed" rule

The discarded branch is not *instantiated*, but it must still be **syntactically
valid** and its **non-dependent** parts are checked. To make a branch fully
dependent (so nothing is checked until instantiation), route through a template:

```cpp
template <class...> inline constexpr bool always_false = false;

template <class T>
void handle(const T&) {
    if constexpr (std::is_integral_v<T>)      { /* ... */ }
    else if constexpr (std::is_floating_point_v<T>) { /* ... */ }
    else {
        static_assert(always_false<T>, "unsupported type");
        //           ^^^^^^^^^^^^^^^^ dependent on T so it only fires when
        //           this branch is actually selected (not always).
    }
}
```

```
   static_assert(false, ...)          -> fires ALWAYS (non-dependent) -> WRONG
   static_assert(always_false<T>, ...) -> depends on T -> fires only if reached
```

(Note: C++23 relaxed some `static_assert(false)` cases in discarded branches,
but `always_false<T>` remains the portable idiom.)

---

## 6. `if consteval` (C++23) — branch on *how* you're being evaluated

Choose different code when running at compile time vs runtime. Cleaner and safer
than `std::is_constant_evaluated()`.

```cpp
constexpr double my_sqrt(double x) {
    if consteval {
        return newton_sqrt(x);      // compile-time-friendly implementation
    } else {
        return std::sqrt(x);        // fast runtime intrinsic
    }
}
```

```
   if consteval { A } else { B }
     during constant evaluation -> run A
     at runtime                  -> run B
```

Compare with the C++20 function form (has a famous footgun — you must use it in
an `if`, never `if constexpr`):

```cpp
constexpr double my_sqrt2(double x) {
    if (std::is_constant_evaluated())   // NOTE: plain 'if', not 'if constexpr'!
        return newton_sqrt(x);
    else
        return std::sqrt(x);
}
```

```
   if constexpr (std::is_constant_evaluated())  // BUG: always true in that
                                                //      context -> misleads.
   Always use plain 'if' with is_constant_evaluated(), or prefer 'if consteval'.
```

---

## 7. `constexpr` + templates: compile-time data tables

A powerful combo: generate lookup tables at compile time.

```cpp
#include <array>

constexpr std::array<int, 10> make_squares() {
    std::array<int, 10> a{};
    for (int i = 0; i < 10; ++i) a[i] = i * i;
    return a;
}
constexpr auto squares = make_squares();     // baked into the binary
static_assert(squares[4] == 16);
```

```
   make_squares() runs at COMPILE time -> 'squares' is a constant array
   -> zero runtime cost to build; it's just data in the binary.
```

Runnable: [`examples/ch12_constexpr_table.cpp`](examples/ch12_constexpr_table.cpp),
[`examples/ch12_if_constexpr.cpp`](examples/ch12_if_constexpr.cpp).

---

## 8. Decision guide

```
  Want...                                       Use
  -------------------------------------------   ------------------------
  a value usable at compile time (and runtime)  constexpr function/var
  to FORBID runtime evaluation                  consteval
  compile-time init but a mutable global        constinit
  branch on a type/trait inside a template      if constexpr
  branch on compile-time-vs-runtime execution   if consteval (C++23)
  compile-time lookup tables                    constexpr function -> array
```

---

## 9. Summary

<!--diagram
title: constexpr family
box[green] Key points
  text: `constexpr`: eligible for compile-time (else runtime)
  text: `consteval`: **must** be compile-time (immediate function)
  text: `constinit`: compile-time **init**, still mutable
  text: `if constexpr`: keep one branch, **discard** (don't instantiate) other
  text: → replaces most SFINAE/tag-dispatch/specialization
  text: → use `always_false<T>` for the "else → error" branch
  text: `if consteval` (C++23) / `is_constant_evaluated()` (C++20): pick code by evaluation context
  text: `constexpr` functions grew hugely: C++20 vector/string, C++23 more
-->
```
 +------------------------------------------------------------------+
 | constexpr : eligible for compile-time (else runtime).            |
 | consteval : MUST be compile-time (immediate function).           |
 | constinit : compile-time INIT, still mutable.                    |
 | if constexpr: keep one branch, DISCARD (don't instantiate) other.|
 |    -> replaces most SFINAE/tag-dispatch/specialization.          |
 |    -> use always_false<T> for the "else -> error" branch.        |
 | if consteval (C++23) / is_constant_evaluated() (C++20):          |
 |    pick code by evaluation context (use plain 'if' with the fn). |
 | constexpr functions grew hugely: C++20 vector/string, C++23 more.|
 +------------------------------------------------------------------+
```

Next: [13-concepts.md](13-concepts.md).
