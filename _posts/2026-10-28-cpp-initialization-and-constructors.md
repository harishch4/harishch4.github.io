---
title: "Part 3 : Giving an Object a Valid Beginning"
date: 2026-10-28 00:02:00 +0530
categories: [C++, Understanding C++ as It Evolves]
tags: [cpp, cpp-evolution, constructors, initialization, const]
description: "Why initialization belongs to construction, how member initializers differ from assignment, and what happens when an object cannot be created with a valid state."
series: understanding-cpp-as-it-evolves
series_title: Understanding C++ as It Evolves
part: 3
---

Our `Volume` class starts at zero and accepts updates only within 0–100. Suppose we want a volume that starts at 40.

The existing interface requires two steps:

```cpp
Volume music;
music.set(40);
```

That works for a mutable object. But consider a setting that should remain fixed:

```cpp
const Volume reference;
reference.set(40); // Error: set() cannot be called on a const object.
```

We need to supply the intended value while creating the object. That is the constructor's job.

## Supply the Starting Value

Previously, our constructor was:

```cpp
Volume() : level_(0) {}
```

It can instead accept an argument:

```cpp
explicit Volume(int initial) : level_(initial)
{
    // Validation will follow below.
}
```

Now `Volume music(40);` initializes `level_` from 40. The constructor declaration uses the class name and has no return type.

`explicit` prevents this constructor from being used for implicit conversions such as passing an integer where a `Volume` is expected. Direct initialization, as in `Volume music(40);`, remains allowed. We will study conversions separately; here, we want creating a volume from an integer to be a deliberate choice.

This constructor alone no longer permits `Volume music;`. Declaring a constructor with a required argument does not also give us a compiler-generated no-argument constructor.

We can preserve the zero default with a default argument:

```cpp
explicit Volume(int initial = 0) : level_(initial)
{
    // Validation will follow below.
}
```

Both forms are now available:

```cpp
Volume muted;       // Uses initial = 0.
Volume music(40);   // Uses initial = 40.
```

Despite having a parameter, this counts as a **default constructor** because it can be called without supplying arguments.

## Initialization Happens Before the Body

The colon introduces the **member initializer list**:

```cpp
: level_(initial)
```

This initializes `level_` before the constructor body executes. Compare it with assignment in the body:

```cpp
explicit Volume(int initial)
{
    level_ = initial;
}
```

Here, the omitted integer member receives no initial value before the body; the assignment then gives it a value. For this simple member, either approach can produce the same final result. Other member types make the distinction essential.

For example, a `const` integer member must receive its value during initialization:

```cpp
class Limit {
    const int maximum_;

public:
    explicit Limit(int maximum) : maximum_(maximum) {}
};
```

Assigning `maximum_` inside the body would be too late: it is already a const member. Likewise, a member of a class type is constructed before the enclosing constructor's body begins. Assigning to it afterward does not replace the need to construct it first.

The initializer list describes how the object's parts begin; the body handles any remaining work.

## A Caller Can Also Supply 150

Accepting an argument introduces a route around the range check in `set`:

```cpp
Volume invalid(150);
```

Without validation, our constructor would create the very state the class was meant to prevent.

An invalid update can return `false` and leave the previous value unchanged. Construction has no previous usable object to preserve, and a constructor cannot return a status value. We must choose a policy: reject construction, adjust the input, or represent failure through a different interface.

For this class, we will reject construction by throwing an exception:

```cpp
explicit Volume(int initial = 0) : level_(initial)
{
    if (initial < 0 || initial > 100) {
        throw std::out_of_range("Volume must be between 0 and 100");
    }
}
```

`std::out_of_range`, declared in `<stdexcept>`, is an exception type. Throwing it exits this constructor without successfully creating the complete `Volume`. Normal execution does not continue with an invalid object.

The member may hold the supplied value during construction; the invariant is the condition we establish by successful completion. For every normally constructed `Volume`, that condition now holds.

A caller can handle the failure:

```cpp
try {
    Volume invalid(150);
} catch (const std::out_of_range& error) {
    std::printf("Construction failed: %s\n", error.what());
}
```

The handler receives the exception and prints its message. The reference avoids copying it, and `const` prevents modification through that reference. We will examine exception handling and cleanup in depth later; the important point here is that construction either succeeds with a valid state or reports failure.

## A Const Object Still Has to Be Initialized

With validation in place, we can create the fixed setting directly:

```cpp
const Volume reference(40);
reference.level(); // Allowed: level() is const.
reference.set(50); // Error: set() is not const.
```

Construction can initialize an object that will be const. Once construction completes, its const restrictions apply.

Our observer remains:

```cpp
int level() const
{
    return level_;
}
```

The trailing `const` allows this operation on both const and non-const objects. It also prevents an assignment such as `level_ = 0;` inside this function. It does not make every `Volume` immutable: a non-const object can still call `set`.

## Run the Updated Class

The [complete example]({{ '/assets/code/cpp-through-problems/chapter-03/volume.cpp' | relative_url }}) keeps the existing `set` and `level` operations and replaces the constructor. Build it with:

```bash
g++ -std=c++98 -Wall -Wextra -pedantic volume.cpp -o volume
./volume
```

The output is:

```text
Default: 0
Initial music: 40
Updated music: 50
Const object: 25
Construction failed: Volume must be between 0 and 100
```

All these facilities belong to the C++98 foundation. Next, we will follow an object to the other end of its lifetime: when it is destroyed, and how that gives resource cleanup a dependable place to happen.

## References

- Stroustrup's [style and technique FAQ](https://www.stroustrup.com/bs_faq2.html), including its discussion of constructors and invariants.
- C++ working draft: [constructors](https://eel.is/c++draft/class.ctor), [member initialization](https://eel.is/c++draft/class.base.init), [conversion by constructor](https://eel.is/c++draft/class.conv.ctor), and [construction failure](https://eel.is/c++draft/except.ctor). These links track the evolving specification.

{% include series-nav.html %}
