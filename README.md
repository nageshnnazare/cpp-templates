# C++ Templates — The One-Stop Mastery Guide (up to C++26)

> A deep, example-driven, diagram-heavy guide to becoming an **expert** in C++
> templates. Everything from "what is a template" to reflection, pack indexing,
> and the C++26 additions. Every concept is paired with runnable code and ASCII
> diagrams that show what the compiler is *actually* doing.

---

## Who this is for

You have **intermediate C++ knowledge** (you know classes, references, `const`,
overloading, the STL at a user level) and you want to *truly* understand
templates — not just copy patterns, but know **why** they work, **how** the
compiler processes them, and **which** tool to reach for.

By the end you will be able to:

- Read and write any template code in production C++.
- Reason about **two-phase lookup**, **argument deduction**, and **overload
  resolution** the way a compiler does.
- Use **variadic templates**, **SFINAE**, **concepts**, and **metaprogramming**
  fluently.
- Understand the modern toolbox: `if constexpr`, `constexpr`/`consteval`,
  concepts, ranges-style constraints, and the **C++26** features (pack indexing,
  `= delete("reason")`, static reflection, `constexpr` structured bindings, etc.).

---

## How to read this guide

The chapters build on each other. If you are new to templates, read in order.
If you are here for a specific topic, jump straight to it — each file is
self-contained and cross-links the prerequisites.

| # | File | Topic |
|---|------|-------|
| 00 | [00-mental-model.md](00-mental-model.md) | What a template *is*: the compiler's view, two-phase compilation |
| 01 | [01-function-templates.md](01-function-templates.md) | Function templates, instantiation, overloading |
| 02 | [02-class-templates.md](02-class-templates.md) | Class templates, member templates, CTAD |
| 03 | [03-template-parameters.md](03-template-parameters.md) | Type / non-type / template-template parameters, `auto` NTTPs |
| 04 | [04-argument-deduction.md](04-argument-deduction.md) | Template Argument Deduction (TAD) rules in depth |
| 05 | [05-specialization.md](05-specialization.md) | Full & partial specialization, tag dispatch |
| 06 | [06-variadic-templates.md](06-variadic-templates.md) | Parameter packs, fold expressions, recursion |
| 07 | [07-sfinae.md](07-sfinae.md) | SFINAE, `enable_if`, `void_t`, detection idiom |
| 08 | [08-type-traits.md](08-type-traits.md) | `<type_traits>`, writing your own traits |
| 09 | [09-perfect-forwarding.md](09-perfect-forwarding.md) | Forwarding references, `std::forward`, reference collapsing |
| 10 | [10-crtp-and-patterns.md](10-crtp-and-patterns.md) | CRTP, policy-based design, mixins, type erasure |
| 11 | [11-metaprogramming.md](11-metaprogramming.md) | TMP: compile-time computation, type lists, `constexpr` |
| 12 | [12-constexpr-consteval.md](12-constexpr-consteval.md) | `constexpr`, `consteval`, `constinit`, `if constexpr` |
| 13 | [13-concepts.md](13-concepts.md) | Concepts & constraints (C++20) — the modern SFINAE |
| 14 | [14-cpp23-features.md](14-cpp23-features.md) | C++23 template-relevant features (`if consteval`, deducing `this`, etc.) |
| 15 | [15-cpp26-features.md](15-cpp26-features.md) | C++26: pack indexing, reflection, `= delete("reason")`, and more |
| 16 | [16-pitfalls-and-best-practices.md](16-pitfalls-and-best-practices.md) | Gotchas, error-message survival, best practices |
| 17 | [17-cheatsheet.md](17-cheatsheet.md) | Quick reference / cheatsheet |

Runnable examples live in [`examples/`](examples/). A build helper is provided.

---

## Building the examples

All examples are single-file programs. Compile with a recent compiler:

```bash
# C++20 examples (concepts, most chapters)
clang++ -std=c++20 -Wall -Wextra examples/ch06_fold_print.cpp -o /tmp/demo && /tmp/demo

# C++23 / C++26 examples need a bleeding-edge compiler
clang++ -std=c++2c -Wall -Wextra examples/ch15_pack_indexing.cpp -o /tmp/demo
```

Use the helper script to build everything your compiler supports:

```bash
./examples/build_all.sh
```

> **Compiler support note (2026):** GCC 14+/15 and Clang 18+/20 cover C++20/23
> fully. C++26 features land incrementally — pack indexing is in GCC 15 / Clang
> 19; **static reflection** (`^^`, `std::meta`) is experimental and may require a
> special branch (e.g. Clang's `p2996` fork or EDG). Each C++26 example notes its
> availability.

---

## The 30-second map of the whole topic

```
                          C++ TEMPLATES
                               |
        +----------------------+----------------------+
        |                      |                      |
   WHAT you make          HOW you feed it        WHAT you compute
        |                      |                      |
  +-----+-----+          +-----+------+         +-----+------+
  | function  |          | type param |         | if constexpr|
  | class     |          | NTTP       |         | constexpr   |
  | variable  |          | template   |         | consteval   |
  | alias     |          |  template  |         | traits      |
  | concept   |          | pack (...) |         | reflection  |
  +-----------+          +------------+         +-------------+
        |                      |                      |
        +----------+-----------+-----------+----------+
                   |                       |
             CONSTRAINING             SELECTING
                   |                       |
             +-----+-----+           +-----+------+
             | concepts  |           | overload   |
             | requires  |           | specialize |
             | SFINAE    |           | tag dispatch|
             +-----------+           +------------+
```

Start with [00-mental-model.md](00-mental-model.md).
