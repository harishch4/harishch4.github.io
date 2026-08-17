---
title: "Part 4 : How LLVM Schedules RISC-V Machine Instructions"
date: 2026-08-17 12:00:00 +0530
categories: [Compilers, Instruction Scheduling]
tags: [llvm, risc-v, machine-instructions, dependency-graph, instruction-scheduling, register-pressure]
description: "Following a small RISC-V instruction sequence through LLVM's dependency graph, ready queues, and scheduling decisions."
---

[Part 3]({% post_url 2026-08-18-how-llvm-models-risc-v-processor-latency-and-resources %}) gave LLVM the timing information it had been missing. For `rocket-rv32`, it can see that an integer multiplication produces its result after four cycles, that the multiplication unit is occupied for one cycle, and that the processor begins at most one instruction per cycle.

None of those facts chooses an instruction order by itself.

LLVM still has to look at a group of instructions, preserve every relationship that makes the program correct, and decide which legal order is likely to keep the processor useful. I wanted to understand that decision without treating the scheduler as a black box, so I followed the same three instructions used in Part 3 into LLVM's machine scheduler:

```asm
mul   t0, t1, t2
add   t3, t0, t4
addi  a0, a0, 4
```

The `add` must wait for the product. The pointer update is independent of that calculation. The interesting question is how LLVM discovers those facts and turns them into a choice.

## The Processor Model Still Does Not Choose an Order

The processor model answers questions about individual operations. It says how long a result takes to become usable, which execution resource an instruction needs, and how many instructions the processor can begin in a cycle.

The model does not say that `addi` should appear between a particular `mul` and `add`. That decision depends on the instructions surrounding them. In another block, the pointer update may feed a later load, increase the number of values that must remain in registers, or compete with other arithmetic for the same resource.

Scheduling is therefore not a table lookup. LLVM has to combine two kinds of information:

| Information | What it contributes |
|---|---|
| Instruction dependencies | Which orders preserve the program's meaning |
| Processor scheduling model | Which legal orders are likely to avoid waiting or resource conflicts |

Dependencies define the legal space. The processor model helps LLVM choose within it.

## The Scheduler Works on Machine Instructions

By the time this scheduling pass runs, LLVM is no longer arranging C expressions. Instruction selection has already converted the calculation into target-specific RISC-V operations represented inside LLVM as `MachineInstr` objects.

At this point, many operands are still **virtual registers**. A virtual register names a value without yet committing it to a physical register such as `t0` or `a0`. A simplified view of the three instructions could look like this:

```text
%product = MUL  %left, %right
%sum     = ADD  %accumulator, %product
%next    = ADDI %pointer, 4
```

The names are not source-language variables. They are the inputs and outputs of machine instructions, written this way only to make the value flow visible.

Scheduling before physical-register assignment is useful because LLVM can move instructions while it still has a clear view of those values. It also creates a new concern: the chosen order affects how many virtual-register values are alive at the same time. We will return to that after seeing how LLVM establishes correctness.

LLVM schedules manageable regions rather than freely moving instructions through an entire function. For the example here, we can treat the region as the straight-line instructions inside one basic block. Branches, calls, and other boundaries restrict how far an instruction may move.

## Dependencies Become a Graph

LLVM begins by creating one scheduling node for each real machine instruction in the region. In the implementation, this node is called an `SUnit`. It then adds directed edges between nodes that must remain ordered.

For our sequence, `%product` is written by `MUL` and read by `ADD`. LLVM therefore creates an edge from the multiplication to the addition:

```text
MUL  ---- product, latency 4 ---->  ADD

ADDI              no edge to either instruction
```

The arrow means more than “keep `MUL` somewhere before `ADD`.” Its latency comes from the selected processor model. With the Rocket model from Part 3, the product is expected to become available four cycles after the multiplication begins.

There is no edge connecting `ADDI` to the other two instructions because this simplified pointer update reads and writes different values. That missing edge is how LLVM represents the freedom we noticed from the hardware side: the update can move relative to the multiply-add chain without changing its result.

This structure is a **directed acyclic graph**, or DAG. “Directed” means every edge has an order. “Acyclic” means following the arrows cannot lead back to the starting node. A cycle would claim that one instruction must come before itself through a chain of dependencies, leaving no valid schedule.

Register values are not the only source of edges. Memory operations also need ordering when LLVM cannot prove that they access separate locations. If a load might read a location written by an earlier store, moving the load above that store could change the value it observes. Calls and instructions with side effects impose further limits. The scheduler may reorder an instruction only when the graph says the move is safe.

This is an important point: LLVM does not move `ADDI` because it looks harmless in assembly text. It moves an instruction only after its machine-level operands and side effects show that the required edges are absent.

## The Ready Set Defines the Legal Choices

Once the graph exists, LLVM does not try every possible ordering. It repeatedly chooses one instruction from a **ready set**.

For a top-down schedule, an instruction is ready when every predecessor that must appear before it has already been scheduled. At the beginning of the example:

| Instruction | Ready? | Reason |
|---|---|---|
| `MUL` | Yes | It does not wait for another instruction in this region |
| `ADD` | No | Its `%product` input comes from `MUL` |
| `ADDI` | Yes | It is independent of the multiply-add chain |

The scheduler may initially choose either `MUL` or `ADDI`; both choices are correct. It may not choose `ADD`.

Suppose it chooses `MUL`. The dependency that blocked `ADD` has now been placed before it, so the addition can enter the set of dependency-ready candidates. That does not mean its product is available in the very next cycle. The edge latency still tells the scheduler that selecting `ADD` immediately is expected to cause a delay.

This gives us two separate questions:

1. Is the instruction legally movable to this part of the schedule?
2. If it is legal, is it a useful choice for the target processor now?

The dependency graph answers the first. Latency and resource tracking help answer the second.

LLVM's generic machine scheduler can grow a schedule from both ends of a region rather than only from the top. It maintains candidates that can legally be placed at the beginning and candidates that can legally be placed at the end. Scheduling from the bottom applies the same idea in reverse: an instruction becomes a candidate when the instructions that must follow it have already been placed at the bottom.

The two directions give the scheduler more freedom when one end has an unattractive choice. The essential rule remains the same: it chooses only a node whose required neighbours on that side have already been handled.

## Latency Makes Some Legal Choices Better

Return to the two initially ready instructions. If LLVM schedules `ADDI` first, the order can become:

```asm
addi  a0, a0, 4
mul   t0, t1, t2
add   t3, t0, t4
```

This is legal, but the independent update occurs before the long-latency operation has begun. It does nothing to cover the time between `MUL` and `ADD`.

If LLVM schedules `MUL` first and then uses `ADDI` while the product is pending, the order becomes:

```asm
mul   t0, t1, t2
addi  a0, a0, 4
add   t3, t0, t4
```

The multiplication has started one cycle earlier, and the pointer update now occupies one of the cycles before the product is consumed. The four-cycle dependency has not disappeared; one useful instruction has been placed inside its delay.

This is why schedulers pay attention to the **critical path**. A critical path is a dependency chain whose accumulated latency limits how soon the region can complete. Starting a long chain early gives its results time to develop while unrelated instructions execute.

The multiply-add edge is the only dependent chain in this tiny example, so the intuition is clear. In a real block there may be several chains, different edge latencies, and many independent candidates. LLVM computes path information for the scheduling nodes and uses it when comparing choices. It is trying to avoid a schedule in which an important long-latency chain begins unnecessarily late.

The result is a preference, not a new correctness rule. Choosing `ADDI` first would still compute the right answer. It is simply less helpful for this processor model and this surrounding instruction sequence.

## Independence Does Not Guarantee Resource Availability

The graph can also contain two instructions with no edge between them even though the processor cannot begin them close together. Consider two divisions whose inputs and outputs are unrelated:

```asm
div  t0, t1, t2
div  t3, t4, t5
```

Their register dependencies permit either order. In the Rocket model, however, integer division occupies its division resource for 33 cycles. After the first division begins, the second may be dependency-ready while the resource it needs is still unavailable.

This is why LLVM's scheduling boundary tracks more than the ready set. It also tracks the expected cycle, issue capacity, and processor-resource use. A candidate that would run into a busy resource can remain pending until the modeled hazard has passed, while another ready instruction may be selected.

The distinction mirrors the hardware problem:

| Condition | What prevents the instruction from being useful now? |
|---|---|
| Dependency not satisfied | An input or required ordering is still pending |
| Resource not available | The needed execution unit is still occupied |
| Issue width exhausted | The processor cannot begin another instruction in that cycle |

An instruction can be independent and still be a poor immediate choice. The DAG describes relationships between instructions; the scheduling model describes limits in the machine they share.

## Register Pressure Pulls in the Other Direction

Starting long-latency work early often helps, but moving every producer as early as possible creates another problem.

Consider an instruction that calculates a value near the top of a block, while the only instruction that uses it remains near the bottom. From the moment the value is produced until its last use, it has to be kept somewhere. The value is said to be **live** over that interval.

If scheduling causes too many values to be live at once, there may not be enough physical registers for all of them. Register allocation then has to save some values to memory and load them again later. Those extra operations are called **spills**, and the cost of them can outweigh the cycles saved by an aggressive schedule.

The pre-register-allocation scheduler therefore tracks **register pressure**: an estimate of how heavily the future physical-register sets are being demanded by simultaneously live values. A candidate that would exceed a target limit or increase an already critical pressure is less attractive.

This creates a real tension:

- Moving a producer earlier may hide its latency.
- Keeping the producer closer to its consumer may shorten the value's live range.

There is no single rule that wins in every block. LLVM's generic scheduler compares candidates using a sequence of heuristics that includes register pressure, latency, and processor resources. If none gives a stronger reason, original instruction order can serve as a final tie-breaker.

The word **heuristic** matters here. Finding the globally best order while considering all dependencies, resources, latencies, and register-allocation effects is too expensive to solve exactly for every scheduling region during an ordinary compilation. LLVM makes informed local choices instead.

## What Is Generic and What Is RISC-V-Specific

Most of the mechanism described here is shared across LLVM targets. [`ScheduleDAGInstrs.cpp`](https://github.com/llvm/llvm-project/blob/8a34f9d127e52586c7285fa07efd1b6b8e428ca0/llvm/lib/CodeGen/ScheduleDAGInstrs.cpp) creates scheduling nodes and adds register and memory dependencies. [`MachineScheduler.cpp`](https://github.com/llvm/llvm-project/blob/8a34f9d127e52586c7285fa07efd1b6b8e428ca0/llvm/lib/CodeGen/MachineScheduler.cpp) drives the ready queues, tracks scheduling boundaries, and implements the generic candidate-selection strategy.

RISC-V does not replace that machinery with a completely separate scheduler. Its target code creates the live machine scheduling DAG using a RISC-V strategy derived from LLVM's generic scheduler. It also supplies the processor model examined in Part 3 and can add target-specific graph adjustments for features such as instruction fusion, load or store clustering, and vector-state handling. Those hooks are visible in [`RISCVTargetMachine.cpp`](https://github.com/llvm/llvm-project/blob/8a34f9d127e52586c7285fa07efd1b6b8e428ca0/llvm/lib/Target/RISCV/RISCVTargetMachine.cpp) and [`RISCVMachineScheduler.cpp`](https://github.com/llvm/llvm-project/blob/8a34f9d127e52586c7285fa07efd1b6b8e428ca0/llvm/lib/Target/RISCV/RISCVMachineScheduler.cpp).

For the scalar RV32 example, the division of responsibility is simple:

| Layer | Responsibility |
|---|---|
| Generic LLVM scheduling machinery | Build dependencies, maintain candidates, and compare scheduling costs |
| RISC-V instruction descriptions | State which values and abstract scheduling classes an instruction uses |
| Selected RISC-V processor model | Supply latency, issue width, and execution-resource behaviour |
| RISC-V scheduling hooks | Add target-specific preferences or constraints where the generic model is not enough |

The target supplies the facts that make a decision processor-aware. The generic scheduler supplies the process that turns those facts into an order.

## The Output Is an Instruction Order, Not a Simulation

After each selection, LLVM moves the chosen `MachineInstr` to its scheduled position and updates the graph and ready queues. When the region is complete, the machine instructions appear in their new order.

It is useful to be precise about what this produces. LLVM is not executing the block, and it is not proving that every dynamic run will take a fixed number of cycles. Cache misses, branch behaviour, and other runtime effects remain unknown. The scheduler is also not turning the compiler into an RTL model of Rocket.

It is arranging instructions using the target's expected timing. In the simple example, that may place the independent pointer update between the multiplication and its consumer:

```asm
mul   t0, t1, t2
addi  a0, a0, 4
add   t3, t0, t4
```

The hardware still reads this as an ordinary in-order instruction stream. If a value is not ready when its consumer arrives, the processor still stalls. The compiler's contribution is to make that stall less likely by placing useful independent work where the model predicts a wait.

That closes the path we started from the hardware side. Pipeline stages create overlap. Dependencies restrict it. Forwarding shortens some waits. The processor model tells LLVM which waits and resource conflicts to expect. The machine scheduler then searches the legal instruction order for work that can occupy those gaps.

## The Question Left for Part 5

So far, the scheduling decision has been explained using a deliberately small sequence. The next step is to see whether the same reasoning is visible in a real compilation.

In Part 5, I will compile a concrete RV32 loop for `rocket-rv32`, inspect LLVM's machine instructions before and after scheduling, and compare the resulting order with the latency and resource model. `llvm-mca` can then help us examine the modeled bottlenecks, without pretending that its estimate is a hardware simulation.

That experiment will let us separate three things that are easy to blur together: the independent work present in the program, the order LLVM actually chooses, and the behaviour predicted by the processor model.
