---
layout: post
title: "Cache Fundamentals: From Cache Lines to Way Locking"
date: 2026-08-12 12:00:00 +0530
categories: [Computer Architecture, Memory Systems]
tags: [cache, way-locking, local-memory, computer-architecture]
description: "A beginner-friendly explanation of cache lines, sets, ways, eviction, and how way locking can reserve cache capacity as software-managed local memory."
math: true
---

Modern processors can execute instructions much faster than they can fetch data from main memory. A cache reduces this gap by keeping useful data close to the processor.

That simple idea quickly introduces unfamiliar terms: *cache line*, *set*, *way*, *associativity*, *eviction*, and *way locking*. This article builds those ideas from the ground up and then explains how a cache can dedicate some of its ways to software-managed local memory.

## Why Do We Need a Cache?

Consider a processor executing an addition:

```c
result = a + b;
```

The addition itself may be fast, but the processor first needs the values of `a` and `b`. If they must be fetched from slower main memory, the processor may spend many cycles waiting.

The memory system therefore usually has multiple levels:

```text
Processor
   │
   ▼
Registers       smallest and fastest
   │
   ▼
L1 cache
   │
   ▼
L2/L3 cache
   │
   ▼
Main memory     larger and slower
```

A cache is a small, fast memory that keeps copies of data from a larger, slower memory. When the processor requests an address, the cache checks whether it already has the requested data.

- **Cache hit:** the data is present in the cache.
- **Cache miss:** the data is absent and must be fetched from a lower memory level.

The goal is to make most accesses hits.

## Cache Lines and Memory Blocks

A cache does not normally fetch a single byte at a time. It transfers a fixed-size group of adjacent bytes called a **memory block** and stores it in a **cache line**.

For most discussions, *cache block size* and *cache line size* mean the same size. The words emphasize two sides of the transfer:

- A **memory block** is the fixed-size region in memory.
- A **cache line** is the entry holding that block inside the cache.

Suppose the line size is 64 bytes. If the processor requests a four-byte value at address 100, the cache may fetch the aligned block covering addresses 64 through 127.

```text
Memory addresses

64                                               127
┌──────────────────────────────────────────────────┐
│                 64-byte block                    │
│                         ▲                        │
└─────────────────────────┼────────────────────────┘
                    requested value
```

Fetching nearby bytes is useful because programs often access nearby data. For example, after reading `array[0]`, a program will often read `array[1]`.

## Why Is a Cache Divided into Sets?

The cache must answer an important question very quickly:

> If this address is cached, where should I look for it?

Searching every cache line would be expensive. Instead, each memory block is mapped to one **set**. A set is a small group of cache-line slots.

A simplified mapping is:

$$
\text{set index} = \text{memory block number} \bmod \text{number of sets}
$$

If there are four sets, blocks may map like this:

| Memory block | Cache set |
|---:|---:|
| 0 | 0 |
| 1 | 1 |
| 2 | 2 |
| 3 | 3 |
| 4 | 0 |
| 5 | 1 |

Blocks 0 and 4 both map to Set 0. This means multiple memory blocks may compete for space in the same set.

## What Is a Cache Way?

If a set held only one line, every new block mapping to that set would replace the old one. To reduce this conflict, a set can contain multiple line slots. Each slot is called a **way**.

A four-way set-associative cache has four possible slots in every set:

| Set | Way 0 | Way 1 | Way 2 | Way 3 |
|---:|---|---|---|---|
| 0 | one line | one line | one line | one line |
| 1 | one line | one line | one line | one line |
| 2 | one line | one line | one line | one line |
| ... | ... | ... | ... | ... |

A memory block still maps to exactly one set, but it may occupy any way within that set.

Therefore:

$$
\boxed{\text{total cache lines} = \text{number of sets} \times \text{number of ways}}
$$

And the data capacity is:

$$
\boxed{\text{cache capacity} = \text{sets} \times \text{ways} \times \text{line size}}
$$

For example, consider a cache with:

- 256 sets
- 4 ways per set
- 64 bytes per line

It contains:

$$
256 \times 4 = 1024\ \text{cache lines}
$$

Its data capacity is:

$$
256 \times 4 \times 64 = 65{,}536\ \text{bytes} = 64\ \text{KiB}
$$

This calculation usually excludes small amounts of metadata such as tags, valid bits, dirty bits, and replacement state.

## What Happens When a Set Is Full?

Suppose four blocks mapping to Set 0 occupy all four ways. A fifth block also maps to Set 0.

The cache must select an existing line to remove before inserting the new one. This removal is called **eviction**. A replacement policy—such as least-recently-used, pseudo-LRU, or random selection—chooses the victim.

This automatic behavior is normally useful, but it creates a problem for important data. A frequently reused table, kernel, or model weight may be evicted by unrelated memory traffic and then fetched again later.

## What Is Way Locking?

**Way locking reserves one or more cache ways so normal cache replacement cannot use them.**

In a four-way cache, reserving one way can be visualized as:

| Set | Way 0 | Way 1 | Way 2 | Way 3 |
|---:|---|---|---|---|
| 0 | Local memory | Cache | Cache | Cache |
| 1 | Local memory | Cache | Cache | Cache |
| 2 | Local memory | Cache | Cache | Cache |
| ... | ... | ... | ... | ... |

Notice that a way extends across **every set**. Locking one complete way reserves one line-sized slot in each set. It does not normally mean locking a single entry in one chosen set.

The capacity represented by one way is:

$$
\boxed{\text{capacity per way} = \text{number of sets} \times \text{line size}}
$$

For the earlier 64 KiB, four-way example:

$$
256 \times 64 = 16\ \text{KiB per way}
$$

Reserving one way therefore creates 16 KiB of protected space and leaves 48 KiB for ordinary caching.

## Way Locking as Local Memory

Some designs use way locking to turn part of a cache into **software-managed local memory**.

The difference is who decides what remains there:

- In an ordinary cache, hardware automatically fills and replaces lines.
- In local memory, software explicitly decides what data to place and reuse.

This creates a hybrid structure:

```text
One physical cache structure
┌──────────────────────┬──────────────────────────────┐
│ Software-managed LM  │ Hardware-managed cache       │
│ reserved ways        │ remaining ways               │
└──────────────────────┴──────────────────────────────┘
```

It can be useful for repeatedly accessed data such as:

- a hot lookup table;
- a small kernel or instruction sequence;
- frequently reused tensor tiles;
- model weights for the current operation;
- a hot expert in a mixture-of-experts model;
- data whose access time must be predictable.

## Interpreting an `nWaysLM` Register

Consider a 32-bit control register described like this:

```text
Way locking to create dedicated cache space for Local Memory

Register value:
{ Unused(31:lg(nWays)), nWaysLM(lg2(nWays)-1:0) }

Supported values:
nWaysLM = 0 to nWays-1
```

The important field is `nWaysLM`: the **number of ways assigned to local memory**. The upper register bits are unused.

Assume the cache has four ways:

$$
nWays = 4
$$

The field needs two bits because:

$$
\log_2(4) = 2
$$

The register can then be understood as:

```text
Bit 31                                  Bit 2  Bit 1 Bit 0
┌────────────────────────────────────────────┬───────────┐
│                  Unused                    │  nWaysLM  │
└────────────────────────────────────────────┴───────────┘
```

The supported values are:

| `nWaysLM` | Binary value | Local-memory ways | Normal cache ways |
|---:|---:|---:|---:|
| 0 | `00` | 0 | 4 |
| 1 | `01` | 1 | 3 |
| 2 | `10` | 2 | 2 |
| 3 | `11` | 3 | 1 |

If the cache is 64 KiB, each way contributes 16 KiB:

| `nWaysLM` | Local-memory capacity | Normal cache capacity |
|---:|---:|---:|
| 0 | 0 KiB | 64 KiB |
| 1 | 16 KiB | 48 KiB |
| 2 | 32 KiB | 32 KiB |
| 3 | 48 KiB | 16 KiB |

The register contains a **count**, not a set identifier. There is no field selecting a particular set. The natural interpretation is therefore that the chosen number of ways is reserved across all sets.

The excerpt alone does not specify which numbered ways are selected. Hardware might reserve the lowest-numbered ways, the highest-numbered ways, or use an internal convention described elsewhere.

## Can One Entry in a Specific Set Be Locked?

It is technically possible, but it requires different hardware support.

One cache entry is identified by a set and a way:

$$
\text{cache entry} = (\text{set},\ \text{way})
$$

To lock only Set 10, Way 2, the cache could store a lock bit with each entry:

```text
Set 10, Way 2
  valid  = 1
  dirty  = 0
  locked = 1
  tag    = ...
```

That mechanism is generally called **cache-line locking** rather than complete-way locking. Other designs may allow software to lock an address range and let the hardware determine the affected sets and ways.

An `nWaysLM` count alone cannot express a specific set, line, or address range.

## Can All Ways Be Locked?

The answer is architecture-specific. A hardware design could allow it if ordinary accesses bypass the cache or if the entire structure can operate as local memory.

However, the example register explicitly supports only:

$$
nWaysLM = 0\ \text{to}\ nWays - 1
$$

For a four-way cache, the largest supported value is three. At least one way must remain available for normal caching.

This avoids a case in which a new block maps to a set where every entry is unavailable to the normal replacement mechanism.

## Can Ways Be Unlocked Again?

Usually, yes—if the register is writable by software. Setting `nWaysLM` to a smaller value returns capacity to the normal cache. The exact behavior depends on the cache controller.

Three operations must not be confused:

- **Unlock:** make an entry or way eligible for normal replacement.
- **Invalidate:** mark its cached contents as absent.
- **Write back:** copy modified, or *dirty*, contents to the lower memory level.

Before changing the partition, software may need to write back or invalidate affected lines. The required ordering and privilege level must come from the hardware specification.

## Benefits and Costs

Way locking is a trade-off, not an automatic optimization.

### Potential benefits

- Important data cannot be evicted by unrelated cache traffic.
- Repeated lower-memory transfers can be reduced.
- Access time can become more predictable.
- Software can deliberately manage a small, fast working set.

### Potential costs

- The ordinary cache becomes smaller.
- Remaining cache ways experience more conflicts.
- Reserved capacity is wasted if the locked data is not reused.
- Software must manage placement, synchronization, and lifetime correctly.
- Reconfiguring ways may require write-back and invalidation operations.

For example, reserving three ways of a four-way cache gives software most of the capacity, but leaves the hardware-managed cache with only one line per set. That can substantially increase conflict misses.

## A Compact Mental Model

Think of the cache as a parking lot:

- **Sets** are rows.
- **Ways** are parking spaces in every row.
- A memory block is assigned to one row by its address.
- It may occupy any available space in that row.
- If the row is full, one car must leave—an eviction.
- Way locking reserves the same-position space across every row.

The key idea is:

> Way locking trades general-purpose cache capacity for protected, predictable storage.

Once cache lines, sets, and ways are clear, the mechanism is no longer mysterious: it is simply a configurable partition of the cache’s existing line slots.
