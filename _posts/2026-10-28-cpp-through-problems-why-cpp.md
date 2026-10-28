---
title: "Part 1 : Learning C++ When You Already Know C"
date: 2026-10-28 00:00:00 +0530
categories: [C++, C++ Through Problems]
tags: [cpp, cpp-through-problems, fundamentals]
description: "A learning path from C to C++: understand the foundations, then follow the problems behind features introduced in successive versions."
series: cpp-through-problems
series_title: C++ Through Problems
part: 1
---

If you already know C, a small C++ program can look familiar enough to follow. There are functions, loops, pointers, and structures. You can usually find the calculation and understand what it is trying to do.

Then you encounter a constructor, a reference, or a template. An explanation of one leads to another. To understand a smart pointer, you need to understand ownership and destruction. To understand moving an object, you need to understand what copying that object would mean. Soon there are several unfamiliar concepts open at once, and it becomes difficult to decide which one to learn first.

Knowing C gives us a useful starting point. What we need next is an order in which the C++ ideas can build on one another.

That is the purpose of this series. We will begin with problems we can already express in C, examine the responsibilities those solutions leave with the programmer, and introduce the C++ mechanisms that help organize them. Once those foundations are clear, we will follow the language through its different versions and ask what each addition makes possible or easier to express.

## We Already Know How to Represent Data

Suppose a program needs to work with a sequence of measurements. In C, we might represent it like this:

```c
struct Samples {
    int *data;
    size_t size;
    size_t capacity;
};
```

Here, `size` records how many measurements are present, while `capacity` records how many elements the allocated storage can hold. Assume this structure owns its allocation.

The declaration is small, but using it correctly depends on rules that the declaration does not enforce. `size` must not exceed `capacity`. The pointer must refer to sufficient storage whenever capacity is nonzero. When the collection grows, its fields must still agree after the change. Eventually, its allocation must be released.

We can write functions to initialize the structure, append an element, and destroy it:

```c
int samples_init(struct Samples *samples);
int samples_append(struct Samples *samples, int value);
void samples_destroy(struct Samples *samples);
```

This is a workable design. C programs use interfaces like this to manage all kinds of resources.

But consider what happens when we write:

```c
struct Samples second = first;
```

The structure's fields are copied, including the pointer. The allocation itself is not duplicated. If both structures are then treated as independent owners and destroyed, the program may free the same allocation twice.

We need a policy: perhaps copying is forbidden, perhaps an explicit operation makes an independent copy, or perhaps ownership is transferred. We also need callers to follow that policy.

The interesting question is what happens when these rules become part of the type's behaviour.

## A Type Can Take Responsibility for Its Rules

C++ lets a class define how its objects are initialized, destroyed, copied, and assigned. It can also restrict direct access to the representation, so code using an object works through operations that preserve its rules.

Each of those facilities answers a different question.

A constructor gives initialization a defined place. A destructor gives end-of-lifetime cleanup a defined place. Copy operations let a type specify what copying its value means, or prevent copying when that operation does not make sense.

We will examine these mechanisms separately and then put them together. Introducing all of them in a complete resource-owning class at the beginning would recreate the problem we are trying to avoid: too many ideas arriving before their relationships are clear.

For now, the C structure gives us something concrete to carry forward. We already understand its representation. What we want to learn is how C++ lets us express the rules around that representation, and how those rules affect code that uses it.

## The Order We Will Follow

Before implementing that class, we need a few smaller pieces.

We will begin with the practical differences that matter when moving from C source to C++: compiling in the right language mode, declarations and type checking, initialization, references, and function overloading. The aim is to establish the vocabulary that later examples depend on. We will assume familiarity with C functions, pointers, arrays, control flow, and separate compilation.

From there, we will build classes around familiar data. This leads naturally to access control, constructors, destructors, and the distinction between an object's storage and its lifetime.

Once lifetime is clear, we can connect it to resources. We will follow an allocation or an open file through normal execution, early returns, and failures. That gives RAII a problem to solve and gives us a reason to study exceptions alongside cleanup.

Copying comes next. We will distinguish copying a pointer from copying the resource it refers to, and examine the behaviour a resource-owning type needs. Those decisions prepare us for move semantics later.

Templates will enter when we have an operation or a type whose structure should be reusable across element types. With that foundation, containers, iterators, and algorithms become a connected design we can investigate.

We will use standard-library types along the way. Using a vector does not require first implementing one. Building small types of our own will help us understand the rules that make library types behave as they do.

## Where the C++ Versions Fit

The foundations give us the context for the language's evolution.

We will first establish the core ideas present in C++98/03. Then we will work through additions in C++11, C++14, C++17, C++20, and C++23, ordering topics within each version by their dependencies.

C++11 will be a substantial part of the journey. Once we understand copying and ownership, we can investigate moving resources and representing ownership with smart pointers. Once callable objects and algorithms are familiar, lambdas have a clear role. Compile-time evaluation, variadic templates, and concurrency will each get their own prerequisites and examples.

Later versions will build on that picture. Generic lambdas extend an idea we already know. `if constexpr` helps us express choices inside generic code. Concepts let us state requirements on template arguments. Ranges build on our experience with collections, iterators, and algorithms.

The chapter pattern will stay consistent: begin with a task, attempt it with the tools already introduced, identify the difficulty, and examine the new feature. We will then consider where it helps, what it costs, and which earlier approaches remain useful.

Examples will explain how a feature can be useful. When discussing its historical motivation, we will also consult proposals and first-hand design accounts rather than treating our example as the whole reason the feature exists.

## C Gives Us a Starting Point, but Some Assumptions Need Checking

Familiar syntax can hide differences. C++ has its own rules for conversions, initialization, overload resolution, and object lifetime. Existing C code is not automatically valid C++, and compiling it as C++ does not by itself give it a useful C++ design.

We will make those differences explicit when they affect an example. In particular, we will distinguish obtaining raw storage from constructing an object in that storage. That distinction becomes important once a type has initialization and cleanup behaviour of its own.

We will also keep implementation observations separate from language guarantees. Generated code can help us investigate the cost of a feature, but one compiler's output for one example cannot establish how every valid program must behave.

## Where We Begin Next

Our first task is to make the boundary between C and C++ concrete.

In the next part, we will take a small C-style program and compile it as C++. We will examine the language mode, a few assumptions that stop working, and the first choices C++ gives us for expressing the program more clearly. We will keep the build discussion focused on what changes for a reader who already knows separate compilation and linking in C.

After that, we can begin building the type model: initialization, references, and functions, followed by classes and lifetime. Each step will supply something the next one needs.

The question we will return to throughout the series is simple: we already know how to perform the work in C; what does C++ let us express about that work, and when is that additional structure useful?

## Further Reading

- [Stroustrup's FAQ](https://www.stroustrup.com/bs_faq.html), including its discussion of C and C++ and the design of the language.
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines), for guidance we will revisit as interfaces, lifetime, and resource management enter the story.

{% include series-nav.html %}
