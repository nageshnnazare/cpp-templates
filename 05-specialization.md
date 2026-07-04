# 05 — Template Specialization (Full & Partial)

Specialization lets you say: *"for this particular argument (or family of
arguments), do something different."* It is how `std::vector<bool>` is a bit-set,
how type traits work, and how you customize behavior per type.

---

## 1. The vocabulary

```
  PRIMARY TEMPLATE      the general recipe
  FULL SPECIALIZATION   a recipe for ONE specific argument set (template<>)
  PARTIAL SPECIALIZATION a recipe for a FAMILY (still has template params)
```

```
   template<class T> struct S      ;   // PRIMARY
   template<>        struct S<int> ;   // FULL: exactly int
   template<class T> struct S<T*>  ;   // PARTIAL: any pointer
```

Only **class templates** (and variable templates) can be *partially*
specialized. **Function templates cannot** be partially specialized — you
overload instead (§6).

---

## 2. Full specialization

```cpp
template <class T>            // primary
struct TypeName {
    static const char* get() { return "unknown"; }
};

template <>                   // full specialization for int
struct TypeName<int> {
    static const char* get() { return "int"; }
};

template <>                   // for double
struct TypeName<double> {
    static const char* get() { return "double"; }
};
```

```
   TypeName<int>::get()     -> "int"      (uses specialization)
   TypeName<double>::get()  -> "double"   (uses specialization)
   TypeName<char>::get()    -> "unknown"  (falls back to primary)
```

Selection is like a lookup: the compiler picks the most specific matching
version.

```
              TypeName<char>
                    |
        is there a full spec for char? --no--> use primary  -> "unknown"

              TypeName<int>
                    |
        is there a full spec for int?  --yes--> "int"
```

### A specialization can have a *completely different* body/layout

```cpp
template <class T>
struct Storage { T value; };          // holds a T

template <>
struct Storage<bool> {                // totally different implementation
    unsigned char bits;               // pack bools tightly, etc.
    void set(bool b) { bits = b; }
};
```

The specialization need not share members with the primary. `std::vector<bool>`
famously does this (and famously causes surprises).

---

## 3. Partial specialization

Applies to a **pattern** of arguments while keeping some parameters open.

```cpp
template <class T>
struct IsPointer            { static constexpr bool value = false; };

template <class T>
struct IsPointer<T*>        { static constexpr bool value = true; };  // any T*

// usage
static_assert(!IsPointer<int>::value);
static_assert( IsPointer<int*>::value);
static_assert( IsPointer<double*>::value);
```

```
   IsPointer<int>    matches primary          -> false
   IsPointer<int*>   matches T* (T=int)        -> true
   IsPointer<char**> matches T* (T=char*)      -> true
```

More patterns:

```cpp
template <class T> struct Traits;                       // primary (undefined)
template <class T> struct Traits<const T> { /* strip const */ };
template <class T> struct Traits<T&>      { /* ref */ };
template <class T, class U> struct Traits<std::pair<T,U>> { /* pairs */ };
template <class T, std::size_t N> struct Traits<T[N]> { /* arrays, know N */ };
```

```
   Pattern              Matches                       Binds
   const T              const X                       T = X
   T&                   X&                            T = X
   std::pair<T,U>       std::pair<A,B>                T=A, U=B
   T[N]                 X[k]                          T=X, N=k
```

This is *the* mechanism behind `<type_traits>`
([08-type-traits.md](08-type-traits.md)).

---

## 4. How the compiler chooses among partial specializations

When multiple partial specializations match, the compiler picks the **most
specialized** using *partial ordering* (roughly: "if spec A can handle
everything B can, but not vice versa, then B is more specialized").

```cpp
template <class T>            struct S      { int id = 0; };  // primary
template <class T>            struct S<T*>  { int id = 1; };  // any pointer
template <>                   struct S<int*>{ int id = 2; };  // exactly int*
```

```
   S<double*>   -> matches primary? yes. matches T*? yes.  -> T* wins (id 1)
   S<int*>      -> matches primary, T*, AND full<int*>     -> full wins (id 2)
   S<double>    -> only primary                            -> id 0

   Specialization ladder (most specific first):
       S<int*>  >  S<T*>  >  S<T>
```

If two partial specs match and neither is more specialized -> **ambiguous
error**.

---

## 5. Variable template specialization (C++14)

Variable templates can be specialized too — this is the modern idiom for trait
*values*:

```cpp
template <class T> constexpr bool is_void_v = false;   // primary
template <>        constexpr bool is_void_v<void> = true;  // full spec

template <class T> constexpr int rank_v = 0;               // primary
template <class T> constexpr int rank_v<T[]>  = 1;         // partial
template <class T, std::size_t N> constexpr int rank_v<T[N]> = 1;  // partial
```

```
   is_void_v<int>   -> false
   is_void_v<void>  -> true
   rank_v<int>      -> 0
   rank_v<int[]>    -> 1
```

---

## 6. Why functions use OVERLOADING, not partial specialization

You *can* fully specialize a function template, but you **cannot** partially
specialize it. And even full specialization behaves surprisingly:

```cpp
template <class T> void f(T)  { std::puts("primary"); }
template <class T> void f(T*) { std::puts("overload for T*"); } // OVERLOAD, not spec
template <>        void f<int>(int) { std::puts("full spec int"); }
```

The subtle trap (Sutter's "gotcha"): a full specialization attaches to whichever
**primary/overload** was selected *first* by overload resolution, which can
route your specialization somewhere you didn't intend. **Rule:**

> For functions, express variation with **overloads** (and, in modern code,
> **concepts**/`if constexpr`). Reserve specialization for **class/variable
> templates**.

```
   Function customization      Preferred tool
   ------------------------     -----------------------------
   "different type category"    overload set
   "compile-time branch"        if constexpr (chapter 12)
   "constrain applicability"    concepts (chapter 13) / SFINAE (07)
   class trait value            variable/class template specialization
```

---

## 7. Tag dispatch — specialization's cousin

Before `if constexpr`/concepts, the way to select an implementation at compile
time based on a *property* was **tag dispatch**: pick an overload using a small
empty "tag" type derived from a trait.

```cpp
#include <iterator>
#include <type_traits>

// Two implementations, selected by iterator category tag:
template <class It, class Dist>
void advance_impl(It& it, Dist n, std::random_access_iterator_tag) {
    it += n;                              // O(1) for vectors
}
template <class It, class Dist>
void advance_impl(It& it, Dist n, std::input_iterator_tag) {
    while (n-- > 0) ++it;                 // O(n) for lists
}

template <class It, class Dist>
void my_advance(It& it, Dist n) {
    // pick the overload by constructing the category tag:
    advance_impl(it, n,
                 typename std::iterator_traits<It>::iterator_category{});
}
```

```
   my_advance(vec_it, 5)
        |
        category = random_access_iterator_tag
        |
        v  overload resolution matches the random-access overload
   it += 5      (fast path)

   my_advance(list_it, 5)
        |
        category = bidirectional -> derives from input_iterator_tag
        |
        v
   loop ++it 5 times   (slow path)
```

This is exactly how `std::advance`, `std::distance`, etc. were historically
implemented. In modern C++ you'd often write `if constexpr` or constrained
overloads instead, but tag dispatch is still everywhere in the standard library
and worth recognizing.

---

## 8. `if constexpr` — the modern alternative to many specializations

Instead of writing two class specializations or overloads, branch inside one
function; the false branch is *discarded* (not compiled for that instantiation):

```cpp
template <class T>
std::string describe(const T& x) {
    if constexpr (std::is_integral_v<T>) {
        return "integer: " + std::to_string(x);
    } else if constexpr (std::is_floating_point_v<T>) {
        return "float: " + std::to_string(x);
    } else {
        return "other";
    }
}
```

```
   describe(42)      -> integral branch kept, others DISCARDED
   describe(3.14)    -> float branch kept
   describe("hi")    -> else branch kept
   The discarded branches are not type-checked against T -> no errors.
```

Full treatment in [12-constexpr-consteval.md](12-constexpr-consteval.md).

---

## 9. Worked example: serialize with class-template specialization

```cpp
#include <iostream>
#include <string>
#include <vector>

template <class T>
struct Serializer {
    static std::string to_str(const T& v) { return std::to_string(v); }
};

template <>
struct Serializer<std::string> {
    static std::string to_str(const std::string& v) { return '"' + v + '"'; }
};

template <class T>
struct Serializer<std::vector<T>> {          // partial spec for any vector
    static std::string to_str(const std::vector<T>& v) {
        std::string out = "[";
        for (std::size_t i = 0; i < v.size(); ++i) {
            out += Serializer<T>::to_str(v[i]);          // recurse!
            if (i + 1 < v.size()) out += ", ";
        }
        return out + "]";
    }
};

template <class T>
std::string serialize(const T& v) { return Serializer<T>::to_str(v); }

int main() {
    std::cout << serialize(42) << '\n';                     // 42
    std::cout << serialize(std::string("hi")) << '\n';      // "hi"
    std::cout << serialize(std::vector<int>{1,2,3}) << '\n';// [1, 2, 3]
    std::vector<std::vector<int>> nested{{1,2},{3}};
    std::cout << serialize(nested) << '\n';                 // [[1, 2], [3]]
}
```

```
   serialize(nested)
      Serializer<vector<vector<int>>>          (partial spec)
        -> for each element Serializer<vector<int>>   (partial spec)
             -> for each element Serializer<int>       (primary)
   Recursion via specialization dispatch.
```

Runnable: [`examples/ch05_serialize.cpp`](examples/ch05_serialize.cpp).

---

## 10. Summary

<!--diagram
title: Specialization
box[green] Key points
  text: **Full spec:** `template<> struct S<int> {...};` (one type)
  text: **Partial spec:** `template<class T> struct S<T*> {...};` (a family)
  text: Classes & variable templates only (**not** functions)
  text: Most-specialized match wins; ties = ambiguous
  text: Functions → use **overloads** / `if constexpr` / concepts, not spec
  text: Tag dispatch: classic compile-time selection via trait tags
  text: `if constexpr`: modern one-function alternative
-->
```
 +------------------------------------------------------------------+
 | FULL spec:    template<> struct S<int> {...};   (one type)        |
 | PARTIAL spec: template<class T> struct S<T*> {...}; (a family)    |
 |   - classes & variable templates only (NOT functions)            |
 |   - most-specialized match wins; ties = ambiguous               |
 |                                                                  |
 | Functions -> use OVERLOADS / if constexpr / concepts, not spec.  |
 | Tag dispatch: classic compile-time selection via trait tags.    |
 | if constexpr: modern one-function alternative.                   |
 +------------------------------------------------------------------+
```

Next: [06-variadic-templates.md](06-variadic-templates.md).
