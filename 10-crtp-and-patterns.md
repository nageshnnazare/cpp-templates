# 10 — CRTP, Policies, Mixins & Type Erasure

Design patterns that are *built out of* templates. These are the idioms that
appear in real libraries (Eigen, ranges, standard library, embedded frameworks).

---

## 1. CRTP — Curiously Recurring Template Pattern

A class inherits from a template **instantiated with itself**:

```cpp
template <class Derived>
struct Base {
    void interface() {
        static_cast<Derived*>(this)->implementation();   // "static polymorphism"
    }
};

struct Widget : Base<Widget> {          // <-- inherits Base<Widget>
    void implementation() { std::puts("Widget impl"); }
};
```

```
   struct Widget : Base<Widget>
                        ^^^^^^
              the base is parameterized on the DERIVED type

   Base<Widget>::interface()
        static_cast<Widget*>(this)->implementation();
        |
   compiler KNOWS the derived type -> the call is resolved at COMPILE TIME
   -> no vtable, no virtual dispatch, fully inlinable.
```

### CRTP = "virtual functions without the vtable"

```
   Runtime polymorphism (virtual)      CRTP (static polymorphism)
   ------------------------------      --------------------------
   Base* p = &derived;                 Base<Derived>& r = derived;
   p->foo();  // vtable lookup          r.foo();   // resolved at compile time
   + heterogeneous containers          - each Base<X> is a different type
   - indirect call, no inline          + zero overhead, inlinable
   - runtime cost                      - can't store mixed types in one container
```

### Use case 1: inject shared behavior (mixin of methods)

```cpp
template <class Derived>
struct Comparable {
    friend bool operator!=(const Derived& a, const Derived& b) { return !(a == b); }
    friend bool operator> (const Derived& a, const Derived& b) { return b < a; }
    friend bool operator<=(const Derived& a, const Derived& b) { return !(b < a); }
    friend bool operator>=(const Derived& a, const Derived& b) { return !(a < b); }
};

struct Version : Comparable<Version> {
    int major, minor;
    friend bool operator==(const Version& a, const Version& b) {
        return a.major==b.major && a.minor==b.minor;
    }
    friend bool operator<(const Version& a, const Version& b) {
        return a.major<b.major || (a.major==b.major && a.minor<b.minor);
    }
};
// Version now has !=, >, <=, >= for free, derived from == and <.
```

```
   You define:  == and <
   CRTP adds:   !=, >, <=, >=      (all derived, all inlined)
```

> In C++20, `operator<=>` (the spaceship, chapter 14) makes much of this
> obsolete — but CRTP-based "boost.operators"-style mixins are everywhere in
> pre-C++20 code and still useful for non-ordering behavior.

### Use case 2: static interface / policy checking

```cpp
template <class Derived>
struct Shape {
    double area()      const { return self().area_impl(); }
    double perimeter() const { return self().perimeter_impl(); }
private:
    const Derived& self() const { return static_cast<const Derived&>(*this); }
};

struct Circle : Shape<Circle> {
    double r;
    double area_impl()      const { return 3.14159 * r * r; }
    double perimeter_impl() const { return 2 * 3.14159 * r; }
};
```

### CRTP pitfalls

```
  * Base<Derived> is instantiated with an INCOMPLETE Derived. You can't use
    sizeof(Derived) or Derived members in the Base class BODY at parse time —
    only inside member functions (instantiated later).
  * No common base type -> can't hold Circle and Square in one container
    (they are Shape<Circle> and Shape<Square>, unrelated). Combine with type
    erasure (§4) if you need that.
  * static_cast to the wrong Derived (copy/paste bug) is UB and silent.
    Deducing-this (C++23, chapter 14) removes this footgun.
```

---

## 2. Deducing `this` (C++23) — CRTP's successor for many cases

C++23 lets a member function deduce its own object type, eliminating the
`static_cast` in many CRTP uses:

```cpp
struct Shape {
    template <class Self>
    double area(this const Self& self) {    // 'this Self&' explicit object param
        return self.area_impl();            // self is the real derived type
    }
};
struct Circle : Shape {
    double r;
    double area_impl() const { return 3.14159 * r * r; }
};
```

```
   circle.area()
     Self deduced as Circle  -> self is a Circle&  -> self.area_impl() OK
   No Base<Derived> needed, no static_cast, no CRTP boilerplate.
```

See [14-cpp23-features.md](14-cpp23-features.md).

---

## 3. Policy-Based Design

Configure a class's behavior by passing **policy** template parameters — small
types that supply pieces of behavior. Popularized by Alexandrescu's *Modern C++
Design*.

```cpp
// Policies:
struct ThrowingCheck {
    static void check(bool ok) { if (!ok) throw std::out_of_range("bad"); }
};
struct NoCheck {
    static void check(bool) {}                 // compiles to nothing
};

// Host class parameterized on the policy:
template <class T, class CheckPolicy = ThrowingCheck>
class Vector {
    std::vector<T> data_;
public:
    T& at(std::size_t i) {
        CheckPolicy::check(i < data_.size());  // behavior injected by policy
        return data_[i];
    }
    void push_back(T v) { data_.push_back(std::move(v)); }
};

Vector<int>            safe;    // bounds-checked (throws)
Vector<int, NoCheck>   fast;    // checks compiled away -> zero overhead
```

```
   Vector<int, ThrowingCheck>::at   -> if(!ok) throw ...
   Vector<int, NoCheck>::at         -> check() is empty -> inlined to nothing

   Same source, different POLICY -> different behavior, chosen at compile time,
   with ZERO runtime cost for the "off" policy.
```

```
              +------------------------+
              |     Vector<T, Policy>  |
              +-----------+------------+
                          |
          policy plugs in |  CheckPolicy::check(...)
                          v
         +----------------+-----------------+
         | ThrowingCheck  | NoCheck | LogCheck | ...
         +----------------------------------+
   Mix & match orthogonal behaviors without a combinatorial class explosion.
```

Allocators (`std::vector<T, Alloc>`), comparators (`std::set<T, Compare>`), and
`std::char_traits` are all policy parameters in the standard library.

---

## 4. Type Erasure — runtime polymorphism *without* inheritance

CRTP gives compile-time polymorphism but no common type. Sometimes you need to
store *different* types behind one interface at **runtime** (like `std::function`,
`std::any`, `std::shared_ptr`'s deleter). **Type erasure** does this with
templates + a hidden virtual interface.

The recipe (three layers):

```
   +------------------------------------------------------+
   |  Wrapper (value type users hold): Drawable            |
   |     holds: unique_ptr<Concept>                        |
   +------------------------------------------------------+
                    |
   +----------------v-------------------------------------+
   |  Concept (abstract interface, INTERNAL): draw()=0     |
   +------------------------------------------------------+
                    ^  implemented by
   +----------------+-------------------------------------+
   |  Model<T> (template): stores a T, forwards draw()     |
   |     to the free function draw(const T&)              |
   +------------------------------------------------------+
```

```cpp
#include <memory>
#include <vector>
#include <iostream>

// The type-erased value type:
class Drawable {
    struct Concept {                          // internal interface
        virtual ~Concept() = default;
        virtual void do_draw() const = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
    };
    template <class T>
    struct Model : Concept {                  // template adapter per T
        T obj;
        explicit Model(T o) : obj(std::move(o)) {}
        void do_draw() const override { draw(obj); }         // ADL free function
        std::unique_ptr<Concept> clone() const override {
            return std::make_unique<Model>(*this);
        }
    };
    std::unique_ptr<Concept> self_;
public:
    template <class T>                        // accepts ANY drawable type
    Drawable(T x) : self_(std::make_unique<Model<T>>(std::move(x))) {}
    Drawable(const Drawable& o) : self_(o.self_->clone()) {}
    Drawable& operator=(Drawable o) { self_ = std::move(o.self_); return *this; }

    friend void draw(const Drawable& d) { d.self_->do_draw(); }  // dispatch
};

// Unrelated types, no common base, no inheritance:
struct Circle { };
struct Square { };
void draw(const Circle&) { std::cout << "circle\n"; }
void draw(const Square&) { std::cout << "square\n"; }

int main() {
    std::vector<Drawable> shapes;
    shapes.emplace_back(Circle{});
    shapes.emplace_back(Square{});
    for (const auto& s : shapes) draw(s);     // circle / square
}
```

```
   shapes: [ Drawable{Model<Circle>}, Drawable{Model<Square>} ]
                       |                        |
   draw(s) -> self_->do_draw() -> draw(obj)  (virtual dispatch inside)
   Users see ONE type (Drawable), store heterogeneous values, no base class
   required on Circle/Square. This is how std::function/std::any work.
```

Type erasure trades a heap allocation + one virtual call for **non-intrusive**
runtime polymorphism (the stored types don't need to inherit anything).

---

## 5. Mixins via variadic inheritance

Compose behavior by inheriting from several small templates:

```cpp
template <class Base> struct Serializable : Base { void save() const {/*...*/} };
template <class Base> struct Loggable     : Base { void log()  const {/*...*/} };

struct Empty {};
using Entity = Serializable<Loggable<Empty>>;   // stack the mixins
```

```
   Serializable<Loggable<Empty>>
        adds save()   adds log()
   -> Entity has both save() and log(), each defined once, reusable.
```

---

## 6. Expression templates (a peek)

Libraries like Eigen use templates to build an **expression tree at compile
time** so that `a + b + c` on big vectors runs in **one loop** with no
temporaries.

```
   a + b + c        (naive: two temporaries, three loops)
        |
   expression templates capture the STRUCTURE:
        Add< Add<Vec,Vec>, Vec >          (no computation yet)
        |
   assignment triggers ONE fused loop:
        for i: result[i] = a[i] + b[i] + c[i];
```

The types encode the operation; evaluation is deferred until assignment. This is
advanced but is *the* reason template-heavy math libraries are fast. (Modern
ranges/`views` achieve similar laziness more readably.)

---

## 7. Choosing a polymorphism strategy

```
  Need                                    Use
  -------------------------------------   ------------------------------------
  zero-overhead reuse of methods          CRTP mixin / deducing-this (C++23)
  compile-time configurable behavior      policy-based design
  heterogeneous container, non-intrusive  type erasure (std::function/any-like)
  heterogeneous container, you own types   classic virtual inheritance
  fused lazy computation                   expression templates / ranges views
```

Runnable: [`examples/ch10_crtp.cpp`](examples/ch10_crtp.cpp),
[`examples/ch10_type_erasure.cpp`](examples/ch10_type_erasure.cpp).

---

## 8. Summary

<!--diagram
title: CRTP and patterns
box[green] Key points
  text: **CRTP:** `struct D : Base<D>`. Static polymorphism, zero overhead, no vtable, but no common base type
  text: Deducing-this (C++23) replaces most CRTP `static_cast` boilerplate
  text: **Policy design:** inject orthogonal behavior via template params; "off" policies cost nothing
  text: **Type erasure:** Concept/Model/Wrapper → runtime polymorphism without inheritance (`std::function`/`any` pattern)
  text: Mixins: stack small templates. Expression templates: lazy fusion
-->
```
 +------------------------------------------------------------------+
 | CRTP: struct D : Base<D>. Static polymorphism, zero overhead,    |
 |        no vtable, but no common base type.                       |
 | Deducing-this (C++23) replaces most CRTP static_cast boilerplate.|
 | Policy design: inject orthogonal behavior via template params,   |
 |        "off" policies cost nothing.                              |
 | Type erasure: Concept/Model/Wrapper -> runtime polymorphism      |
 |        without inheritance (std::function/any pattern).          |
 | Mixins: stack small templates. Expression templates: lazy fusion.|
 +------------------------------------------------------------------+
```

Next: [11-metaprogramming.md](11-metaprogramming.md).
