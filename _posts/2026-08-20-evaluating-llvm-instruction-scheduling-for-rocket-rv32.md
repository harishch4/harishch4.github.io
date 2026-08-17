---
title: "Part 5 : Evaluating LLVM Instruction Scheduling for Rocket RV32"
date: 2026-08-17 12:00:00 +0530
categories: [Compilers, Instruction Scheduling]
tags: [llvm, llvm-mca, risc-v, rocket, machine-ir, instruction-scheduling]
description: "Following an RV32 dot product through LLVM's machine scheduler, then reading the real llvm-mca output for the instruction order before and after scheduling."
---

[Part 4]({% post_url 2026-08-19-how-llvm-schedules-risc-v-machine-instructions %}) described LLVM's scheduling process: machine instructions become nodes in a dependency graph, legal candidates become ready, and the scheduler chooses among them using information about latency and processor resources.

I understood the idea, but I still wanted to see it happen. I wanted to begin with a small C function, stop LLVM immediately before and after the machine scheduler, and identify the instructions that actually moved. Then I wanted to give both orders to `llvm-mca` and read its output rather than ending with “the scheduled version should be better.”

That is the experiment in this part. Everything below uses LLVM 18.1.0 and LLVM's `rocket-rv32` processor model.

## The Calculation

The example is an eight-element dot product:

```c
int dot8(const int *a, const int *b) {
  int sum = 0;

  for (int i = 0; i < 8; ++i)
    sum += a[i] * b[i];

  return sum;
}
```

It calculates:

\[
  a_0b_0 + a_1b_1 + \cdots + a_7b_7
\]

Each product depends on two loads, and the final sum depends on every product. But the work for one element is independent of the work for another. The processor can load `a[3]` and `b[3]` while an earlier multiplication is still producing its result.

That combination gives the scheduler something useful to work with: dependencies it must preserve and independent instructions it can move around them.

The source and final assembly can also be opened in [Compiler Explorer](https://godbolt.org/z/KWsx74T4G). It uses RISC-V Clang 18.1.0 with:

```text
-O2 -march=rv32im -mabi=ilp32 -mcpu=rocket-rv32
```

## Stopping on Both Sides of the Scheduler

First I compiled the function to LLVM IR:

```bash
clang-18 \
  --target=riscv32 \
  -march=rv32im \
  -mabi=ilp32 \
  -mcpu=rocket-rv32 \
  -O2 \
  -S -emit-llvm \
  dot8.c \
  -o dot8.ll
```

I then stopped `llc` immediately before the machine-scheduler pass:

```bash
llc-18 \
  -mtriple=riscv32 \
  -mcpu=rocket-rv32 \
  -mattr=+m \
  -O2 \
  -stop-before=machine-scheduler \
  dot8.ll \
  -o before.mir
```

The result is Machine IR, or MIR. At this point the operations are RISC-V machine instructions, but virtual registers such as `%4` and `%15` have not yet been replaced by physical registers.

To resume from exactly that state and stop after the scheduler, I used:

```bash
llc-18 \
  -mtriple=riscv32 \
  -mcpu=rocket-rv32 \
  -mattr=+m \
  -O2 \
  -start-before=machine-scheduler \
  -stop-after=machine-scheduler \
  before.mir \
  -o after.mir
```

The instructions and their dependencies are the same in both files. Their order is not.

## Before Scheduling

I removed the MIR metadata and kept the instruction body:

```text
%2:gpr  = LW  %0, 0
%3:gpr  = LW  %1, 0
%4:gpr  = MUL %3, %2

%5:gpr  = LW  %0, 4
%6:gpr  = LW  %1, 4
%7:gpr  = MUL %6, %5
%8:gpr  = ADD %7, %4

%9:gpr  = LW  %0, 8
%10:gpr = LW  %1, 8
%11:gpr = MUL %10, %9

%13:gpr = LW  %0, 12
%14:gpr = LW  %1, 12
%15:gpr = MUL %14, %13
%36:gpr = ADD %15, %11
%16:gpr = ADD %36, %8

%17:gpr = LW  %0, 16
%18:gpr = LW  %1, 16
%19:gpr = MUL %18, %17

%21:gpr = LW  %0, 20
%22:gpr = LW  %1, 20
%23:gpr = MUL %22, %21
%40:gpr = ADD %23, %19

%25:gpr = LW  %0, 24
%26:gpr = LW  %1, 24
%27:gpr = MUL %26, %25
%42:gpr = ADD %27, %40
%28:gpr = ADD %42, %16

%29:gpr = LW  %0, 28
%30:gpr = LW  %1, 28
%31:gpr = MUL %30, %29
%32:gpr = ADD %31, %28
```

This order follows the calculation in small groups. It loads a pair of values, multiplies them, and soon combines the result with another product.

The order is correct, but it repeatedly places an instruction close to the instruction that produces its operand. On the selected model, a load has a latency of two cycles and a multiplication has a latency of four. An `ADD` placed immediately after a `MUL` cannot make the multiplication finish sooner. It has to wait for the product.

Meanwhile, loads belonging to later elements have not started. Those loads are independent work that could occupy some of this time.

## After Scheduling

After the machine-scheduler pass, the MIR body becomes:

```text
%2:gpr  = LW  %0, 0
%3:gpr  = LW  %1, 0
%5:gpr  = LW  %0, 4
%6:gpr  = LW  %1, 4
%4:gpr  = MUL %3, %2
%7:gpr  = MUL %6, %5

%9:gpr  = LW  %0, 8
%10:gpr = LW  %1, 8
%13:gpr = LW  %0, 12
%14:gpr = LW  %1, 12
%11:gpr = MUL %10, %9
%15:gpr = MUL %14, %13
%8:gpr  = ADD %7, %4

%17:gpr = LW  %0, 16
%18:gpr = LW  %1, 16
%36:gpr = ADD %15, %11
%19:gpr = MUL %18, %17

%21:gpr = LW  %0, 20
%22:gpr = LW  %1, 20
%25:gpr = LW  %0, 24
%26:gpr = LW  %1, 24
%23:gpr = MUL %22, %21
%27:gpr = MUL %26, %25

%29:gpr = LW  %0, 28
%30:gpr = LW  %1, 28
%16:gpr = ADD %36, %8
%31:gpr = MUL %30, %29
%40:gpr = ADD %23, %19
%42:gpr = ADD %27, %40
%28:gpr = ADD %42, %16
%32:gpr = ADD %31, %28
```

The difference appears immediately. Before scheduling, the first two loads were followed directly by their multiplication. After scheduling, LLVM brings the next two independent loads forward first:

```text
Before: load, load, multiply, load, load, multiply
After:  load, load, load, load, multiply, multiply
```

The same pattern appears throughout the block. Loads from later elements move upward, independent multiplications are started near one another, and additions are delayed until their products are more likely to be ready.

LLVM has not removed a dependency. `%8` still needs `%7` and `%4`; `%32` still needs `%31` and `%28`. The scheduler has only changed what appears between a value's producer and consumer.

That movement has a cost. More loaded values remain live at the same time, so the scheduled order increases register pressure. In this small function LLVM can keep them in registers, and the extra independent work is useful enough for the scheduler to accept that trade-off.

## Producing Assembly for Both Orders

`llvm-mca` reads assembly rather than MIR. I therefore generated two physical-register versions of the function from the same LLVM IR.

For the first version, I disabled the machine scheduler:

```bash
llc-18 \
  -mtriple=riscv32 \
  -mcpu=rocket-rv32 \
  -mattr=+m \
  -O2 \
  -enable-misched=false \
  dot8.ll \
  -o before.s
```

For the second, I allowed the normal `-O2` pipeline to run:

```bash
llc-18 \
  -mtriple=riscv32 \
  -mcpu=rocket-rv32 \
  -mattr=+m \
  -O2 \
  dot8.ll \
  -o after.s
```

The MIR comparison tells us what the machine scheduler moved. These two assembly files give `llvm-mca` valid physical registers while retaining the corresponding unscheduled and scheduled orders.

I removed only `ret` from both inputs so that the measurement covers the sixteen loads, eight multiplications, and seven additions that perform the dot product. Each input therefore contains the same 31 instructions.

## Running `llvm-mca`

I analyzed both assembly files with LLVM 18.1.0:

```bash
llvm-mca-18 \
  -mtriple=riscv32 \
  -mcpu=rocket-rv32 \
  -mattr=+m \
  -iterations=1 \
  -timeline \
  -instruction-info \
  -resource-pressure \
  before.s

llvm-mca-18 \
  -mtriple=riscv32 \
  -mcpu=rocket-rv32 \
  -mattr=+m \
  -iterations=1 \
  -timeline \
  -instruction-info \
  -resource-pressure \
  after.s
```

One iteration is intentional. I want to compare the modeled completion time of this one unrolled block, not many independent copies running in steady state.

Here is the actual summary printed for `before.s`:

```text
Iterations:        1
Instructions:      31
Total Cycles:      58
Total uOps:        31

Dispatch Width:    1
uOps Per Cycle:    0.53
IPC:               0.53
Block RThroughput: 31.0
```

And this is the actual summary printed for `after.s`:

```text
Iterations:        1
Instructions:      31
Total Cycles:      39
Total uOps:        31

Dispatch Width:    1
uOps Per Cycle:    0.79
IPC:               0.79
Block RThroughput: 31.0
```

The instruction-information section of the scheduled run shows the latencies used by the model. The complete report contains one row per instruction; these rows show the three instruction kinds in this block:

```text
Instruction Info:
[1]: #uOps
[2]: Latency
[3]: RThroughput
[4]: MayLoad
[5]: MayStore
[6]: HasSideEffects (U)

[1]    [2]    [3]    [4]    [5]    [6]    Instructions:
 1      2     1.00    *                   lw   a2, 0(a0)
 1      4     1.00                        mul  a2, a3, a2
 1      1     1.00                        add  a2, a3, a2
```

The resource section is also copied directly from the scheduled run:

```text
Resources:
[0]   - RocketUnitALU
[1]   - RocketUnitB
[2]   - RocketUnitFPALU
[3]   - RocketUnitFPDivSqrt
[4]   - RocketUnitIDiv
[5]   - RocketUnitIMul
[6]   - RocketUnitMem

Resource pressure per iteration:
[0]    [1]    [2]    [3]    [4]    [5]    [6]
7.00    -      -      -      -     8.00   16.00
```

Those counts match the program exactly: seven additions, eight multiplications, and sixteen loads. The amount of work is the same before and after scheduling.

## Reading the Result

The direct comparison is:

| Measurement | Scheduler disabled | Scheduler enabled |
|---|---:|---:|
| Instructions | 31 | 31 |
| Total modeled cycles | 58 | 39 |
| Modeled IPC | 0.53 | 0.79 |
| Block reciprocal throughput | 31.0 cycles | 31.0 cycles |

The scheduled order takes 19 fewer modeled cycles:

\[
  \frac{58 - 39}{58} \times 100 \approx 33\%
\]

Nothing was removed. Both versions contain the same sixteen loads, eight multiplications, and seven additions. The improvement comes from using instructions the program already needed as separation between dependent operations.

`Dispatch Width: 1` tells us that this Rocket model dispatches at most one instruction each cycle. With 31 instructions, 31 cycles is therefore an unavoidable dispatch lower bound.

`Total Cycles` answers a different question: how long this one sequence takes to complete in the model. The unscheduled order reaches dependent consumers early and takes 58 cycles. The scheduled order fills more of those gaps with independent work and takes 39.

`IPC` follows from the same totals:

\[
  \frac{31}{58} \approx 0.53
  \qquad
  \frac{31}{39} \approx 0.79
\]

These are modeled values for this experiment, not measured IPC from a physical processor.

`Block RThroughput` remains 31 cycles in both cases because reordering does not remove the single-issue limit. If independent copies of this block could run continuously, its 31 instructions would still require at least 31 dispatch cycles per copy. Scheduling improves the completion time of the dependency chain; it does not widen the machine.

## The Timeline Makes the Empty Time Visible

In the timeline, `D` marks dispatch, the lowercase `e` characters show execution, and `E` marks the end of execution. The important part is not memorizing the symbols. It is noticing how far the 31 instructions stretch across the horizontal cycle axis.

The unscheduled order stretches to cycle 58. The first seven instructions already show the problem: after the second multiplication begins, its dependent addition cannot execute until the product is ready.

```text
Timeline view:
                    0123456789          0123456789          01234567
Index     0123456789          0123456789          0123456789

[0,0]     DeE  .    .    .    .    .    .    .    .    .    .    . .   lw   a2, 0(a0)
[0,1]     .DeE .    .    .    .    .    .    .    .    .    .    . .   lw   a3, 0(a1)
[0,2]     .  DeeeE  .    .    .    .    .    .    .    .    .    . .   mul  a2, a3, a2
[0,3]     .    DeE  .    .    .    .    .    .    .    .    .    . .   lw   a3, 4(a0)
[0,4]     .    .DeE .    .    .    .    .    .    .    .    .    . .   lw   a4, 4(a1)
[0,5]     .    .  DeeeE  .    .    .    .    .    .    .    .    . .   mul  a3, a4, a3
[0,6]     .    .    . DE .    .    .    .    .    .    .    .    . .   add  a2, a3, a2
```

After scheduling, the first four instructions are loads from two different elements. The two multiplications follow only after their input loads have had time to progress:

```text
Timeline view:
                    0123456789          012345678
Index     0123456789          0123456789

[0,0]     DeE  .    .    .    .    .    .    .  .   lw   a2, 0(a0)
[0,1]     .DeE .    .    .    .    .    .    .  .   lw   a3, 0(a1)
[0,2]     . DeE.    .    .    .    .    .    .  .   lw   a4, 4(a0)
[0,3]     .  DeE    .    .    .    .    .    .  .   lw   a5, 4(a1)
[0,4]     .   DeeeE .    .    .    .    .    .  .   mul  a2, a3, a2
[0,5]     .    DeeeE.    .    .    .    .    .  .   mul  a3, a5, a4
[0,6]     .    . DeE.    .    .    .    .    .  .   lw   a4, 8(a0)
[0,7]     .    .  DeE    .    .    .    .    .  .   lw   a5, 8(a1)
```

The complete timeline continues in the same way. Independent loads and multiplications occupy positions that were previously lost to dependency latency. The final additions still form a dependent tail because no schedule can calculate the final sum before its partial sums exist.

## What the Experiment Shows

There are two separate pieces of evidence here.

The MIR files show the compiler transformation itself. They contain the same machine instructions and virtual-register dependencies on either side of one pass, so we can identify exactly which loads, multiplications, and additions LLVM moved.

The `llvm-mca` output evaluates the consequence of those orders under the same Rocket scheduling model. It reports 58 cycles with machine scheduling disabled and 39 cycles with it enabled, while the instruction count and resource usage remain unchanged.

That does not mean every Rocket implementation will execute this function in 39 cycles. `llvm-mca` is a model-based analyzer, not RTL simulation or a hardware benchmark. It does not capture every cache miss, memory-system delay, or implementation detail. The 33% reduction belongs to this block and this model.

What it does establish is more focused: instruction order alone can expose or hide a meaningful amount of usable overlap. The pipeline provides the opportunity, the program's dependencies set the legal limits, and LLVM's scheduler decides which independent instruction should occupy the space between a producer and its consumer.

That brings the hardware and compiler sides of the series together in one example. 

If you've followed till now, i hope you have gone throught the same excitement i've faced while learning this. only for those who reached till here i might post an subsequent post abot:
The next question is whether source-level changes can expose more independent work to LLVM—and how to check that a change helped instead of merely making the code look more complicated. Happy learning :)
---
title: "Evaluating LLVM Instruction Scheduling for Rocket RV32"
date: 2026-08-20 10:00:00 +0530
categories: [Compilers, Instruction Scheduling]
tags: [llvm, llvm-mca, risc-v, rocket, machine-ir, instruction-scheduling]
description: "Following an RV32 dot product through LLVM's machine scheduler, then reading the real llvm-mca output for the instruction order before and after scheduling."
---

[Part 4]({% post_url 2026-08-19-how-llvm-schedules-risc-v-machine-instructions %}) described LLVM's scheduling process: machine instructions become nodes in a dependency graph, legal candidates become ready, and the scheduler chooses among them using information about latency and processor resources.

I understood the idea, but I still wanted to see it happen. I wanted to begin with a small C function, stop LLVM immediately before and after the machine scheduler, and identify the instructions that actually moved. Then I wanted to give both orders to `llvm-mca` and read its output rather than ending with “the scheduled version should be better.”

That is the experiment in this part. Everything below uses LLVM 18.1.0 and LLVM's `rocket-rv32` processor model.

## The Calculation

The example is an eight-element dot product:

```c
int dot8(const int *a, const int *b) {
  int sum = 0;

  for (int i = 0; i < 8; ++i)
    sum += a[i] * b[i];

  return sum;
}
```

It calculates:

\[
  a_0b_0 + a_1b_1 + \cdots + a_7b_7
\]

Each product depends on two loads, and the final sum depends on every product. But the work for one element is independent of the work for another. The processor can load `a[3]` and `b[3]` while an earlier multiplication is still producing its result.

That combination gives the scheduler something useful to work with: dependencies it must preserve and independent instructions it can move around them.

The source and final assembly can also be opened in [Compiler Explorer](https://godbolt.org/z/KWsx74T4G). It uses RISC-V Clang 18.1.0 with:

```text
-O2 -march=rv32im -mabi=ilp32 -mcpu=rocket-rv32
```

## Stopping on Both Sides of the Scheduler

First I compiled the function to LLVM IR:

```bash
clang-18 \
  --target=riscv32 \
  -march=rv32im \
  -mabi=ilp32 \
  -mcpu=rocket-rv32 \
  -O2 \
  -S -emit-llvm \
  dot8.c \
  -o dot8.ll
```

I then stopped `llc` immediately before the machine-scheduler pass:

```bash
llc-18 \
  -mtriple=riscv32 \
  -mcpu=rocket-rv32 \
  -mattr=+m \
  -O2 \
  -stop-before=machine-scheduler \
  dot8.ll \
  -o before.mir
```

The result is Machine IR, or MIR. At this point the operations are RISC-V machine instructions, but virtual registers such as `%4` and `%15` have not yet been replaced by physical registers.

To resume from exactly that state and stop after the scheduler, I used:

```bash
llc-18 \
  -mtriple=riscv32 \
  -mcpu=rocket-rv32 \
  -mattr=+m \
  -O2 \
  -start-before=machine-scheduler \
  -stop-after=machine-scheduler \
  before.mir \
  -o after.mir
```

The instructions and their dependencies are the same in both files. Their order is not.

## Before Scheduling

I removed the MIR metadata and kept the instruction body:

```text
%2:gpr  = LW  %0, 0
%3:gpr  = LW  %1, 0
%4:gpr  = MUL %3, %2

%5:gpr  = LW  %0, 4
%6:gpr  = LW  %1, 4
%7:gpr  = MUL %6, %5
%8:gpr  = ADD %7, %4

%9:gpr  = LW  %0, 8
%10:gpr = LW  %1, 8
%11:gpr = MUL %10, %9

%13:gpr = LW  %0, 12
%14:gpr = LW  %1, 12
%15:gpr = MUL %14, %13
%36:gpr = ADD %15, %11
%16:gpr = ADD %36, %8

%17:gpr = LW  %0, 16
%18:gpr = LW  %1, 16
%19:gpr = MUL %18, %17

%21:gpr = LW  %0, 20
%22:gpr = LW  %1, 20
%23:gpr = MUL %22, %21
%40:gpr = ADD %23, %19

%25:gpr = LW  %0, 24
%26:gpr = LW  %1, 24
%27:gpr = MUL %26, %25
%42:gpr = ADD %27, %40
%28:gpr = ADD %42, %16

%29:gpr = LW  %0, 28
%30:gpr = LW  %1, 28
%31:gpr = MUL %30, %29
%32:gpr = ADD %31, %28
```

This order follows the calculation in small groups. It loads a pair of values, multiplies them, and soon combines the result with another product.

The order is correct, but it repeatedly places an instruction close to the instruction that produces its operand. On the selected model, a load has a latency of two cycles and a multiplication has a latency of four. An `ADD` placed immediately after a `MUL` cannot make the multiplication finish sooner. It has to wait for the product.

Meanwhile, loads belonging to later elements have not started. Those loads are independent work that could occupy some of this time.

## After Scheduling

After the machine-scheduler pass, the MIR body becomes:

```text
%2:gpr  = LW  %0, 0
%3:gpr  = LW  %1, 0
%5:gpr  = LW  %0, 4
%6:gpr  = LW  %1, 4
%4:gpr  = MUL %3, %2
%7:gpr  = MUL %6, %5

%9:gpr  = LW  %0, 8
%10:gpr = LW  %1, 8
%13:gpr = LW  %0, 12
%14:gpr = LW  %1, 12
%11:gpr = MUL %10, %9
%15:gpr = MUL %14, %13
%8:gpr  = ADD %7, %4

%17:gpr = LW  %0, 16
%18:gpr = LW  %1, 16
%36:gpr = ADD %15, %11
%19:gpr = MUL %18, %17

%21:gpr = LW  %0, 20
%22:gpr = LW  %1, 20
%25:gpr = LW  %0, 24
%26:gpr = LW  %1, 24
%23:gpr = MUL %22, %21
%27:gpr = MUL %26, %25

%29:gpr = LW  %0, 28
%30:gpr = LW  %1, 28
%16:gpr = ADD %36, %8
%31:gpr = MUL %30, %29
%40:gpr = ADD %23, %19
%42:gpr = ADD %27, %40
%28:gpr = ADD %42, %16
%32:gpr = ADD %31, %28
```

The difference appears immediately. Before scheduling, the first two loads were followed directly by their multiplication. After scheduling, LLVM brings the next two independent loads forward first:

```text
Before: load, load, multiply, load, load, multiply
After:  load, load, load, load, multiply, multiply
```

The same pattern appears throughout the block. Loads from later elements move upward, independent multiplications are started near one another, and additions are delayed until their products are more likely to be ready.

LLVM has not removed a dependency. `%8` still needs `%7` and `%4`; `%32` still needs `%31` and `%28`. The scheduler has only changed what appears between a value's producer and consumer.

That movement has a cost. More loaded values remain live at the same time, so the scheduled order increases register pressure. In this small function LLVM can keep them in registers, and the extra independent work is useful enough for the scheduler to accept that trade-off.

## Producing Assembly for Both Orders

`llvm-mca` reads assembly rather than MIR. I therefore generated two physical-register versions of the function from the same LLVM IR.

For the first version, I disabled the machine scheduler:

```bash
llc-18 \
  -mtriple=riscv32 \
  -mcpu=rocket-rv32 \
  -mattr=+m \
  -O2 \
  -enable-misched=false \
  dot8.ll \
  -o before.s
```

For the second, I allowed the normal `-O2` pipeline to run:

```bash
llc-18 \
  -mtriple=riscv32 \
  -mcpu=rocket-rv32 \
  -mattr=+m \
  -O2 \
  dot8.ll \
  -o after.s
```

The MIR comparison tells us what the machine scheduler moved. These two assembly files give `llvm-mca` valid physical registers while retaining the corresponding unscheduled and scheduled orders.

I removed only `ret` from both inputs so that the measurement covers the sixteen loads, eight multiplications, and seven additions that perform the dot product. Each input therefore contains the same 31 instructions.

## Running `llvm-mca`

I analyzed both assembly files with LLVM 18.1.0:

```bash
llvm-mca-18 \
  -mtriple=riscv32 \
  -mcpu=rocket-rv32 \
  -mattr=+m \
  -iterations=1 \
  -timeline \
  -instruction-info \
  -resource-pressure \
  before.s

llvm-mca-18 \
  -mtriple=riscv32 \
  -mcpu=rocket-rv32 \
  -mattr=+m \
  -iterations=1 \
  -timeline \
  -instruction-info \
  -resource-pressure \
  after.s
```

One iteration is intentional. I want to compare the modeled completion time of this one unrolled block, not many independent copies running in steady state.

Here is the actual summary printed for `before.s`:

```text
Iterations:        1
Instructions:      31
Total Cycles:      58
Total uOps:        31

Dispatch Width:    1
uOps Per Cycle:    0.53
IPC:               0.53
Block RThroughput: 31.0
```

And this is the actual summary printed for `after.s`:

```text
Iterations:        1
Instructions:      31
Total Cycles:      39
Total uOps:        31

Dispatch Width:    1
uOps Per Cycle:    0.79
IPC:               0.79
Block RThroughput: 31.0
```

The instruction-information section of the scheduled run shows the latencies used by the model. The complete report contains one row per instruction; these rows show the three instruction kinds in this block:

```text
Instruction Info:
[1]: #uOps
[2]: Latency
[3]: RThroughput
[4]: MayLoad
[5]: MayStore
[6]: HasSideEffects (U)

[1]    [2]    [3]    [4]    [5]    [6]    Instructions:
 1      2     1.00    *                   lw   a2, 0(a0)
 1      4     1.00                        mul  a2, a3, a2
 1      1     1.00                        add  a2, a3, a2
```

The resource section is also copied directly from the scheduled run:

```text
Resources:
[0]   - RocketUnitALU
[1]   - RocketUnitB
[2]   - RocketUnitFPALU
[3]   - RocketUnitFPDivSqrt
[4]   - RocketUnitIDiv
[5]   - RocketUnitIMul
[6]   - RocketUnitMem

Resource pressure per iteration:
[0]    [1]    [2]    [3]    [4]    [5]    [6]
7.00    -      -      -      -     8.00   16.00
```

Those counts match the program exactly: seven additions, eight multiplications, and sixteen loads. The amount of work is the same before and after scheduling.

## Reading the Result

The direct comparison is:

| Measurement | Scheduler disabled | Scheduler enabled |
|---|---:|---:|
| Instructions | 31 | 31 |
| Total modeled cycles | 58 | 39 |
| Modeled IPC | 0.53 | 0.79 |
| Block reciprocal throughput | 31.0 cycles | 31.0 cycles |

The scheduled order takes 19 fewer modeled cycles:

\[
  \frac{58 - 39}{58} \times 100 \approx 33\%
\]

Nothing was removed. Both versions contain the same sixteen loads, eight multiplications, and seven additions. The improvement comes from using instructions the program already needed as separation between dependent operations.

`Dispatch Width: 1` tells us that this Rocket model dispatches at most one instruction each cycle. With 31 instructions, 31 cycles is therefore an unavoidable dispatch lower bound.

`Total Cycles` answers a different question: how long this one sequence takes to complete in the model. The unscheduled order reaches dependent consumers early and takes 58 cycles. The scheduled order fills more of those gaps with independent work and takes 39.

`IPC` follows from the same totals:

\[
  \frac{31}{58} \approx 0.53
  \qquad
  \frac{31}{39} \approx 0.79
\]

These are modeled values for this experiment, not measured IPC from a physical processor.

`Block RThroughput` remains 31 cycles in both cases because reordering does not remove the single-issue limit. If independent copies of this block could run continuously, its 31 instructions would still require at least 31 dispatch cycles per copy. Scheduling improves the completion time of the dependency chain; it does not widen the machine.

## The Timeline Makes the Empty Time Visible

In the timeline, `D` marks dispatch, the lowercase `e` characters show execution, and `E` marks the end of execution. The important part is not memorizing the symbols. It is noticing how far the 31 instructions stretch across the horizontal cycle axis.

The unscheduled order stretches to cycle 58. The first seven instructions already show the problem: after the second multiplication begins, its dependent addition cannot execute until the product is ready.

```text
Timeline view:
                    0123456789          0123456789          01234567
Index     0123456789          0123456789          0123456789

[0,0]     DeE  .    .    .    .    .    .    .    .    .    .    . .   lw   a2, 0(a0)
[0,1]     .DeE .    .    .    .    .    .    .    .    .    .    . .   lw   a3, 0(a1)
[0,2]     .  DeeeE  .    .    .    .    .    .    .    .    .    . .   mul  a2, a3, a2
[0,3]     .    DeE  .    .    .    .    .    .    .    .    .    . .   lw   a3, 4(a0)
[0,4]     .    .DeE .    .    .    .    .    .    .    .    .    . .   lw   a4, 4(a1)
[0,5]     .    .  DeeeE  .    .    .    .    .    .    .    .    . .   mul  a3, a4, a3
[0,6]     .    .    . DE .    .    .    .    .    .    .    .    . .   add  a2, a3, a2
```

After scheduling, the first four instructions are loads from two different elements. The two multiplications follow only after their input loads have had time to progress:

```text
Timeline view:
                    0123456789          012345678
Index     0123456789          0123456789

[0,0]     DeE  .    .    .    .    .    .    .  .   lw   a2, 0(a0)
[0,1]     .DeE .    .    .    .    .    .    .  .   lw   a3, 0(a1)
[0,2]     . DeE.    .    .    .    .    .    .  .   lw   a4, 4(a0)
[0,3]     .  DeE    .    .    .    .    .    .  .   lw   a5, 4(a1)
[0,4]     .   DeeeE .    .    .    .    .    .  .   mul  a2, a3, a2
[0,5]     .    DeeeE.    .    .    .    .    .  .   mul  a3, a5, a4
[0,6]     .    . DeE.    .    .    .    .    .  .   lw   a4, 8(a0)
[0,7]     .    .  DeE    .    .    .    .    .  .   lw   a5, 8(a1)
```

The complete timeline continues in the same way. Independent loads and multiplications occupy positions that were previously lost to dependency latency. The final additions still form a dependent tail because no schedule can calculate the final sum before its partial sums exist.

## What the Experiment Shows

There are two separate pieces of evidence here.

The MIR files show the compiler transformation itself. They contain the same machine instructions and virtual-register dependencies on either side of one pass, so we can identify exactly which loads, multiplications, and additions LLVM moved.

The `llvm-mca` output evaluates the consequence of those orders under the same Rocket scheduling model. It reports 58 cycles with machine scheduling disabled and 39 cycles with it enabled, while the instruction count and resource usage remain unchanged.

That does not mean every Rocket implementation will execute this function in 39 cycles. `llvm-mca` is a model-based analyzer, not RTL simulation or a hardware benchmark. It does not capture every cache miss, memory-system delay, or implementation detail. The 33% reduction belongs to this block and this model.

What it does establish is more focused: instruction order alone can expose or hide a meaningful amount of usable overlap. The pipeline provides the opportunity, the program's dependencies set the legal limits, and LLVM's scheduler decides which independent instruction should occupy the space between a producer and its consumer.

That brings the hardware and compiler sides of the series together in one example. 

If you've followed till now, i hope you have gone throught the same excitement i've faced while learning this. only for those who reached till here i might post an subsequent post abot:
The next question is whether source-level changes can expose more independent work to LLVM—and how to check that a change helped instead of merely making the code look more complicated. Happy learning :)
