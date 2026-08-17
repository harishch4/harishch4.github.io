---
title: "Part 2 : Pipeline Dependencies, Forwarding, and Instruction-Level Parallelism"
date: 2026-08-17 12:00:00 +0530
categories: [Computer Architecture, Instruction Scheduling]
tags: [risc-v, rocket, pipeline-hazards, forwarding, scoreboard, ilp]
description: "Following values through an in-order RISC-V pipeline to understand forwarding, stalls, scoreboards, and where instruction-level parallelism comes from."
---

[Part 1]({% post_url 2026-08-17-instruction-pipelining-in-an-in-order-risc-v-processor %}) ended with a question that the ideal pipeline table could not answer. When an instruction reaches the execute stage, how does the processor know that the values it needs are actually ready?

The question came from this sequence:

```asm
lw   t0, 0(a0)
lw   t1, 0(a1)
mul  t0, t0, t1
add  a2, a2, t0
```

The `mul` needs the results of both loads. The `add` needs the result of the multiplication. Their order expresses the calculation correctly, but program order alone does not tell us how quickly the instructions can move through the pipeline.

To find that out, we need to follow one result from the stage that produces it to the stage where the next instruction needs it.

## A Result Becomes Ready at a Particular Time

Start with a simpler pair of instructions:

```asm
add  t0, t1, t2
sub  t3, t0, t4
```

The `add` produces a new value for `t0`. The `sub` uses that value as one of its inputs. The second instruction therefore cannot perform the subtraction until the first has completed the addition.

In the five-stage pipeline from Part 1, the `add` performs its arithmetic in `EX`. Its result exists at the end of that stage, but it is not written into the register file until the later `WB` stage.

The `sub` reaches `EX` one cycle after the `add`. It needs the new value of `t0` at the beginning of that cycle. If it reads only from the register file, the value will appear to be unavailable: the `add` has calculated it, but has not yet reached `WB`.

Waiting for write-back would be correct, but it would also waste time. The required value is already inside the processor. It simply has not travelled through the normal register-file path yet.

## Forwarding Takes the Shorter Path

The processor can add a path from the output of one pipeline stage back to the input of the execution unit. The result produced by the `add` at the end of one cycle can then become an input to the `sub` in the next cycle.

This is called **forwarding**, or **bypassing**. The value bypasses the register file and travels directly from the older instruction to the instruction that needs it.

Forwarding does not remove the relationship between the instructions. The `sub` still has to execute after the `add` has produced `t0`. What forwarding removes is the unnecessary wait for `t0` to reach `WB`.

The timing works because the two events occur on opposite sides of a clock boundary. The `add` produces its result at the end of its `EX` cycle. The `sub` needs the result at the beginning of its `EX` cycle one clock later. The value exists in time to cross a forwarding path between them.

That gives us the first rule:

> A result can be forwarded only after it exists.

The rule sounds obvious, but it explains why the same solution does not completely remove the delay after a load.

## A Load Produces Its Value One Stage Later

Now return to the second load and the multiplication:

```asm
lw   t1, 0(a1)
mul  t0, t0, t1
```

The load uses `EX` to calculate its memory address. The value stored at that address is obtained in `MEM`, one stage later. Under the simple timing model used here, the loaded value becomes available at the end of the memory stage.

If both instructions advance every cycle, the `mul` would enter `EX` during the same cycle in which the load is in `MEM`. That is too early. The multiplication needs `t1` at the beginning of the cycle, while memory produces it at the end.

The pipeline has to hold the multiplication for one cycle:

| Instruction | Cycle 1 | Cycle 2 | Cycle 3 | Cycle 4 | Cycle 5 | Cycle 6 | Cycle 7 |
|---|---|---|---|---|---|---|---|
| `lw t1, 0(a1)` | IF | ID | EX | MEM | WB |  |  |
| `mul t0, t0, t1` |  | IF | ID | ID (held) | EX | MEM | WB |

At the end of cycle 4, the load has produced `t1`. In cycle 5, that value can be forwarded into the multiplication's `EX` stage. Forwarding still helps because the multiplication does not have to wait for a later register-file read, but it cannot eliminate the one cycle in which the value did not yet exist.

This is the familiar **load-use delay**. The example assumes that the memory access completes in one `MEM` cycle. If the data takes longer to return, the instruction waiting for it must remain blocked for longer as well.

## A Stall Holds an Instruction; a Bubble Leaves a Stage Empty

During cycle 4, the `mul` cannot leave `ID`, but the older load should continue into `MEM`. The processor therefore does not freeze the entire pipeline.

It holds the pipeline state containing the multiplication so the same instruction can be checked again in the next cycle. The instruction behind the multiplication is also prevented from moving past it. Meanwhile, older instructions continue toward completion.

Because the multiplication does not enter `EX` in cycle 4, that stage receives no useful instruction. The empty slot then moves through the later stages just as an instruction would.

The two words describe different parts of the same event. A **stall** is the decision to hold an instruction in place. A **bubble** is the empty pipeline slot created because that instruction did not advance.

The processor has preserved correctness, but one cycle of execution capacity has gone unused.

## How the Processor Detects the Problem

By the time an instruction reaches `ID`, the processor knows which registers it reads and which register it will write. It also retains similar information for older instructions already in later stages.

For the multiplication, the source registers are `t0` and `t1`. For the load ahead of it, the destination register is `t1`. The matching register number tells the processor that the younger instruction wants a value being produced by the older one.

This is a **read-after-write dependency**: the `mul` reads `t1` after the load writes it. The dependency is part of the program and exists regardless of the processor. It becomes a **pipeline hazard** only when the overlapping stage timing could cause the multiplication to use the old value.

The hardware check therefore needs more than a register-number match. It has to answer three connected questions. Does an older instruction write one of the current instruction's source registers? If so, has that older instruction produced the value yet? If the value exists, is there a path that can deliver it to the stage that needs it?

For the earlier `add` followed by `sub`, the answers lead to forwarding. For the load followed immediately by `mul`, the result is not ready soon enough, so the answers lead to a stall. The same dependency check produces different actions because the two producers make their results available at different stages.

In a short fixed pipeline, these checks can be made by comparing the source registers in `ID` with the destination registers of instructions in `EX`, `MEM`, and `WB`. That is enough while every unfinished producer remains visible in one of those stages.

It becomes less convenient when an operation takes longer and no longer follows the ordinary one-stage-per-cycle path.

## Remembering a Result That Is Still Pending

Suppose the multiplication unit needs several cycles to produce `t0`. An implementation may allow the multiplication to continue in that unit while the front of the pipeline handles later instructions. Eventually this instruction reaches `ID`:

```asm
add  a2, a2, t0
```

The multiplication may no longer be represented by an instruction sitting in the ordinary `EX`, `MEM`, or `WB` position, but its result is still unfinished. Comparing the `add` only with those visible pipeline stages could miss the dependency.

The processor now needs a small piece of state that survives for as long as the operation remains unfinished. A **scoreboard** provides that state.

For this in-order example, the essential information can be as small as a busy indication for each register:

| Register | State | Meaning |
|---|---|---|
| `t0` | Busy | An older instruction has promised a new value, but it is not ready |
| `t1` | Ready | No unfinished instruction is expected to replace the current value |

When the multiplication is accepted, the scoreboard marks `t0` busy. When the later `add` checks its source registers, it sees that `t0` is still pending and remains in place. When the multiplication completes, the busy state is cleared and the `add` can continue.

The scoreboard does not contain the result itself, and it does not calculate when the multiplication will finish. It remembers that the current value of a register must not be consumed yet.

Not every five-stage dependency needs a scoreboard. Nearby fixed-latency hazards can be handled directly by comparing the instructions in the pipeline stages. The scoreboard becomes useful when the producer can remain unfinished after it is no longer easy to identify from those stage positions. More elaborate processors may record more information, but register readiness is the part we need for this story.

## Correct Waiting Is Not the Same as Useful Work

Forwarding and the scoreboard answer important hardware questions. Forwarding finds the shortest available path for a value. The scoreboard remembers when a value is still pending. The stall prevents an instruction from using the wrong value.

None of them finds another instruction to execute instead.

That limitation matters in an in-order processor. Consider this order from a loop:

```asm
lw    t0, 0(a0)
lw    t1, 0(a1)
mul   t0, t0, t1
addi  a0, a0, 4
```

The pointer update does not need either loaded value, but it appears after the multiplication. If the multiplication is held in `ID` while waiting for `t1`, the `addi` remains behind it. The hardware preserves the given order rather than searching past the blocked instruction.

The same operations can be arranged like this:

```asm
lw    t0, 0(a0)
lw    t1, 0(a1)
addi  a0, a0, 4
mul   t0, t0, t1
```

The first load has already used the old value of `a0`, so updating the pointer afterward does not change the value that was loaded. The `addi` also does not need `t0` or `t1`. It can use the execution stage during the cycle in which an immediately following `mul` would have been forced to wait for the second load.

By the time the multiplication reaches `EX`, the loaded value is available. The required dependency has not changed, and the load has not become faster. An independent instruction has simply occupied a cycle that would otherwise have contained a bubble.

This is **instruction-level parallelism**, usually shortened to **ILP**: independent work within one instruction stream that can overlap in the processor. In a single-issue pipeline, ILP does not mean starting several instructions in the same cycle. It means keeping the stages useful by placing independent instructions between operations that must wait for one another.

The example also shows why instruction order matters. The independent work existed in both versions, but the in-order hardware could use it only when it appeared before the blocked consumer.

## The Information Needed to Choose a Better Order

We can now state the scheduling problem without treating it as a compiler trick. The processor has a time at which each instruction produces a result, a stage at which another instruction needs that result, and a set of paths through which the result may be delivered. If the value cannot arrive in time, the pipeline waits. If an independent instruction can legally occupy that position, the wait may be hidden.

Choosing that instruction requires two kinds of knowledge. The calculation tells us which instructions depend on one another. The processor tells us how long their results take and which parts of the hardware they use.

An in-order core reacts to the sequence it receives. A compiler has the earlier opportunity to arrange that sequence while preserving the calculation. Before it can do that well, it needs a description of the target processor's timing and execution resources.

That is where Part 3 begins.
