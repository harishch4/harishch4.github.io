---
title: "Part 1 : Why C++? Starting with a Small Program"
date: 2026-10-28 00:00:00 +0530
categories: [C++, C++ Through Problems]
tags: [cpp, cpp-through-problems, fundamentals, standard-library]
description: "Starting with a small collection of numbers to understand the questions behind C++: representation, abstraction, ownership, and how the language evolves."
---

When I look at C++ as a list of features, it is easy to lose the connections between them. Classes, templates, references, move semantics, concepts, coroutines: each has its own vocabulary and rules. Learning the definition of each one does not necessarily explain why I would reach for it in a program.

I want to approach the language by following the questions that make those features useful. We will write a small program, notice what it asks us to manage, and then build the vocabulary needed to reason about it. Once the foundations are clear, we will follow the language through successive versions and examine what each addition helps us express.

This first chapter starts with a collection of numbers. It is deliberately small enough that we can keep the whole task in our heads.

## The path through this series

**C++ Through Problems** is a growing series of posts. You should be comfortable with variables, conditions, loops, and basic functions in some programming language; prior C++ knowledge is not required.

We will begin with compilation, types, functions, references, containers, classes, and object lifetime. Then we will examine the C++98/03 foundations: resource management, copying, templates, algorithms, exceptions, and runtime polymorphism.

From there, we will follow the language through successive versions:

| Version | Questions we will explore |
|---|---|
| C++11 | How can we transfer resources, express ownership, write small callable operations, and coordinate threads? |
| C++14 | How can generic lambdas, type deduction, and more flexible compile-time functions make these tools easier to use? |
| C++17 | How can we represent missing values and alternatives, simplify generic code, and borrow text safely? |
| C++20 | How can we express template requirements, compose range operations, suspend computations, and organize code with modules? |
| C++23 | How can we compose fallible operations, work with multidimensional data, and simplify everyday interfaces? |

Each part will remain a normal blog post, grouped under the C++ Through Problems category. As later parts are added, they will link back to the earlier explanations they build on.

For each feature, the aim is to understand the earlier approach, the limitation it exposes, the new solution, and its tradeoffs. Where we discuss historical design intent, we will distinguish our teaching examples from documented motivations and link to first-hand sources.

## Start with the work we want to do

Suppose a program receives some measurements and needs their total. For now, the measurements are whole numbers, the collection is small, and the total fits in an `int`.

We can write the calculation directly:

```cpp
int total = 12 + 18 + 15;
```

This expresses one particular calculation. As soon as the data comes from somewhere else, we need a representation for the collection and a way to visit its elements.

An array gives us a first representation:

```cpp
int samples[] = {12, 18, 15};
int total = 0;

for (int i = 0; i < 3; ++i) {
    total += samples[i];
}
```

`samples` contains three integers. The loop starts at index zero, reads each element, and adds it to `total`. The array's size is fixed; we cannot append a fourth element to that same array.

There is also a relationship we have written down twice: the initializer has three elements, and the loop uses the number `3`. Adding an element without updating the loop would silently leave a measurement out. We can derive the array's size instead, and later we will see several ways to do that.

For the moment, this exposes two separate questions. How should the program know how many elements to visit? And what should happen when the collection needs to grow?

## A collection that can grow

Here is a complete C++ program using a standard-library container:

```cpp
#include <iostream>
#include <vector>

int main()
{
    std::vector<int> samples{12, 18, 15};
    samples.push_back(21);

    int total = 0;
    for (int sample : samples) {
        total += sample;
    }

    std::cout << "Total: " << total << '\n';
}
```

The output is:

```text
Total: 66
```

There is unfamiliar syntax here, but we can connect each piece to its job.

| Piece | What it contributes |
|---|---|
| `#include <vector>` | Makes the standard library's vector declarations available |
| `std::vector<int>` | A growable sequence of integers |
| `{12, 18, 15}` | Supplies its initial elements |
| `samples.push_back(21)` | Appends one element |
| `for (int sample : samples)` | Visits each element, copying its integer value into `sample` |
| `#include <iostream>` | Makes the standard stream facilities used here available |
| `std::cout` | Writes to the standard output stream |
| `'\n'` | A newline character |

`std` is the namespace used by the standard library. The `::` connects that namespace name to a name inside it. `int main()` defines the entry function for this ordinary hosted program. Reaching its closing brace returns a success status, as if we had written `return 0;`.

The range-based loop follows the collection's elements without a separately maintained element count. We could also use a range-based loop with the earlier array. Choosing a vector and choosing that loop solve different parts of our problem.

### Build and run it

The complete example uses **C++11 or later**. Save it as `total.cpp`, or [download the source]({{ '/assets/code/cpp-through-problems/chapter-01/total.cpp' | relative_url }}), then run:

```bash
g++ -std=c++11 -Wall -Wextra -pedantic total.cpp -o total
./total
```

With Clang, replace `g++` with `clang++`. These commands assume the compiler and its C++ standard-library development files are installed. The next chapter will examine what happens between the source file and the executable.

## Where did the storage management go?

The interesting part of the vector example is the work we no longer spell out at each call site.

The vector keeps track of how many elements it contains and how much storage is available. When appending requires more storage, it arranges that storage and transfers the existing elements as needed. When the local vector's lifetime ends, it destroys its elements and releases the storage it owns.

We still need to understand those operations. Inserting an element may allocate memory, and growth may invalidate references or pointers to existing elements. Abstraction gives us a place to put that responsibility; it does not make the underlying work disappear.

Here the variable `samples` is a local object. Its lifetime ends when execution leaves its scope normally. Its owned element storage is managed separately by the vector. Describing the whole thing simply as being "on the stack" would hide that distinction.

This leads to one of the central questions in C++:

> Which object is responsible for a resource, and when does that responsibility end?

We will return to that question when we study constructors, destructors, and RAII. RAII connects resource ownership to object lifetime. The resource might be memory, an open file, or a held lock.

There is no explicit `new` or `delete` in our example. Using an existing resource-managing type is already useful before we learn to implement one.

## A type can carry behaviour

Compare these declarations:

```cpp
int total = 0;
std::vector<int> samples{12, 18, 15};
```

Both introduce objects with a type. An integer supports operations such as addition and comparison. A vector supports operations such as appending, obtaining its size, and visiting its elements.

C++ lets libraries define types with their own operations and lifetime behaviour. `std::vector` is supplied by the standard library; the general mechanisms for building such types are part of the language.

That distinction matters. A compiler has language rules for classes and templates. The standard library uses those facilities to provide containers and algorithms. We will also use them to build our own abstractions when a program needs a type that expresses its particular rules.

The `<int>` in `std::vector<int>` selects the element type. A vector of another supported element type uses the same container template. We will study templates after we have enough experience with ordinary types and functions to see what is being generalized.

## Abstraction has costs we can investigate

The vector program is easier to extend than the fixed-size example, but it makes a different storage choice.

If we know there will always be exactly three measurements, a fixed-size array may be sufficient. If the collection grows as input arrives, a vector can manage that growth. A suitable representation depends on the task.

For `int`, vector elements are stored contiguously. That property is useful when traversing them or passing a pointer and element count to an interface. Appending is amortized constant time, but a particular append can require allocating new storage and transferring existing elements.

These are properties we can reason about before inspecting assembly. Later, when we measure or examine generated code, we will keep the boundaries clear: what the language guarantees, what the library promises, and what a particular implementation does.

The fact that a loop is short does not establish that it is fast. Equally, the fact that a library operation hides several steps does not establish that it is slow. We need to know which work the task requires and which work our chosen representation adds.

## Where the versions enter the story

This tiny program already combines ideas from different stages of C++.

| Facility used here | Standard context |
|---|---|
| `std::vector` and the standard stream library | Present in C++98 |
| Initializing this vector from a braced list of elements | Available since C++11 |
| Range-based `for` | Introduced in C++11 |

Braces themselves were not new in C++11: the earlier array initialization also uses braces. What matters is the particular initialization facility we are using.

This is how we will approach versions throughout the series. We will identify the feature precisely, show the earlier way to express the task, and ask what the new form changes.

For example, an earlier vector traversal could use an explicitly written iterator type. A range-based loop gives us a direct way to express visiting a range's elements. That helps with readability, but it does not answer every iteration problem: sometimes we need positions, several ranges together, or a different traversal strategy.

Later chapters will follow larger changes. Once copying and ownership make sense, we can ask why move semantics is useful. Once templates make sense, we can ask what constraints and concepts contribute. Each question should arise from something concrete we can already understand.

## What this first example leaves open

The program works for the small inputs we chose. It also gives us questions to carry forward:

- What is an object, and how does its type affect its operations?
- What is copied when we pass a collection to a function?
- What happens to a reference when a vector grows?
- How does cleanup happen when a function exits early?
- How do we represent an operation that can fail?
- What happens if an integer calculation exceeds its representable range?

For that last question, signed integer overflow is undefined behaviour in C++. Our example deliberately keeps the total within range. A program accepting arbitrary measurements would need an explicit policy for range and overflow; changing a container does not solve an arithmetic problem.

We will answer these questions in stages. The purpose of the first program is to give the discussion something to refer back to.

## Try changing it

Before continuing, make three small changes:

1. Append another measurement and predict the output before running it.
2. Remove all initial measurements, leaving `std::vector<int> samples{};`, and remove the append. What does the loop do, and what is printed?
3. Change the loop body to `sample = 0;`. After that loop, use a second loop to print the vector's elements. Did they change?

The third experiment is a useful starting point for the chapter on values and references. In the loop we wrote, `sample` is a separate integer initialized from each element. Changing it does not change that element.

In the next chapter, we will follow this same source file through compilation and linking. Before building more of the language, it helps to know what the tools are doing with the program we have already written.

## References and further reading

- [Stroustrup's FAQ](https://www.stroustrup.com/bs_faq.html): first-hand discussion of C++'s goals, learning, and relationship to C.
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines): guidance on interfaces, ownership, resource management, and expressing intent.
- [C++ working draft: vector overview](https://eel.is/c++draft/vector.overview), [vector modifiers](https://eel.is/c++draft/vector.modifiers), and [range-based for](https://eel.is/c++draft/stmt.ranged): specification details behind the container and loop. These links track the evolving working draft rather than a frozen C++11 edition.
