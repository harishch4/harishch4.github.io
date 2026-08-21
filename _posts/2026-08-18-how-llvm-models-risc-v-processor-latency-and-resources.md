---
title: "Part 3 : How LLVM Models RISC-V Processor Latency and Resources"
date: 2026-08-17 12:00:00 +0530
categories: [Compilers, Instruction Scheduling]
tags: [llvm, risc-v, rocket, tablegen, scheduling-model, instruction-latency]
description: "Building LLVM's RISC-V scheduling model from the hardware questions established by instruction pipelining, forwarding, and instruction-level parallelism."
---

[Part 2]({% post_url 2026-08-17-pipeline-dependencies-forwarding-and-instruction-level-parallelism %}) ended at the point where the compiler finally had a reason to care about the processor. A dependency tells the compiler which instruction must come first, but it does not say how much distance the two instructions need.

Consider the end of the dot-product calculation again:
```asm
mul  t0, t0, t1
add  a2, a2, t0
```

The `add` has to follow the `mul` because it reads the product in `t0`. That relationship is true on every processor. The useful distance between them is not. If one processor produces the multiplication result after one cycle and another produces it after four, the same instruction order will behave differently on the two machines.

This was the part I had been missing when I first looked at instruction scheduling. I was treating latency as if it belonged to the instruction name. In reality, `mul` defines an operation; the processor implementation determines when its result becomes usable.

LLVM therefore needs two descriptions at once. It needs a processor-independent description of what each RISC-V instruction reads and writes, and a processor-specific description of how that work passes through the available hardware.

## Why Dependencies Are Not Enough

Suppose LLVM sees these three instructions:

```asm
mul   t0, t1, t2
add   t3, t0, t4
addi  a0, a0, 4
```

The register operands reveal that the `add` depends on the `mul`. The pointer update does not depend on either of them. From that information alone, LLVM can know that moving the `add` before the `mul` would be incorrect and that the `addi` might be movable.

It still cannot judge whether moving the `addi` is useful. If the multiplication result is ready in the next cycle, there may be no delay to cover. If the result takes four cycles, placing independent work between the producer and consumer may avoid several empty cycles.

There is a second question as well. Imagine two independent multiplications:

```asm
mul  t0, t1, t2
mul  t3, t4, t5
```

Even though the second instruction does not need the first result, it can begin immediately only if the multiplication hardware is ready to accept another operation. A unit may take four cycles to produce each result while still accepting a new instruction every cycle. Another unit may remain occupied for all four cycles.

The compiler therefore needs to know both when a result becomes available and how long an instruction prevents other instructions from using the same hardware.

## Separating Instruction Meaning from Processor Timing

My first expectation was that LLVM would attach a number such as “four cycles” directly to the RISC-V `MUL` instruction. That would make the instruction definition valid for only one processor.

Instead, LLVM gives instructions abstract scheduling names. The RISC-V definition of `LW` includes this scheduling description:

```tablegen
Sched<[WriteLDW, ReadMemBase]>
```

The definition of `MUL` uses:

```tablegen
Sched<[WriteIMul, ReadIMul, ReadIMul]>
```

These declarations can be seen in [`RISCVInstrInfo.td`](https://github.com/llvm/llvm-project/blob/8a34f9d127e52586c7285fa07efd1b6b8e428ca0/llvm/lib/Target/RISCV/RISCVInstrInfo.td) and [`RISCVInstrInfoM.td`](https://github.com/llvm/llvm-project/blob/8a34f9d127e52586c7285fa07efd1b6b8e428ca0/llvm/lib/Target/RISCV/RISCVInstrInfoM.td).

`WriteLDW` does not mean that every load word takes the same number of cycles. It means that the instruction produces a value of the kind described as a load-word result. `ReadMemBase` describes its address-base input. In the same way, `WriteIMul` identifies an integer-multiplication result, while the two `ReadIMul` entries describe its two inputs.

These names form the connection between an instruction and a processor model. Each processor can give `WriteLDW` and `WriteIMul` different latency and resource information without changing the definition of `LW` or `MUL`.

The files use **TableGen**, LLVM's declarative description language. The `.td` records are processed while LLVM is built and turned into tables that the compiler can query. The important point for this story is not the TableGen syntax itself. It is the separation it creates:

| Description | Question answered |
|---|---|
| RISC-V instruction definition | What kind of result and operands does this instruction have? |
| Processor scheduling model | How does this processor execute that kind of instruction? |

## Selecting Rocket RV32 Selects a Timing Model

LLVM has to know which processor-specific interpretation to use. In [`RISCVProcessors.td`](https://github.com/llvm/llvm-project/blob/8a34f9d127e52586c7285fa07efd1b6b8e428ca0/llvm/lib/Target/RISCV/RISCVProcessors.td), the processor name `rocket-rv32` is associated with `RocketModel`:

```tablegen
def ROCKET_RV32 : RISCVProcessorModel<"rocket-rv32",
                                      RocketModel,
                                      [Feature32Bit,
                                       FeatureStdExtI,
                                       FeatureStdExtZifencei,
                                       FeatureStdExtZicsr]>;
```

When `rocket-rv32` is selected as the target CPU, LLVM uses `RocketModel` for scheduling and related cost decisions. Other RISC-V processors can map the same abstract instruction descriptions to entirely different models.

This is why selecting only the RISC-V ISA is not enough for precise scheduling. `RV32IM` tells LLVM which integer and multiplication instructions are available. It does not tell LLVM how many instructions the core can begin per cycle, how long a multiply takes, or whether two operations compete for the same execution unit. Those are properties of the chosen processor model.

## Describing Width and Execution Resources

The Rocket scheduling model begins with a few properties of the core:

```tablegen
def RocketModel : SchedMachineModel {
  let MicroOpBufferSize = 0;
  let IssueWidth = 1;
  let LoadLatency = 3;
  let MispredictPenalty = 3;
  let CompleteModel = false;
}
```

The full definition is in [`RISCVSchedRocket.td`](https://github.com/llvm/llvm-project/blob/8a34f9d127e52586c7285fa07efd1b6b8e428ca0/llvm/lib/Target/RISCV/RISCVSchedRocket.td).

`IssueWidth = 1` is the compiler-side form of the single-issue hardware model used in Parts 1 and 2. At most one micro-operation is dispatched in a cycle. Ordinary instructions in this model normally count as one micro-operation, so LLVM cannot construct a schedule that assumes two of them begin together.

`MicroOpBufferSize = 0` tells LLVM to treat the model as in-order rather than as a machine with a window of buffered operations that can be selected out of order. `LoadLatency` and `MispredictPenalty` provide broader cost information for loads and incorrectly predicted branches. `CompleteModel = false` says that this model does not claim to describe every scheduling class that LLVM could encounter.

The top-level `LoadLatency = 3` and the `WriteLDW` latency of two cycles shown later are used at different levels. `LoadLatency` is a coarse machine-wide load estimate. `WriteLDW` supplies the producer-to-consumer latency for the specific load-word scheduling class. When following the timing of `lw` in this model, the `WriteLDW` mapping is the relevant entry.

The model then names the hardware resources that instructions may need:

```tablegen
let BufferSize = 0 in {
  def RocketUnitALU  : ProcResource<1>;
  def RocketUnitIMul : ProcResource<1>;
  def RocketUnitMem  : ProcResource<1>;
  def RocketUnitB    : ProcResource<1>;
}
```

`ProcResource<1>` says that the model contains one unit of that resource kind. Integer arithmetic uses `RocketUnitALU`, integer multiplication uses `RocketUnitIMul`, loads and stores use `RocketUnitMem`, and control-flow instructions use `RocketUnitB`.

These records are a scheduling abstraction, not a drawing of every RTL block. Their purpose is to let LLVM recognize conflicts. Two instructions that both require a single unavailable resource cannot be placed as if that hardware could accept them simultaneously.

The `BufferSize = 0` setting makes these in-order dispatch resources. LLVM must respect their availability from the point at which the instructions are issued in program order. The underlying meanings of these fields are defined in [`TargetSchedule.td`](https://github.com/llvm/llvm-project/blob/8a34f9d127e52586c7285fa07efd1b6b8e428ca0/llvm/include/llvm/Target/TargetSchedule.td).

## Latency Is Not Resource Occupancy

The abstract writes from the instruction definitions are connected to Rocket's resources using `WriteRes` records. A few entries are enough to show the model:

```tablegen
def : WriteRes<WriteIALU, [RocketUnitALU]>;

let Latency = 4 in {
  def : WriteRes<WriteIMul, [RocketUnitIMul]>;
}

let Latency = 2 in {
  def : WriteRes<WriteLDW, [RocketUnitMem]>;
}

def : WriteRes<WriteIDiv, [RocketUnitIDiv]> {
  let Latency = 33;
  let ReleaseAtCycles = [33];
}
```

`Latency` describes the producer-to-consumer delay: how many cycles LLVM should place between the instruction that writes a result and an instruction that reads it. An integer ALU write uses the default latency of one cycle. In the Rocket model, a word load has a latency of two cycles, and an integer multiplication has a latency of four.

Resource occupancy answers a different question: when can another instruction use the same unit? LLVM represents that with `ReleaseAtCycles`. When it is omitted, as it is for the ALU, word load, and multiplication entries above, the resource is consumed for one cycle. The model therefore allows the resource to accept another suitable instruction in the following cycle even if the first result is not ready yet.

Integer division is different. Its result latency is 33 cycles, and `RocketUnitIDiv` is released only after 33 cycles. The next division cannot use that unit during the interval.

The distinction can be summarized directly from the model:

| Operation | Scheduling write | Result latency | Resource | Modeled occupancy |
|---|---|---:|---|---:|
| Integer ALU operation | `WriteIALU` | 1 cycle | `RocketUnitALU` | 1 cycle |
| Load word | `WriteLDW` | 2 cycles | `RocketUnitMem` | 1 cycle |
| Integer multiply | `WriteIMul` | 4 cycles | `RocketUnitIMul` | 1 cycle |
| Integer divide | `WriteIDiv` | 33 cycles | `RocketUnitIDiv` | 33 cycles |

This is the same distinction we reached from the hardware side. Latency determines when a dependent instruction can use the result. Occupancy determines when another instruction can use the execution resource. One constrains dependencies; the other can constrain otherwise independent instructions.

## Representing Operand Timing and Forwarding

Part 2 showed that the required delay can depend on the path between a producer and a particular consumer operand. A forwarded value may be available earlier than a normal register-file path would suggest.

LLVM can express such a case with `ReadAdvance`. Conceptually, a record of the form

```tablegen
ReadAdvance<SomeRead, N, [SomeWrite]>
```

says that this kind of operand read may occur `N` cycles earlier when it consumes a result from the listed kind of write. That lets a processor model describe a bypass that applies to a particular producer-consumer combination rather than reducing the producer's latency for every possible use.

The Rocket file declares zero-cycle advances for its listed read types. LLVM therefore uses the corresponding write latency without an additional operand-specific reduction. This does not mean the hardware has no forwarding. The write latency is already the effective delay that the scheduling model wants LLVM to respect; a non-zero `ReadAdvance` is needed only when a particular read should alter that delay.

This is an important boundary. LLVM does not reproduce every wire in the forwarding network. It records the timing consequence that matters when instructions are placed relative to one another.

## Following the Dot-Product Instructions Through the Model

We can now follow the same instructions across the layers of LLVM without treating the `.td` files as unrelated definitions.

For the load

```asm
lw  t1, 0(a1)
```

the RISC-V instruction definition assigns `WriteLDW` to the result and `ReadMemBase` to the address input. Selecting `rocket-rv32` chooses `RocketModel`. That model maps `WriteLDW` to `RocketUnitMem` with a latency of two cycles.

For the multiplication

```asm
mul  t0, t0, t1
```

the instruction definition assigns `WriteIMul` to the result and `ReadIMul` to both inputs. Rocket maps `WriteIMul` to `RocketUnitIMul` with a latency of four cycles.

The connection is therefore:

| Layer | `lw` | `mul` |
|---|---|---|
| Instruction meaning | `WriteLDW`, `ReadMemBase` | `WriteIMul`, two `ReadIMul` operands |
| Selected processor | `RocketModel` | `RocketModel` |
| Execution resource | `RocketUnitMem` | `RocketUnitIMul` |
| Result latency | 2 cycles | 4 cycles |

The compiler now has the information Part 2 said was missing. It knows that the multiplication depends on the load result, that the load result has a two-cycle latency in this model, and that an independent instruction may be useful between them. It also knows that the product itself takes four cycles to become available to the accumulating `add`.

What it has not yet done is choose an order. The model supplies the constraints and costs; the scheduling algorithm still has to decide which ready instruction should be placed next.

## A Scheduling Model Is Not a Simulator

It is tempting to read the latency values as a promise that the processor will always take exactly that many cycles. The model is not that precise.

A load marked with a two-cycle scheduling latency can still take much longer when data misses in the cache. Branch prediction depends on runtime history. Other system activity can affect memory timing. The scheduling model does not run the program or know its input values.

Instead, it gives LLVM a stable approximation of the processor behaviour that instruction ordering can influence. It describes expected result delays, dispatch limits, and competition for execution resources. The quality of the resulting schedule depends on how accurately those entries represent the processor being targeted.

This also explains why copying Rocket's numbers into a different RV32IMF core would not create a model for that core. The instruction set may match while the multiplication latency, memory timing, forwarding paths, and execution resources differ. A useful scheduling model has to describe the implementation, not merely the ISA.

## The Question Left for Part 4

We now have both sides of the scheduling problem. The instructions define the dependencies that preserve the calculation. The processor model assigns timing and resource constraints to those instructions.

The remaining question is how LLVM turns that information into an order. It needs to represent the dependencies, determine which instructions are currently legal to schedule, and choose among several independent candidates without creating a different bottleneck elsewhere.

That is the next part of the series.
