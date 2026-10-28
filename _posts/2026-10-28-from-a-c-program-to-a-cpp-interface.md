---
title: "Part 2 : Why Put Data and Functions in a Class?"
date: 2026-10-28 00:01:00 +0530
categories: [C++, Understanding C++ as It Evolves]
tags: [cpp, cpp-evolution, classes, encapsulation]
description: "A value with a simple rule shows what classes add: an interface that controls how an object's state can change."
series: understanding-cpp-as-it-evolves
series_title: Understanding C++ as It Evolves
part: 2
---

Classes helped C++ express the organization of a program. But putting data and functions inside the same pair of braces does not, by itself, explain their value.

Consider a volume setting. Its representation is an integer; its meaning is more specific: a level between 0 and 100.

## An Integer Does Not Know the Rule

We can group the value in a structure:

```c
struct Volume {
    int level;
};
```

This gives the setting a name, but the representation still allows values that do not make sense:

```c
struct Volume volume = {50};
volume.level = 150;
```

The assignment is valid code. The compiler sees an integer receiving another integer. The application's rule exists only in our understanding of what `level` means.

A function can check that rule:

```c
int set_volume(struct Volume *volume, int value)
{
    if (value < 0 || value > 100) {
        return 0;
    }
    volume->level = value;
    return 1;
}
```

Now callers have a checked operation. Yet they can still bypass it with `volume.level = 150`. The function preserves the rule only when everyone agrees to use it.

C can enforce an interface boundary using an opaque structure and functions. C++ classes provide language-level access control within the type definition itself.

## Control the Ways the Value Can Change

Here is a complete C++ version:

```cpp
#include <cstdio>

class Volume {
private:
    int level_;

public:
    Volume() : level_(0) {}

    bool set(int value)
    {
        if (value < 0 || value > 100) {
            return false;
        }
        level_ = value;
        return true;
    }

    int level() const
    {
        return level_;
    }
};

int main()
{
    Volume volume;
    std::printf("Initial: %d\n", volume.level());

    volume.set(50);
    std::printf("After setting 50: %d\n", volume.level());

    if (!volume.set(150)) {
        std::printf("Rejected 150; still: %d\n", volume.level());
    }
}
```

`level_` is a **data member**. The trailing underscore is a naming convention, with no special language meaning.

`private:` makes that member inaccessible to ordinary code outside the class. The operations following `public:` form the interface callers can use. Both access labels apply until another label changes the access.

This means:

```cpp
volume.set(50);    // Allowed: call the public operation.
volume.level_ = 150; // Rejected: the data member is private.
```

The second line is a compile-time error. The compiler does not understand the range 0–100; it prevents outside code from bypassing the operations where we enforce it.

## The Operation Belongs to an Object

`set` is a **member function**. When we call `volume.set(50)`, it operates on `volume`. Its use of `level_` refers to the data member in that object.

Two objects have separate state:

```cpp
Volume left;
Volume right;
left.set(25);
right.set(75);
```

The function's implementation is shared; the data belongs to each object. We do not store a separate copy of the function in every `Volume`.

The `level()` function lets callers read the setting without exposing writable access to its representation. The `const` after its parameter list means it cannot modify ordinary data members through the object on which it is called.

That distinction matters: callers need to observe the level, but observing it does not require permission to overwrite it directly.

## The Object Also Needs a Valid Beginning

Checking updates is insufficient if the initial value is undefined. This line supplies the starting state:

```cpp
Volume() : level_(0) {}
```

It defines a **constructor**, a special member function used when an object is initialized. The part after the colon initializes `level_` to zero before the constructor body runs. The body is empty because no further work is needed.

Consequently, `Volume volume;` creates an object whose level is already valid. There is no separate initialization call for the caller to remember.

We now have a rule that holds after construction and after each public operation: the stored level is between 0 and 100. Such a rule is called a **class invariant**. The constructor establishes it; `set` preserves it, leaving the previous value unchanged when an input is rejected.

Private data alone would not achieve this. A public function that wrote an unchecked value could still break the rule. Access control narrows the places where correctness must be maintained.

## What the Class Adds

The integer represents storage. `Volume` represents a value with a restricted range and operations that respect it.

We have kept the representation small while expressing more about its use. Ordinary non-virtual member functions and access control do not require a runtime permissions check or a function pointer in each object.

C++ also allows member functions and access labels in a `struct`. The relevant defaults differ: members are public by default in a `struct` and private by default in a `class`. Using the keyword `class` here communicates our intent, but the boundary comes from the access rules and interface we defined.

## Run It

Download [volume.cpp]({{ '/assets/code/cpp-through-problems/chapter-02/volume.cpp' | relative_url }}) and build it with:

```bash
g++ -std=c++98 -Wall -Wextra -pedantic volume.cpp -o volume
./volume
```

The output is:

```text
Initial: 0
After setting 50: 50
Rejected 150; still: 50
```

Everything used here belongs to the C++98 foundation. We will examine construction more closely next: how initialization differs from assignment, and how to create an object that needs a caller-supplied starting value.

## References

- Stroustrup's FAQ: [What's so great about classes?](https://www.stroustrup.com/bs_faq.html#class).
- C++ working draft: [member access control](https://eel.is/c++draft/class.access), [non-static member functions](https://eel.is/c++draft/class.mfct.non.static), and [initializing bases and members](https://eel.is/c++draft/class.base.init). These links track the evolving specification.

{% include series-nav.html %}
