---
title: "Part 1 : Why C++ Was Created"
date: 2026-10-28 00:00:00 +0530
categories: [C++, Understanding C++ as It Evolves]
tags: [cpp, cpp-evolution, language-design, history]
description: "The problem behind C++: combining support for organizing complex programs with the efficiency and flexibility required for systems programming."
series: understanding-cpp-as-it-evolves
series_title: Understanding C++ as It Evolves
part: 1
---

C gives us direct ways to describe computation and work with memory. We can build operating systems, simulations, and large applications with it. So what problem was C++ trying to solve?

The answer begins with a difficulty that grows with the program: keeping its organization understandable while still using the machine efficiently.

## Two Useful Qualities, in Different Tools

During his doctoral work at Cambridge, Bjarne Stroustrup built a simulator to study system software. He used Simula, whose classes helped him represent the system as cooperating components. That organization remained manageable as the simulator grew.

The performance of the Simula system he was using, however, made the simulator impractical for obtaining the results he needed. He rewrote it in BCPL. The new version ran fast enough, but writing and debugging it was much harder.

These experiences suggested a goal: preserve the facilities that helped him structure a complex program while obtaining the performance needed to run it usefully. This was a lesson from particular tools and implementations, rather than proof that abstraction must be slow.

In 1979, at Bell Labs, work on distributing parts of the UNIX kernel brought him back to questions of system organization and communication. He began developing **C with Classes**, combining ideas from Simula with C's suitability for systems programming. Stroustrup describes this progression in [A History of C++: 1979–1991](https://www.stroustrup.com/hopl2.pdf), sections 2.1–2.2.

## Why Classes Mattered

A large program contains concepts that are richer than individual machine values: a connection, a task, a matrix, a file. Each has data, permitted operations, and rules that those operations must preserve.

We can represent these concepts in C using structures and functions. A carefully designed C interface can also hide its representation. The question is how much support the language provides for expressing and checking that organization.

Classes give a user-defined type a place for its representation and operations, with access rules controlling how they are used. Construction and destruction give objects defined beginnings and ends. Inheritance and virtual functions provide ways to express related types and select behaviour through an interface.

These mechanisms let us put more of a program's design into its types. They do not automatically produce a good design; they give us tools for defining one. And classes are only part of C++: its support for generic programming later became equally important. Stroustrup's [overview of the language](https://www.stroustrup.com/C++.html) describes these complementary roles.

## The Constraint: Keep Abstraction Affordable

Organizing a program would be less useful for systems work if every abstraction imposed an unacceptable execution or memory cost.

C++ therefore developed around a demanding goal: let programmers express higher-level ideas while retaining control over representation and resource use. Its **zero-overhead principle** has two parts: unused facilities should not impose costs, and a used abstraction should be implementable as efficiently as equivalent hand-written functionality.

This is a design principle, not a guarantee that every C++ program is efficient. A general-purpose abstraction may still do more work than a specialized solution needs. We must understand the behaviour we choose. Stroustrup explains this distinction in [C++ — an Invisible Foundation of Everything](https://accu.org/journals/overload/29/161/stroustrup/).

Compatibility also mattered. Building on C let the language work within an existing world of code, tools, and systems expertise. That continuity introduced compromises: C++ retained much familiar syntax and many low-level facilities, rather than becoming a language designed entirely afresh.

## From C with Classes to a Standard Language

C with Classes evolved into C++, with additions including virtual functions, overloading, and references. The first commercial release followed in 1985. The first ISO standard arrived in 1998, after substantial development of both the language and its library.

By then, C++ included templates, exceptions, and a standard library containing strings, containers, iterators, and algorithms. The containers and algorithms associated with the STL demonstrated another kind of abstraction: code reusable across types without requiring those types to share an inheritance hierarchy.

C++98 therefore describes the first standardized foundation, not the year those ideas were all invented. The [Computer History Museum's C++ collection](https://softwarepreservation.computerhistory.org/c_plus_plus/) records their development and standardization.

## What the Later Versions Added

The language continued evolving as experience exposed awkward interfaces, missing facilities, and opportunities for better expression. This is the route we will follow through the series:

| Version | Selected additions we will examine |
|---|---|
| **C++98** | The standardized foundation: classes, object lifetime, templates, exceptions, containers, and algorithms |
| **C++03** | Corrections and clarifications to that foundation |
| **C++11** | Move semantics, smart pointers such as `unique_ptr` and `shared_ptr`, lambdas, type deduction with `auto`, `constexpr`, and a standard threading and memory model |
| **C++14** | Generic lambdas, return type deduction, more flexible `constexpr` functions, and `make_unique` |
| **C++17** | Structured bindings, `if constexpr`, `optional`, `variant`, `string_view`, and filesystem facilities |
| **C++20** | Concepts, ranges, coroutines, modules, and further compile-time facilities |
| **C++23** | `expected`, `print`, `mdspan`, and further language and ranges improvements |

The table is a map, not a prerequisite list. We will introduce these facilities individually, with enough context to understand what each contributes. The [C++11 FAQ](https://www.stroustrup.com/C++11FAQ.html), [Clang language tables](https://clang.llvm.org/cxx_status.html), and libc++'s [C++17](https://libcxx.llvm.org/Status/Cxx17.html) and [C++23](https://libcxx.llvm.org/Status/Cxx23.html) tables document the additions and their associated proposals.

The central ambition remains recognizable: make complex software easier to express and maintain while using hardware effectively. Later versions give us new ways to pursue it, alongside older approaches that existing programs still use.

We will begin the foundations with classes: what a user-defined type adds beyond grouping data, and how its interface can express the rules for using that data.

{% include series-nav.html %}
