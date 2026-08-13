---
layout: post
title: "How FFT Made Me See Multiplication Differently"
date: 2026-08-13 12:00:00 +0530
categories: [Algorithms, Mathematics]
tags: [fft, multiplication, convolution, algorithms]
description: "A personal note on the beautiful connection between digits, polynomials, convolution, and FFT-based multiplication."
math: true
---

I was reading about large-integer multiplication when I came across a sentence that made me stop: *we can multiply numbers using the FFT*.

I reread it. FFT? The same idea used to move between signals and frequencies? What could that possibly have to do with the multiplication we learned in school?

The connection begins with something almost embarrassingly simple. Take the number 123:

$$
123=1\times10^2+2\times10+3.
$$

Now replace 10 with $x$:

$$
x^2+2x+3.
$$

Nothing profound seems to have happened. We have merely written the digits as the coefficients of a polynomial. But this small change opens the entire door.

## What Long Multiplication Was Hiding

Write 45 in the same way:

$$
45=4\times10+5
\quad\longrightarrow\quad
4x+5.
$$

Now multiply the two polynomials:

$$
(x^2+2x+3)(4x+5).
$$

Expanding it gives

$$
4x^3+13x^2+22x+15.
$$

If we read the coefficients from the lowest position upward, we get

$$
[15,22,13,4].
$$

At first, this does not look like the answer to $123\times45$. The values are not even valid decimal digits. But then we perform the carries.

From 15, keep 5 and carry 1. Adding that 1 to 22 gives 23, so keep 3 and carry 2. Adding 2 to 13 gives 15, so keep 5 and carry 1. Finally, $4+1=5$.

The result is

$$
5535.
$$

That was the first moment the connection felt real to me. The polynomial multiplication had already produced the answer. Carrying was the cleanup needed to turn its coefficients back into decimal digits.

Looking again at the raw coefficients shows exactly what our school method was doing:

$$
15=3\times5,
$$

$$
22=3\times4+2\times5,
$$

$$
13=2\times4+1\times5,
$$

$$
4=1\times4.
$$

Every digit of one number meets every digit of the other. Products that belong to the same decimal position are added together.

This pattern has a name: **convolution**.

I had previously associated convolution with signal processing and neural networks. It sounded like a separate, more advanced operation. But it turns out I had been performing a convolution whenever I did long multiplication on paper. The familiar diagonal sums were it.

For two numbers with $n$ digits, directly forming all these pairs takes work that grows roughly like $n^2$. For small numbers this is irrelevant. For numbers containing millions of digits, it is the part we would like to avoid.

And this is exactly the part that changes when we stop describing the polynomials by their coefficients.

## The Same Polynomial, Seen Another Way

A polynomial can be described by listing its coefficients:

$$
A(x)=x^2+2x+3
\quad\longleftrightarrow\quad
[3,2,1].
$$

But coefficients are not the only description available. We can also choose values of $x$ and record what the polynomial becomes there:

$$
A(0)=3,
$$

$$
A(1)=6,
$$

$$
A(-1)=2.
$$

With enough such values, we can reconstruct the original polynomial. We have not changed the polynomial; we have changed what information we use to describe it.

This matters because polynomial multiplication behaves very differently in this second description.

Suppose

$$
C(x)=A(x)B(x).
$$

At any point $r$,

$$
C(r)=A(r)B(r).
$$

That small equation is the heart of the trick.

In the coefficient view, finding $C$ means combining every coefficient of $A$ with every coefficient of $B$. In the point-value view, we simply multiply the two values sitting at the same point.

If we know $A$ and $B$ at enough points, we can multiply their matching values and then reconstruct $C$.

For a moment, it seems as if we have escaped the convolution entirely. Of course, there is a catch: evaluating a large polynomial at many ordinary points, and later reconstructing it, could cost as much as the original multiplication.

So the idea only becomes useful if we can move between these two descriptions quickly.

That is where the FFT enters—not as an unrelated trick, but as the missing doorway.

## Why the FFT Fits So Perfectly

The FFT moves between coefficients and values at a very special collection of points called **roots of unity**.

I picture them as points spaced evenly around a circle. For four points they are $1$, $i$, $-1$, and $-i$. For eight, there are eight evenly spaced points; for sixteen, sixteen.

The exact mathematics of these points is a subject of its own. What matters here is their symmetry. Calculations performed at one point are related to calculations at others. The FFT repeatedly reuses those relationships instead of evaluating the polynomial independently at every point.

That structure reduces a task that appears to need roughly $n^2$ work to one that needs roughly $n\log n$ work.

Now the entire journey is visible:

> Start with digits. Treat them as polynomial coefficients. Use the FFT to obtain the values of those polynomials at special points. Multiply matching values. Use the inverse FFT to recover the coefficients of the product. Then perform the carries.

The only multiplication in the transformed representation is point by point:

$$
A(r_0)B(r_0),\quad
A(r_1)B(r_1),\quad
A(r_2)B(r_2),\quad\ldots
$$

All the cross-connections between digits that made convolution expensive have been absorbed into the change of representation.

This is often described as moving into the frequency domain. That phrase is correct, but it initially hid the idea from me because I immediately thought about audio frequencies. For multiplication, the picture that helped me was simpler:

> The FFT changes the polynomial from a form where multiplication is hard into a form where multiplication is easy.

The inverse FFT then brings the answer home.

## Leaving the Number and Finding It Again

There is one practical detail in our small example. To multiply numbers using an FFT, we first leave enough empty coefficient positions for the result. Otherwise, the circular nature of the transform can make high positions wrap around into low ones. This is handled by adding zeros before the transform.

After the inverse FFT, we recover values very close to the raw coefficients:

$$
[15,22,13,4].
$$

With the usual FFT, tiny rounding errors can appear because the calculation uses approximate numbers. An implementation rounds the recovered coefficients carefully and then performs the carries.

For `123 × 45`, this route would be absurdly excessive. Ordinary multiplication wins immediately. Transform-based multiplication becomes valuable only for numbers so large that the slower growth in work outweighs the cost of entering and leaving the transformed representation.

Real large-number libraries therefore use different algorithms at different sizes. They may begin with ordinary multiplication, move to algorithms such as Karatsuba, and use transform-based methods only when the inputs become large enough.

Those details matter if I want to implement the algorithm. They are not what stayed with me after learning it.

What stayed with me was the round trip.

We begin with an integer, a thing so familiar that its representation feels inseparable from the number itself. We notice that its digits are polynomial coefficients. We leave that coefficient world and visit a set of carefully chosen points where the troublesome convolution becomes pairwise multiplication. Then we reverse the journey, carry the coefficients, and find the original number system waiting for us.

Nothing about the answer changed. Only the viewpoint did.

## The Part I Cannot Stop Thinking About

I expected a fast multiplication algorithm to be a more ingenious version of long multiplication—perhaps a way to skip some digit products or handle carries more cleverly.

Instead, the key idea is to stop doing multiplication in the representation where it is difficult.

There is something deeply satisfying about that. The algorithm does not fight the $n^2$ pattern directly. It recognizes that the pattern is convolution, finds another world where convolution becomes ordinary multiplication, does the easy operation there, and comes back.

The symbols line up almost too neatly:

$$
\text{digits}
\longrightarrow
\text{polynomial coefficients}
\longrightarrow
\text{point values}
\longrightarrow
\text{pairwise products}
\longrightarrow
\text{coefficients}
\longrightarrow
\text{digits}.
$$

The more I look at it, the less it feels like a trick. It feels like the problem was waiting to be asked in the right language.

That is what I find beautiful about FFT multiplication. It is a reminder that a hard operation may be hard only in the form in which we first encountered it.

## Further Reading

- James W. Cooley and John W. Tukey, [*An Algorithm for the Machine Calculation of Complex Fourier Series*](https://doi.org/10.1090/S0025-5718-1965-0178586-1)
- David Harvey and Joris van der Hoeven, [*Integer Multiplication in Time $O(n\log n)$*](https://annals.math.princeton.edu/2021/193-2/p04)
