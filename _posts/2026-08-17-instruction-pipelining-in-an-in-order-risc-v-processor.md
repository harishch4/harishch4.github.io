---
title: "Part 1 : Instruction Pipelining in an In-Order RISC-V Processor"
date: 2026-08-17 12:00:00 +0530
categories: [Computer Architecture, Instruction Scheduling]
tags: [risc-v, rocket, pipelining, computer-architecture, instruction-scheduling]
description: "Starting from the hardware problem that makes instruction pipelining necessary, then following a concrete RISC-V instruction sequence to the first limit on overlap."
---

I started this series while trying to understand instruction scheduling. I knew the short definition: the compiler can change the order of instructions to help a processor execute them more efficiently. But that definition left out the part I actually wanted to understand. Why should one order be better than another when the instructions still perform the same work?

Looking only at the assembly was not enough. To understand why an instruction order matters, I first needed to know what the processor is doing each cycle. What work can overlap? What forces the processor to wait? If a cycle would otherwise be wasted, what kind of instruction could use it?

So this series starts below the compiler, with the hardware. This first part builds a concrete picture of instruction pipelining in a simple in-order RISC-V processor. The next part will follow values moving between instructions and explain why the regular flow sometimes has to pause. Once that hardware behaviour is clear, we can look at how a compiler represents it, how it chooses an instruction order, and how we can examine the result using a small RISC-V example.

The place to begin is the problem that made pipelining useful in the first place.

## The Problem: One Instruction Uses the Core in Pieces

Consider a single RISC-V addition:

```asm
add a3, a4, a5
```

Its visible meaning is straightforward:

```text
a3 = a4 + a5
```

The processor, however, cannot perform that instruction as one indivisible action. It has to find the instruction in memory, determine that it is an `add`, obtain the values in `a4` and `a5`, perform the addition, and record the result in `a3`.

Different parts of the processor do those jobs. The hardware that fetches an instruction is not the same hardware that performs an integer addition. If the processor waits for the `add` to finish completely before it even starts fetching the next instruction, most of those parts are idle for much of the time. The fetch hardware waits while the addition is performed, and the arithmetic hardware waits while the next instruction is being fetched and decoded.

The problem is not that the processor lacks work. The next instruction may already be waiting in the program. The problem is that the work has been arranged as a strict sequence: finish every step of one instruction, then begin every step of the next.

Instruction pipelining changes that arrangement. Instead of treating an instruction as one large operation, the processor divides its execution into stages. Once one instruction leaves a stage, the following instruction can enter it. Several instructions can then be active at the same time, each using a different part of the core.

## Choosing a Concrete Pipeline

RISC-V itself does not define these stages. It is an instruction set architecture: it defines the instructions software can use and the results those instructions must produce. It does not require processors to implement those instructions with a particular number of pipeline stages.

That is why two processors can run the same RISC-V program while having very different internal designs. One may use a short pipeline; another may use a deeper one. One may allow only a single instruction to begin each cycle; another may begin several.

To keep the discussion concrete, I will use a five-stage, single-issue, in-order pipeline as the working model. [Rocket](https://chipyard.readthedocs.io/en/stable/Generators/Rocket.html) is a real example of a five-stage, in-order RISC-V core. Exact details vary between implementations, but this model is detailed enough to expose the problem instruction scheduling is trying to solve.

The five stages are:

| Stage | Name | What happens |
|---|---|---|
| `IF` | Instruction fetch | Read the instruction selected by the program counter |
| `ID` | Instruction decode | Determine the operation and read its register operands |
| `EX` | Execute | Perform arithmetic or calculate a memory address |
| `MEM` | Memory access | Read or write data memory when the instruction requires it |
| `WB` | Write-back | Record the result in the destination register |

For the earlier `add`, the actual addition happens in `EX`. It does not need to access data memory, so its `MEM` stage simply carries the result forward. The result is then written to `a3` in `WB`.

At the end of a clock cycle, the instruction and its intermediate values can move to the next stage. During the following cycle, each stage works on whichever instruction currently occupies it. This is what creates the opportunity for overlap.

## How Several Instructions Overlap

To see the overlap without introducing any other problem yet, start with four independent instructions:

```asm
add  t0, a0, a1
sub  t1, a2, a3
xor  t2, a4, a5
or   t3, a6, a7
```

None of these instructions needs a result produced by another instruction in the sequence. In the simplified five-stage model, their movement through the pipeline looks like this:

| Instruction | Cycle 1 | Cycle 2 | Cycle 3 | Cycle 4 | Cycle 5 | Cycle 6 | Cycle 7 | Cycle 8 |
|---|---|---|---|---|---|---|---|---|
| `add t0, a0, a1` | IF | ID | EX | MEM | WB |  |  |  |
| `sub t1, a2, a3` |  | IF | ID | EX | MEM | WB |  |  |
| `xor t2, a4, a5` |  |  | IF | ID | EX | MEM | WB |  |
| `or t3, a6, a7` |  |  |  | IF | ID | EX | MEM | WB |

The first instruction still takes five cycles to travel from `IF` to `WB`. Pipelining has not reduced the amount of time that instruction spends in the processor. The improvement comes from allowing the second instruction to start in cycle 2 instead of waiting until cycle 6.

By cycle 5, the first instruction is writing its result, the second is passing through the memory stage, the third is executing, and the fourth is being decoded. The fetch stage is free to start a fifth instruction. The processor can use every stage during the same cycle, with each stage working on a different instruction.

If each stage takes one cycle and the processor finishes all five stages of one instruction before starting the next, these four instructions require twenty cycles. With the ideal pipeline above, they complete in eight. For `n` instructions moving through an ideal `k`-stage pipeline, the total number of cycles is:

```text
k + n - 1
```

The first instruction needs `k` cycles to emerge. Each additional instruction finishes one cycle later than the previous one.

This gives us two different ways to talk about performance. **Latency** is how long one instruction takes to travel through the pipeline. **Throughput** is how frequently instructions complete once the pipeline is full. In this example, an instruction has a latency of five cycles, while the pipeline can approach a throughput of one completed instruction per cycle.

The processor is also **single-issue**: it begins at most one new instruction in a cycle. Single-issue does not mean only one instruction can be inside the processor. The table has several instructions in progress, but only one new instruction enters `IF` each cycle.

Finally, the processor is **in-order**. Instructions enter the pipeline in program order, and the core does not simply move a later instruction ahead whenever an earlier one cannot continue. That restriction will matter once the instructions stop being independent.

## When One Instruction Needs Another's Result

The independent instructions show what the pipeline can achieve under ideal conditions. The code that first made instruction scheduling interesting to me did not look like that. It looked more like the central calculation in a dot product:

```c
sum += a[i] * b[i];
```

A simplified RISC-V sequence for that calculation is:

```asm
lw   t0, 0(a0)
lw   t1, 0(a1)
mul  t0, t0, t1
add  a2, a2, t0
```

The first two instructions load one value from each array. The `mul` multiplies those values, and the `add` accumulates the product into `a2`. Address updates and loop control are omitted for now so that we can follow the values involved in the calculation.

Unlike the earlier sequence, these instructions are connected. The `mul` needs the values produced by both loads. The `add` then needs the value produced by the multiplication.

| Instruction producing a value | Value | Instruction that needs it |
|---|---|---|
| `lw t0, 0(a0)` | First array value in `t0` | `mul t0, t0, t1` |
| `lw t1, 0(a1)` | Second array value in `t1` | `mul t0, t0, t1` |
| `mul t0, t0, t1` | Product in `t0` | `add a2, a2, t0` |

This relationship is a **data dependency**. An instruction that needs a value cannot use it before the earlier instruction has produced it. That is part of the program's meaning, not an optional performance rule.

The ideal pipeline table no longer tells us enough. It shows when an instruction would like to enter each stage, but it says nothing about when a particular result becomes usable. Before placing the `mul` in its execute stage, we have to know whether both loaded values are ready. Before placing the `add` there, we have to know whether the multiplication has finished producing `t0`.

Now the original scheduling question has a concrete shape. Pipelining creates an opportunity to overlap different parts of several instructions, but a dependency can limit that overlap. If the current instruction cannot continue, an in-order processor has to decide what can move and what must wait.

## The Question Left for Part 2

Part 1 gives us the baseline. A pipeline improves throughput by allowing different instructions to use different stages during the same cycle. That works cleanly for independent instructions because each one already has the values it needs.

The dot-product sequence breaks that assumption. Its instructions have to exchange results while they are moving through the pipeline. To understand the actual timing, we now need to follow one value cycle by cycle: where it is produced, when the next instruction needs it, and how the processor knows whether that instruction can continue.

That is the next part of the story.
