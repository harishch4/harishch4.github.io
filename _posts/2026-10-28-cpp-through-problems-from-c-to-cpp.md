---
title: "Part 2 : From a C Program to a C++ Interface"
date: 2026-10-28 00:01:00 +0530
categories: [C++, C++ Through Problems]
tags: [cpp, cpp-through-problems, references, function-overloading]
description: "Starting with a C accumulator, then following the need to update an existing object into C++ references and function overloading."
series: cpp-through-problems
series_title: C++ Through Problems
part: 2
---

[Part 1]({% post_url 2026-10-28-cpp-through-problems-why-cpp %}) established the path we want to follow: start from what we know in C, understand the rules around our data, and introduce C++ features when we have a reason to use them.

Before building a class that owns memory, we can begin with something smaller. Suppose measurements arrive one at a time, and we want to keep their running total and count. We do not need to retain all the measurements. Each new value only changes those two numbers.

The representation is straightforward. The more interesting question is how a function should receive it: as an independent value, as an address, or as another way to refer to the existing object.

## Begin with the C Program

Here is the complete starting point:

```c
#include <stdio.h>

struct Summary {
    double total;
    unsigned count;
};

void add(struct Summary *summary, double value)
{
    summary->total += value;
    ++summary->count;
}

int main(void)
{
    struct Summary summary = {0.0, 0};
    double values[] = {12.0, 18.0, 15.0};
    unsigned i;

    for (i = 0; i < 3; ++i) {
        add(&summary, values[i]);
    }

    printf("Count: %u, total: %.1f\n", summary.count, summary.total);
    return 0;
}
```

The output is:

```text
Count: 3, total: 45.0
```

`summary` holds the state accumulated so far. `add` receives its address because it must update that state. Passing the structure by value would give the function a separate copy, leaving the caller's summary unchanged.

There is already a contract here. The pointer passed to `add` must point to a valid `Summary`. A null pointer is not an alternative meaning of this operation. There must also be room in the numeric types for the updated total and count; our small inputs keep the example within those limits.

Nothing in this program requires a C++ feature. That makes it a useful baseline: we can change the interface while keeping the work visible.

## Change the Language Mode Before Changing the Design

You can [download the C source]({{ '/assets/code/cpp-through-problems/chapter-02/summary.c' | relative_url }}). Build and run it as C:

```bash
gcc -std=c17 -Wall -Wextra -pedantic summary.c -o summary-c
./summary-c
```

Now compile the same file as C++:

```bash
g++ -x c++ -std=c++11 -Wall -Wextra -pedantic summary.c -o summary-cpp
./summary-cpp
```

`-x c++` explicitly selects C++ for this source file. We use it because the filename still ends in `.c`; the filename would otherwise suggest C. Once we save C++ source as `.cpp`, we can omit that option.

This particular program is valid in both languages and produces the same output. We have changed which language rules apply, but we have not yet used those rules to change the design.

The distinction matters because other C programs may stop compiling. For example, C permits an implicit conversion from `void*` to an object pointer, while C++ does not:

```c
void *storage = 0;
int *p = storage; /* Valid C; rejected as an implicit conversion in C++. */
```

That is a type-conversion difference, not a reason to replace every C allocation with a cast. Storage and object creation deserve their own discussion once we reach lifetime. For this chapter, our summary needs no dynamic allocation at all.

We are also using the C++ compiler driver for the final build. On a typical GCC installation, `g++` arranges C++ library linkage. The separate-compilation model you know from C still applies: declarations make names available to a translation unit, and definitions must be supplied when the program is linked.

## The Structure Name Can Be Used Directly

In C++, the structure declaration also lets us use `Summary` as a type name without a `typedef`:

```cpp
Summary summary = {0.0, 0};
```

The same applies to the parameter:

```cpp
void add(Summary* summary, double value)
{
    summary->total += value;
    ++summary->count;
}
```

This is a small change. `summary` is still a pointer, and the caller still writes `add(&summary, value)`.

We also still initialize the total and count explicitly. Removing the initializer from an automatic local declaration such as `Summary summary;` would leave these scalar members without usable initial values for our calculation. Compiling as C++ does not automatically initialize every local variable to zero.

Our structure is still a simple aggregate. We have not introduced constructors or hidden any data. Those will come later, when we ask how a type can establish and protect its own rules.

## The Function Needs the Existing Object

Return to the pointer parameter. We chose it because the function must change an object that already exists in the caller.

C++ offers another way to express that relationship:

```cpp
void add(Summary& summary, double value)
{
    summary.total += value;
    ++summary.count;
}
```

The `&` in this declaration makes `summary` an **lvalue reference to `Summary`**. In this call:

```cpp
Summary summary = {0.0, 0};
add(summary, 12.0);
```

the parameter refers to the caller's object. We have not copied the structure. Inside `add`, `summary.total` accesses the total in that same object, so the caller observes the change after the function returns.

The spelling of the call now resembles passing by value. The parameter declaration tells us the difference: `Summary` would receive a value, while `Summary&` binds to an existing object.

A small example makes the relationship easier to see:

```cpp
Summary first = {0.0, 0};
Summary& current = first;

current.total = 12.0;
```

`current` refers to `first`. The assignment changes `first.total`. The reference does not hold a second, independent summary.

There is an important difference from a pointer variable. After binding, a reference cannot be made to refer to a different object. If we write:

```cpp
Summary second = {18.0, 1};
current = second;
```

we assign `second` into `first` through the reference. We do not redirect `current` to `second`.

For our `add` operation, a reference fits the requirement: update the supplied object, with no null case. A valid reference must refer to an appropriate object. This does not make lifetime problems disappear; a reference can dangle if the object it refers to no longer exists. It also does not make pointers obsolete. An interface that needs an optional object, pointer arithmetic, or a pointer that can change its target has different requirements.

## Adding Several Values Exposes Another Interface Choice

So far, each call adds one measurement. Suppose a caller already has an array and wants to add all its values.

We could write a second function named `add_many`. In C++, we can also provide another function named `add`, with different parameters:

```cpp
void add(Summary& summary, const double* values, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i) {
        add(summary, values[i]);
    }
}
```

The single-value operation must already have been declared when this definition is compiled. The loop reuses it, so both entry points perform the same state update.

Now the caller can write:

```cpp
add(summary, 12.0);          // Add one measurement.
add(summary, values, count); // Add a sequence of measurements.
```

This is **function overloading**. The compiler selects a suitable overload from the declarations visible at the call, using the arguments. These calls are particularly simple to distinguish because one has two arguments and the other has three. There is no runtime test choosing between them.

The names describe the same action at different scales, which makes overloading reasonable here. It is also perfectly possible to keep separate names. The feature gives us an interface choice; it does not require every related operation to share a name.

Notice what the second overload has not solved. The pointer and element count must still describe a valid range whenever the count is nonzero. The compiler does not infer the array's length from this pointer parameter. Later, we will examine interfaces that carry more of that information together.

## The C++ Program We Ended Up With

Here is the complete version, also available as [summary.cpp]({{ '/assets/code/cpp-through-problems/chapter-02/summary.cpp' | relative_url }}):

```cpp
#include <cstddef>
#include <cstdio>

struct Summary {
    double total;
    unsigned count;
};

void add(Summary& summary, double value)
{
    summary.total += value;
    ++summary.count;
}

void add(Summary& summary, const double* values, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i) {
        add(summary, values[i]);
    }
}

int main()
{
    Summary summary = {0.0, 0};
    double values[] = {12.0, 18.0, 15.0};

    add(summary, values, sizeof values / sizeof values[0]);

    std::printf("Count: %u, total: %.1f\n", summary.count, summary.total);
    return 0;
}
```

`<cstddef>` supplies `std::size_t`, and `<cstdio>` supplies the C-style output facility used as `std::printf`. Keeping familiar output lets us concentrate on the interface changes. There is no need to introduce streams to use references or overloads.

Build and run it with:

```bash
g++ -std=c++11 -Wall -Wextra -pedantic summary.cpp -o summary-cpp
./summary-cpp
```

The output remains `Count: 3, total: 45.0`. All C++ features used in this final program were already available in C++98; the command selects C++11 as our explicit working mode. We will identify later additions when we use them.

We began with a function receiving the address of a C structure. The need to update the existing object led us to a reference parameter. The need to support both individual values and a sequence led us to an overload. Neither change required a class hierarchy, a template, or dynamic allocation.

There is still a rule left outside the interface, though: the caller must remember to give `Summary` a suitable initial state. Its two fields are publicly writable, and anyone can change them independently.

That gives us the next question. How should an object begin its life, and how much of that responsibility should belong to its type? Part 3 will start with initialization and use that question to lead us toward constructors.

## Further Reading

- [Stroustrup's C++ style and technique FAQ](https://www.stroustrup.com/bs_faq2.html), for discussion of references and their relationship to pointers.
- The C++ working draft: [reference initialization](https://eel.is/c++draft/dcl.init.ref), [overload resolution](https://eel.is/c++draft/over.match), and [pointer conversions](https://eel.is/c++draft/conv.ptr). These links track the evolving draft rather than a frozen C++98 or C++11 edition.

{% include series-nav.html %}
