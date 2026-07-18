# 15 — C++26 Template Features

C++26 is the biggest leap for compile-time programming since C++11. The headline
is **static reflection**, but several smaller features (pack indexing, expansion
statements, structured-binding packs, variadic friends, `= delete("reason")`)
transform everyday template code.

> **Reality check (mid-2026):** C++26 is feature-complete in the working draft
> but not yet a published standard. Compiler support is *partial and evolving*.
> Each section notes availability. To experiment you generally need very recent
> GCC/Clang trunk and `-std=c++2c`; reflection often needs a dedicated
> branch/flag. Treat syntax here as "the voted-in design"; minor spellings may
> shift before publication.

---

## 1. Pack indexing (P2662) — subscript a pack directly

Finally, packs can be indexed with `[]`, both for **value** packs and **type**
packs. No more `std::tuple`/`index_sequence` gymnastics (chapter 06 §7).

```cpp
template <class... Ts>
auto first(Ts... args) {
    return args...[0];            // the 0th value in the pack
}

template <class... Ts>
using first_type = Ts...[0];      // the 0th type in the pack

template <std::size_t I, class... Ts>
auto nth(Ts... args) {
    return args...[I];            // I-th element (I must be a constant expr)
}
```

```
   pack...[index]      -> value pack indexing
   Pack...[index]      -> type  pack indexing

   first(10, 20, 30)   -> args...[0]  -> 10
   first_type<int,char,double> -> int
```

```
   BEFORE (C++17):  std::get<0>(std::forward_as_tuple(args...))   (heavy)
   C++26:           args...[0]                                    (native)
```

Availability: GCC 15, Clang 19+ (`-std=c++2c`).

---

## 2. Structured bindings can introduce a pack (P1061)

Structured bindings can now bind to a **pack** of the remaining members — huge
for generic code over tuples/aggregates.

```cpp
auto sum_all(auto tuple_like) {
    auto [...elems] = tuple_like;      // 'elems' is a PACK of the members
    return (elems + ... + 0);          // fold over them
}
sum_all(std::make_tuple(1, 2, 3));     // 6
```

```
   auto [...xs] = t;         // xs is a pack of all elements
   auto [head, ...tail] = t; // first element + pack of the rest

   Then use xs... anywhere a pack expansion is allowed.
```

This makes writing generic algorithms over structs/tuples feel natural.

---

## 3. Expansion statements: `template for` (P1306)

A compile-time loop that **iterates over a pack, tuple, or range of constant
size**, generating a copy of the body for each element — with the element's
*actual type* in each iteration. This replaces most `index_sequence` + fold
tricks with readable loops.

```cpp
template <class... Ts>
void print_all(const Ts&... args) {
    template for (const auto& x : {args...}) {   // one iteration per element
        std::cout << x << ' ';                    // x has the right type each time
    }
}

// iterate a tuple's elements, each with its own type:
auto t = std::make_tuple(1, 2.5, "hi");
template for (auto&& e : t) {
    std::cout << e << '\n';        // int, then double, then const char*
}
```

```
   template for (auto e : structured-thing) { BODY }
        |
   compiler UNROLLS: BODY with e = elem0 (type T0)
                     BODY with e = elem1 (type T1)
                     ...
   Not a runtime loop — it's a compile-time expansion, each body specialized.
```

```
   BEFORE:  tuple_for_each(t, [](auto&& e){...});  // helper + index_sequence
   C++26:   template for (auto&& e : t) { ... }     // just a loop
```

Availability: experimental; landing in trunk compilers through 2025-2026.

---

## 4. Static reflection (P2996 + friends) — the big one

Reflection lets you **inspect and generate** code with ordinary `constexpr`
programming. Two new operators:

```
   ^^X      "reflexpr": produce a std::meta::info describing entity X
            (a type, namespace, member, expression, ...)
   [: r :]  "splice": turn a reflection value r back into code
            (a type, an expression, a member access, ...)
```

```cpp
#include <meta>          // std::meta (header name may differ in final std)

// how many non-static data members does T have?
template <class T>
consteval std::size_t field_count() {
    return std::meta::nonstatic_data_members_of(^^T).size();
}
```

```
   ^^T   ---reflect--->   std::meta::info  (a first-class constexpr VALUE)
                                |
              manipulate with normal constexpr code (loops, vectors, if...)
                                |
   [: info :]  ---splice--->  back into a TYPE / EXPRESSION / member
```

### Example: generic `to_string` for any aggregate (no macros!)

```cpp
#include <meta>
#include <string>

template <class T>
std::string to_string_reflect(const T& obj) {
    std::string out = "{ ";
    // iterate the data members at compile time, splice member access at runtime:
    template for (constexpr auto member : std::meta::nonstatic_data_members_of(^^T)) {
        out += std::meta::identifier_of(member);     // the field NAME (a string!)
        out += "=";
        out += std::to_string(obj.[:member:]);       // splice: obj.<that member>
        out += " ";
    }
    return out + "}";
}

struct Point { int x; int y; };
// to_string_reflect(Point{1,2})  ->  "{ x=1 y=2 }"
```

```
   std::meta::nonstatic_data_members_of(^^Point) = [ info(x), info(y) ]
        template for over them:
            identifier_of(info(x)) = "x"       obj.[:info(x):] = obj.x
            identifier_of(info(y)) = "y"       obj.[:info(y):] = obj.y
   -> "{ x=1 y=2 }"     with ZERO hand-written per-field code.
```

This single feature obsoletes huge categories of code: serialization,
ORM/struct mapping, enum<->string, hashing, `operator==`, DI containers,
scripting bindings — all previously done with macros or fragile TMP.

### More reflection queries (illustrative names)

```
   std::meta::members_of(^^T)                  all members
   std::meta::nonstatic_data_members_of(^^T)   data fields
   std::meta::enumerators_of(^^E)              enum values  -> enum<->string!
   std::meta::identifier_of(r)                 the name (string_view)
   std::meta::type_of(r)                       the member's type (as info)
   std::meta::is_public(r), is_static(r), ...  properties
   std::meta::substitute(^^Tmpl, {args...})    instantiate a template
   [: type_info :]                             splice a type into code
```

### Enum-to-string in ~5 lines (the classic pain point)

```cpp
template <class E>
constexpr std::string_view enum_name(E value) {
    template for (constexpr auto e : std::meta::enumerators_of(^^E)) {
        if (value == [:e:])                       // splice the enumerator value
            return std::meta::identifier_of(e);   // its name
    }
    return "<unknown>";
}
```

```
   enum class Color { Red, Green, Blue };
   enum_name(Color::Green)  ->  "Green"
   No macro, no maintenance, works for ANY enum. This was IMPOSSIBLE in
   standard C++ before reflection.
```

Availability: **experimental**. Clang has a P2996 implementation branch; EDG
supports it; the header/namespace spelling (`<meta>`, `std::meta`) and exact
function names may change before C++26 ships. This is the feature to watch.

---

## 5. `= delete("reason")` (P2573)

Deleted functions can carry a message shown in the diagnostic — helpful for
template APIs that want to *guide* users away from certain instantiations.

```cpp
template <class T>
void serialize(const T&) { /* ... */ }

template <>
void serialize<RawPointer>(const RawPointer&) =
    delete("serialize raw pointers via serialize_ptr() to record ownership");
```

```
   serialize(raw_ptr);
   error: call to deleted function 'serialize'
   note: 'serialize raw pointers via serialize_ptr() to record ownership'
         ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
         your custom guidance appears in the error.
```

---

## 6. Variadic `friend` declarations (P2893)

You can now declare a **pack** of friends — useful for policy/mixin designs and
type erasure where the set of friends is template-parameterized.

```cpp
template <class... Friends>
class Secret {
    friend Friends...;          // all types in the pack are friends
    int value = 42;
};
```

```
   Secret<A, B, C>   ->  A, B, and C are all friends.
   Before C++26 you couldn't expand a pack in a friend declaration.
```

---

## 7. `constexpr` structured bindings & the `_` placeholder

```cpp
// (a) constexpr structured bindings (P2686): usable in constant expressions
constexpr auto p = std::pair{1, 2};
constexpr auto [a, b] = p;          // a, b are now usable in constexpr contexts
static_assert(a + b == 3);

// (b) '_' placeholder (P2169): a name you can reuse / ignore, incl. in bindings
auto [x, _, z] = std::tuple{1, 2, 3};   // ignore the middle; multiple '_' allowed
```

```
   '_' = "I don't care about this / don't warn about unused."
         Can appear multiple times without redeclaration errors.
```

---

## 8. Other C++26 items touching generic/compile-time code

```
   * Contracts (P2900): pre/post/assert; interact with templated interfaces.
   * User-generated static_assert messages: static_assert(cond, constexpr-string)
       -> build the message with constexpr code (great for template diagnostics).
   * #embed: embed binary data as an initializer (constexpr-friendly).
   * std::execution (senders/receivers): heavily templated async framework.
   * More constexpr-ified standard library, constexpr containers/algorithms.
   * Erroneous behavior for uninitialized reads (safety), placeholder concepts.
```

The **user-generated `static_assert` message** pairs beautifully with templates:

```cpp
template <class T>
struct Buffer {
    static_assert(std::is_trivially_copyable_v<T>,
        std::format("Buffer<T> needs trivially copyable T; got size {}",
                    sizeof(T)));   // message computed at compile time (C++26)
};
```

---

## 9. Before/after: writing a struct printer

```
   BEFORE C++26 (needs a macro or manual boilerplate per struct):
   ---------------------------------------------------------------
   #define REFLECT(...) /* ...50 lines of macro magic... */
   struct Point { int x,y; };  REFLECT(Point, x, y)
   // or hand-write operator<< for every struct.

   C++26 (works for ANY struct, zero per-type code):
   ---------------------------------------------------------------
   template <class T> void print(const T& o) {
       template for (constexpr auto m : nonstatic_data_members_of(^^T))
           std::cout << identifier_of(m) << '=' << o.[:m:] << ' ';
   }
```

```
   Impact: reflection + expansion statements collapse whole libraries
   (serialization, JSON, ORM, bindings) into a handful of generic functions.
```

---

## 10. Summary & how to try it

<!--diagram
title: C++26 template features
box[green] Key points
  text: **Pack indexing:** `args...[i]`, `Ts...[i]`
  text: **Binding packs:** `auto [head, ...tail] = t;`
  text: **Expansion stmt:** `template for (auto e : pack-or-tuple) {...}`
  text: **Reflection:** `^^X` (reflect) / `[: r :]` (splice) / `std::meta`
  text: → enum↔string, serialization, generic `==`, DI: no macros
  text: `= delete("reason")`: friendly deleted-function diagnostics
  text: `friend Friends...;` variadic friends
  text: `constexpr` bindings, `_` placeholder, `constexpr static_assert` msg
-->
```
 +------------------------------------------------------------------+
 | Pack indexing:      args...[i],  Ts...[i]                        |
 | Binding packs:      auto [head, ...tail] = t;                    |
 | Expansion stmt:     template for (auto e : pack-or-tuple) {...}  |
 | Reflection:         ^^X (reflect) / [: r :] (splice) / std::meta |
 |    -> enum<->string, serialization, generic ==, DI: no macros.   |
 | = delete("reason"): friendly deleted-function diagnostics.       |
 | friend Friends...;  variadic friends.                            |
 | constexpr bindings, '_' placeholder, constexpr static_assert msg.|
 +------------------------------------------------------------------+
```

To experiment today:

```bash
# pack indexing / bindings packs (mature-ish):
clang++ -std=c++2c examples/ch15_pack_indexing.cpp -o /tmp/d && /tmp/d

# reflection: needs a P2996-enabled compiler build; see the example header note.
```

Next: [16-pitfalls-and-best-practices.md](16-pitfalls-and-best-practices.md).
