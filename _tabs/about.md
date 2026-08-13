---
layout: page
title: About
icon: fas fa-info-circle
order: 4
---

Hi, I’m Harish.

I work in the space where software meets hardware—compilers, runtimes, accelerator software, and, more recently, AI inference.

Over the years, I have worked with LLVM, MLIR, OpenCL, RISC-V, C, C++, and different parts of the software stack needed to make computation run on specialized hardware. But what keeps me interested is not any single tool or framework. It is the chance to follow a computation all the way through the system.

A model may begin as equations or a graph. It then becomes compiler IR, gets lowered into instructions and runtime operations, and eventually has to execute on real hardware with limited memory, bandwidth, and compute. I enjoy understanding how these layers fit together—and finding the places where an assumption made in one layer becomes a problem in another.

A lot of my learning begins with questions that sound simple: What exactly is a cache way? Why do some MoE experts receive more tokens than others? How can an FFT be used to multiply integers?

The answers are usually more interesting than the definitions. Following them often means returning to fundamentals, correcting an incomplete mental model, reading source code, building small examples, or tracing a problem across several layers of the system.

That is also why I started this site.

These posts are notes I want to keep for myself and share with anyone asking similar questions. Some come from implementation and debugging. Others begin with a paper, a piece of source code, or an idea I found unexpectedly beautiful. I try to write them in the order the idea made sense to me—starting with the confusion, following the connections, and ending with the picture I want to remember.

I am still learning, and this site is part of that process. If you are interested in compilers, computer architecture, heterogeneous systems, or AI inference, you will probably find something familiar here.

Thanks for stopping by.
