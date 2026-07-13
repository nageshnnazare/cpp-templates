# 02 — Class Templates

A class template is a recipe for a **family of classes**. `std::vector<int>` and
`std::vector<std::string>` are two *different classes* baked from one recipe.

Prereqs: [00-mental-model.md](00-mental-model.md), [01-function-templates.md](01-function-templates.md).

---

## 1. Anatomy

```cpp
template <typename T>
class Box {
public:
    explicit Box(T value) : value_(value) {}
    const T& get() const { return value_; }
    void set(const T& v) { value_ = v; }
private:
    T value_;
};
```

Usage — **you must supply the argument** (before C++17 CTAD):

```cpp
Box<int> bi(42);
Box<std::string> bs("hi");
```

```
                 Box<T>  (recipe)
                    |
        +-----------+-----------+
   T=int|                       |T=string
        v                       v
   class Box<int>          class Box<std::string>
   { int value_; ... }     { std::string value_; ... }
```

Unlike function templates, **class template arguments are not deduced from
constructor arguments** — until **CTAD** (C++17), see §7.

---

## 2. Member functions are instantiated lazily

A crucial optimization/behavior: a member function of a class template is only
instantiated **when it is actually called**.

```cpp
template <class T>
struct Widget {
    void safe()   { /* uses only common ops */ }
    void risky()  { T{}.does_not_exist(); }   // ill-formed for most T
};

int main() {
    Widget<int> w;
    w.safe();     // fine
    // w.risky(); // ONLY this line would trigger the error
}
```

```
 Widget<int> instantiated:
   - class layout: yes
   - safe():  instantiated when called
   - risky(): NOT instantiated unless called  --> its errors stay dormant
```

This lets a class template support types that satisfy *some* but not all members.
`std::vector<T>` works with move-only types as long as you don't call the members
that require copy.

---

## 3. Defining members outside the class

Verbose but common in large codebases:

```cpp
template <class T>
class Stack {
public:
    void push(const T& x);
    T pop();
    bool empty() const { return data_.empty(); }
private:
    std::vector<T> data_;
};

// Out-of-line definitions repeat the template head:
template <class T>
void Stack<T>::push(const T& x) {
    data_.push_back(x);
}

template <class T>
T Stack<T>::pop() {
    T top = data_.back();
    data_.pop_back();
    return top;
}
```

```
 template <class T>          <-- head repeated
 void Stack<T>::push(...)    <-- Stack<T>:: qualifier (note the <T>)
 \__/  \___________/
 return  qualified name
```

All of this **must be in the header** (definitions needed at instantiation).

---

## 4. Non-type & multiple parameters

```cpp
template <class T, std::size_t N>
class Array {
    T data_[N];
public:
    constexpr std::size_t size() const { return N; }
    T& operator[](std::size_t i) { return data_[i]; }
};

Array<int, 8> a;   // N is part of the type: Array<int,8> != Array<int,4>
```

<!--diagram
title: NTTP encodes size in the type
box[blue] Array<int, 8>
  text: `int data_[8]`
box[blue] Array<int, 4>
  text: `int[4]`
box[gray] Note
  text: **Different types.** You cannot assign one to the other.
-->
```
   Array<int, 8>              Array<int, 4>
   +-----------------+        +---------+
   | int data_[8]    |        |int[4]   |   DIFFERENT types.
   +-----------------+        +---------+   You cannot assign one to the other.
```

This is exactly `std::array<T, N>`. NTTPs bake sizes/flags into the type for
zero runtime overhead.

---

## 5. Default template arguments

```cpp
template <class T, class Allocator = std::allocator<T>>
class MyVector { /* ... */ };

MyVector<int> v;                       // Allocator defaults
MyVector<int, MyAlloc<int>> v2;        // override
```

```
 template <class T, class Alloc = std::allocator<T>>
                              \______________________/
                               used if caller omits it
```

Defaults can refer to earlier parameters (`Allocator = std::allocator<T>` uses
`T`). Order matters: once you give a default, all following params need defaults
too (like function default args).

---

## 6. Member templates (a template inside a template)

A class template can have **member function templates**, enabling cross-type
operations:

```cpp
template <class T>
class Box {
    T value_;
public:
    explicit Box(T v) : value_(v) {}

    // member template: construct from a Box of another type
    template <class U>
    Box(const Box<U>& other) : value_(other.peek()) {}   // needs U -> T

    T peek() const { return value_; }
};

Box<double> d(Box<int>(3));   // uses the member template, int -> double
```

```
   Box<int>            Box<double>
   value_=3   ----->   value_=3.0
              member template ctor Box<double>::Box<int>
```

`std::vector`, `std::shared_ptr`, etc. use member templates for converting
constructors and assignment.

### Calling a dependent member template needs `.template`

```cpp
template <class C>
void f(C c) {
    c.template get<0>();   // 'template' disambiguator (see chapter 00 / 16)
}
```

---

## 7. Class Template Argument Deduction (CTAD) — C++17

Since C++17, you can often **omit** the template arguments and let the compiler
deduce them from the constructor call:

```cpp
std::vector v{1, 2, 3};        // deduces std::vector<int>
std::pair  p{1, 2.5};          // std::pair<int, double>
Box        b{42};              // Box<int>  (our Box above, if ctor allows)
```

```
   Box b{42};
        |
        v   CTAD looks at constructors and (optionally) deduction guides
   Box<int> b{42};
```

### Deduction guides

Sometimes the compiler can't figure it out (e.g., you store a *decayed* type).
You write a **deduction guide**:

```cpp
template <class T>
struct NamedValue {
    std::string name;
    T value;
    NamedValue(std::string n, T v) : name(std::move(n)), value(v) {}
};

// deduction guide: "when constructed from (string-ish, T), the class is
//                    NamedValue<T>"
template <class T>
NamedValue(std::string, T) -> NamedValue<T>;

NamedValue nv{"pi", 3.14};   // NamedValue<double>
```

```
   NamedValue nv{"pi", 3.14};
                     |
        guide:  NamedValue(string, T) -> NamedValue<T>,  T = double
                     |
   NamedValue<double> nv{...};
```

The classic library example:

```cpp
template <class Iter>
vector(Iter, Iter) -> vector<typename iterator_traits<Iter>::value_type>;
// so: std::vector v(first, last);  deduces element type from the iterator
```

CTAD applies to variable declarations, `new`, and function-style casts. It does
**not** apply to the right side of member/base initializers in all cases, and it
is all-or-nothing (you can't deduce *some* args).

---

## 8. Friends of class templates

Three flavors, commonly confused:

```cpp
template <class T>
class Box {
    T v_;
    // (a) friend a specific instantiation of a function template
    template <class U>
    friend void peek(const Box<U>&);

    // (b) friend a NON-template free function generated per instantiation
    friend std::ostream& operator<<(std::ostream& os, const Box& b) {
        return os << b.v_;   // defined inline; one operator<< per Box<T>
    }
};
```

The **hidden-friend** `operator<<` (b) is a best practice: it's only found by
ADL, avoids polluting the global namespace, and is generated per instantiation.

```
   For Box<int>:  a distinct  operator<<(ostream&, const Box<int>&)  exists.
   Found only via ADL when you do:  std::cout << box_of_int;
```

---

## 9. Nested types & the `typename` requirement

When you use a nested type of a *dependent* class, you need `typename`:

```cpp
template <class Container>
void first_elem(const Container& c) {
    typename Container::value_type x = *c.begin();   // 'typename' required
    // ...
}
```

Chapter 00 explains why. This is one of the most common template compile errors.

---

## 10. Static members

Each instantiation has its **own** static members:

```cpp
template <class T>
struct Counter {
    static inline int count = 0;   // C++17 inline: define in header
    Counter() { ++count; }
};

Counter<int> a, b;     // Counter<int>::count == 2
Counter<double> c;     // Counter<double>::count == 1  (separate!)
```

```
   Counter<int>::count      Counter<double>::count
        = 2                       = 1
   (independent storage per instantiation)
```

Before C++17 you had to define the static member out-of-line:
`template<class T> int Counter<T>::count = 0;`.

---

## 11. Worked example: a minimal `unique_ptr`-ish `Box`

```cpp
#include <iostream>
#include <utility>

template <class T>
class Owned {
    T* ptr_;
public:
    explicit Owned(T* p = nullptr) : ptr_(p) {}
    ~Owned() { delete ptr_; }

    Owned(const Owned&)            = delete;   // non-copyable
    Owned& operator=(const Owned&) = delete;

    Owned(Owned&& o) noexcept : ptr_(o.ptr_) { o.ptr_ = nullptr; }
    Owned& operator=(Owned&& o) noexcept {
        if (this != &o) { delete ptr_; ptr_ = o.ptr_; o.ptr_ = nullptr; }
        return *this;
    }

    T&  operator*()  const { return *ptr_; }
    T*  operator->() const { return  ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }
};

int main() {
    Owned<int> a(new int(7));
    std::cout << *a << '\n';        // 7
    Owned<int> b = std::move(a);    // move ctor; a is now empty
    std::cout << (a ? "a full" : "a empty") << '\n';   // a empty
    std::cout << *b << '\n';        // 7
}
```

Runnable: [`examples/ch02_owned.cpp`](examples/ch02_owned.cpp).

---

## 12. Summary

<!--diagram
title: Class templates
box[green] Key points
  text: `template<class T> class C { ... };` → family of classes
  text: Args explicit (`C<int>`) OR deduced via CTAD (C++17)
  text: Members instantiated lazily (only when called)
  text: Member templates enable cross-type ctors/ops
  text: Deduction guides steer CTAD
  text: Static members are per-instantiation; use inline (C++17)
  text: Prefer hidden friends for operators
  text: Definitions live in headers
-->
```
 +----------------------------------------------------------------+
 | template<class T> class C { ... };  -> family of classes       |
 |                                                                |
 | * Args explicit (C<int>) OR deduced via CTAD (C++17).          |
 | * Members instantiated lazily (only when called).              |
 | * Member templates enable cross-type ctors/ops.                |
 | * Deduction guides steer CTAD.                                 |
 | * Static members are per-instantiation; use inline (C++17).    |
 | * Prefer hidden friends for operators.                         |
 | * Definitions live in headers.                                 |
 +----------------------------------------------------------------+
```

Next: [03-template-parameters.md](03-template-parameters.md).
