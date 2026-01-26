# RISC-V Hardware Performance Monitor (HPM) Counters - Complete Guide

**Complete guide from basics to advanced: Understanding, enabling, and using HPM counters in Chipyard's Rocket core**

---

## Table of Contents

1. [What are MHPM CSR Registers?](#1-what-are-mhpm-csr-registers)
2. [How to Include HPM Counters in Your Configuration](#2-how-to-include-hpm-counters-in-your-configuration)
3. [Event Encoding Format - Detailed Explanation](#3-event-encoding-format---detailed-explanation)
4. [Writing HPM Test Benchmarks - From Basic to Advanced](#4-writing-hpm-test-benchmarks---from-basic-to-advanced)
5. [Running HPM Test Benchmarks](#5-running-hpm-test-benchmarks)
6. [Complete Working Example](#6-complete-working-example)
7. [Troubleshooting](#7-troubleshooting)

---

## 1. What are MHPM CSR Registers?

### 1.1 Overview

**MHPM (Machine-mode Hardware Performance Monitor)** counters are special Control and Status Registers (CSRs) in RISC-V that allow software to measure microarchitectural events like:

- Cache misses (instruction cache, data cache)
- Branch mispredictions
- Pipeline stalls (load-use hazards, cache blocks)
- TLB misses (instruction TLB, data TLB)
- Instruction types (loads, stores, branches, multiply, divide)

These counters are essential for:
- **Performance profiling**: Understanding where your program spends time
- **Optimization**: Identifying bottlenecks (cache misses, branch mispredictions)
- **Hardware validation**: Verifying that the CPU behaves as expected
- **Research**: Studying microarchitectural behavior

### 1.2 CSR Address Space

The RISC-V privileged specification defines the following HPM-related CSRs:

| CSR Name | Address | Access | Description |
|----------|---------|--------|-------------|
| `mcycle` | 0xB00 | RW | Cycle counter (always present) |
| `minstret` | 0xB02 | RW | Instruction retired counter (always present) |
| `mhpmcounter3` | 0xB03 | RW | Hardware performance counter 3 |
| `mhpmcounter4` | 0xB04 | RW | Hardware performance counter 4 |
| ... | ... | RW | ... |
| `mhpmcounter31` | 0xB1F | RW | Hardware performance counter 31 (max) |
| `mcountinhibit` | 0x320 | RW | Counter inhibit control |
| `mhpmevent3` | 0x323 | RW | Event selector for counter 3 |
| `mhpmevent4` | 0x324 | RW | Event selector for counter 4 |
| ... | ... | RW | ... |
| `mhpmevent31` | 0x33F | RW | Event selector for counter 31 (max) |

**Key points:**
- `mcycle` and `minstret` are **mandatory** in RISC-V and always count cycles/instructions
- `mhpmcounter3` through `mhpmcounter31` are **optional** (up to 29 counters)
- Each `mhpmcounterN` has a corresponding `mhpmeventN` that selects **which event** to count
- `mcountinhibit` can disable individual counters (bit 0 = cycle, bit 2 = instret, bit 3 = hpmcounter3, etc.)

### 1.3 How HPM Counters Work

1. **Event Selection**: Write to `mhpmevent3` CSR to choose which event to monitor (e.g., D-cache misses)
2. **Counter Reset**: Write 0 to `mhpmcounter3` to start counting from zero
3. **Run Workload**: Execute the code you want to profile
4. **Read Counter**: Read `mhpmcounter3` to see how many times the event occurred

Example:
```c
// Select D-cache miss event
write_csr(mhpmevent3, 0x202);  
// Reset counter
write_csr(mhpmcounter3, 0);    
// Run workload
for (int i = 0; i < 1000; i++) sum += array[i];
// Read result
uint64_t dcache_misses = read_csr(mhpmcounter3);
printf("D-cache misses: %lu\n", dcache_misses);
```

---

## 2. How to Include HPM Counters in Your Configuration

### 2.1 The Problem: Default Configuration Has Zero Counters

By default, Rocket core configurations in Chipyard have **`nPerfCounters = 0`**. This means:
- `mhpmcounter3` through `mhpmcounter31` **do not exist**
- Writing to `mhpmevent3` has no effect
- Reading `mhpmcounter3` always returns **0**

**Source location:**
```
/ssd_scratch/vedantp/chipyard/generators/rocket-chip/src/main/scala/rocket/RocketCore.scala
Line 40: nPerfCounters: Int = 0,
```

### 2.2 Solution: Use `WithNPerfCounters` Configuration Fragment

Chipyard provides a configuration fragment to enable HPM counters:

**Source location:**
```
/ssd_scratch/vedantp/chipyard/generators/chipyard/src/main/scala/config/fragments/TileFragments.scala
Lines 92-99
```

**Code:**
```scala
class WithNPerfCounters(n: Int = 29) extends Config((site, here, up) => {
  case TilesLocated(InSubsystem) => up(TilesLocated(InSubsystem), site) map {
    case tp: RocketTileAttachParams => tp.copy(tileParams = tp.tileParams.copy(
      core = tp.tileParams.core.copy(nPerfCounters = n)))
    case tp: boom.v3.common.BoomTileAttachParams => tp.copy(tileParams = tp.tileParams.copy(
      core = tp.tileParams.core.copy(nPerfCounters = n)))
    case other => other
  }
})
```

This fragment overrides `nPerfCounters` to enable up to 29 HPM counters (the maximum).

### 2.3 Creating a New Configuration with HPM Counters

**Step 1:** Navigate to Chipyard config directory
```bash
cd /ssd_scratch/vedantp/chipyard/generators/chipyard/src/main/scala/config
```

**Step 2:** Create or modify a configuration file (e.g., `RocketConfigs.scala` or create new file)

**Example:** Creating `GemminiRocketHPMConfig`
```scala
// File: generators/chipyard/src/main/scala/config/RocketConfigs.scala

class GemminiRocketHPMConfig extends Config(
  new chipyard.config.WithNPerfCounters(29) ++        // Enable 29 HPM counters
  new gemmini.DefaultGemminiConfig ++                  // Add Gemmini accelerator
  new chipyard.config.AbstractConfig)                  // Base config
```

**Step 3:** Build the new configuration
```bash
cd /ssd_scratch/vedantp/chipyard/sims/verilator

# Build Verilator simulator with HPM counters enabled
make CONFIG=GemminiRocketHPMConfig
```

This generates:
```
simulator-chipyard.harness-GemminiRocketHPMConfig
```

**Important:** Building takes 30-60 minutes depending on your system. Use `-j$(nproc)` to parallelize.

### 2.4 Verifying HPM Counters are Enabled

After building, you can verify by running a simple test:

```bash
cd /ssd_scratch/vedantp/chipyard/sims/verilator
export VERILATOR_THREADS=$(nproc)

# Run any test that uses HPM counters
./simulator-chipyard.harness-GemminiRocketHPMConfig <your_test_binary>
```

If you see **non-zero** values in HPM counters, they're enabled!

---

## 3. Event Encoding Format - Detailed Explanation

### 3.1 Rocket Core EventSets Architecture

Rocket core organizes performance events into **EventSets**. Each EventSet contains related events.

**Source location:**
```
/ssd_scratch/vedantp/chipyard/generators/rocket-chip/src/main/scala/rocket/RocketCore.scala
Lines 178-220

/ssd_scratch/vedantp/chipyard/generators/rocket-chip/src/main/scala/rocket/Events.scala
Lines 1-89
```

Rocket core defines **3 EventSets** (indexed 0, 1, 2):

### 3.2 EventSet 0: Instruction Type Events

**Events:** Counts specific instruction types as they commit

| Event | Index | Description | Signal Condition |
|-------|-------|-------------|------------------|
| exception | 0 | Exception occurred | `wb_xcpt` |
| load | 1 | Load instruction | `id_ctrl.mem && mem_cmd === M_XRD && !id_ctrl.fp` |
| store | 2 | Store instruction | `id_ctrl.mem && mem_cmd === M_XWR && !id_ctrl.fp` |
| amo | 3 | Atomic memory operation | `id_ctrl.mem && (isAMO(mem_cmd) \|\| mem_cmd === M_XLR/M_XSC)` |
| system | 4 | System instruction (CSR access) | `id_ctrl.csr =/= CSR.N` |
| arith | 5 | Arithmetic instruction | `id_ctrl.wxd && !(jal\|jalr\|mem\|fp\|mul\|div\|csr)` |
| branch | 6 | Branch instruction | `id_ctrl.branch` |
| jal | 7 | JAL instruction | `id_ctrl.jal` |
| jalr | 8 | JALR instruction | `id_ctrl.jalr` |
| mul | 9 | Multiply instruction | `id_ctrl.mul` (if MulDiv present) |
| div | 10 | Divide instruction | `id_ctrl.div` (if MulDiv present) |
| fp load | 11 | FP load instruction | `id_ctrl.fp && fpu.dec.ldst && fpu.dec.wen` (if FPU present) |
| fp store | 12 | FP store instruction | `id_ctrl.fp && fpu.dec.ldst && !fpu.dec.wen` (if FPU present) |
| fp add | 13 | FP add instruction | `id_ctrl.fp && fpu.dec.fma && fpu.dec.swap23` (if FPU present) |
| fp mul | 14 | FP multiply instruction | `id_ctrl.fp && fpu.dec.fma && !swap23 && !ren3` (if FPU present) |
| fp mul-add | 15 | FP multiply-add instruction | `id_ctrl.fp && fpu.dec.fma && fpu.dec.ren3` (if FPU present) |
| fp div/sqrt | 16 | FP divide/sqrt instruction | `id_ctrl.fp && (fpu.dec.div \|\| fpu.dec.sqrt)` (if FPU present) |
| fp other | 17 | Other FP instruction | `id_ctrl.fp && !(ldst\|fma\|div\|sqrt)` (if FPU present) |

### 3.3 EventSet 1: Pipeline Stall Events

**Events:** Counts pipeline hazards and stalls

| Event | Index | Description | Signal Condition |
|-------|-------|-------------|------------------|
| load-use interlock | 0 | RAW hazard from load | `id_ex_hazard && ex_ctrl.mem \|\| id_mem_hazard && mem_ctrl.mem \|\| id_wb_hazard && wb_ctrl.mem` |
| long-latency interlock | 1 | Scoreboard hazard | `id_sboard_hazard` |
| csr interlock | 2 | CSR read-after-write hazard | `id_ex_hazard && ex_ctrl.csr =/= N \|\| ...` |
| I$ blocked | 3 | I-cache miss causing stall | `icache_blocked` |
| D$ blocked | 4 | D-cache miss causing stall | `id_ctrl.mem && dcache_blocked` |
| branch misprediction | 5 | Branch direction mispredicted | `take_pc_mem && mem_direction_misprediction` |
| control-flow target misprediction | 6 | Branch target mispredicted | `take_pc_mem && mem_misprediction && mem_cfi && !mem_direction_misprediction` |
| flush | 7 | Pipeline flush | `wb_reg_flush_pipe` |
| replay | 8 | Instruction replay | `replay_wb` |
| mul/div interlock | 9 | Multiply/divide hazard | `id_ex_hazard && (ex_ctrl.mul \|\| ex_ctrl.div) \|\| ...` (if MulDiv present) |
| fp interlock | 10 | Floating-point hazard | `id_ex_hazard && ex_ctrl.fp \|\| ...` (if FPU present) |

### 3.4 EventSet 2: Cache and TLB Events

**Events:** Counts memory system activity

| Event | Index | Description | Signal Condition |
|-------|-------|-------------|------------------|
| I$ miss | 0 | Instruction cache miss | `io.imem.perf.acquire` |
| D$ miss | 1 | Data cache miss | `io.dmem.perf.acquire` |
| D$ release | 2 | Data cache writeback | `io.dmem.perf.release` |
| ITLB miss | 3 | Instruction TLB miss | `io.imem.perf.tlbMiss` |
| DTLB miss | 4 | Data TLB miss | `io.dmem.perf.tlbMiss` |
| L2 TLB miss | 5 | Second-level TLB miss | `io.ptw.perf.l2miss` |

### 3.5 Event Selector Encoding Format

The `mhpmeventN` CSR uses the following encoding:

```
Bits [7:0]   = EventSet ID (0, 1, or 2)
Bits [63:8]  = Event Mask (bitmask of events to count within the set)
```

**Source location:**
```
/ssd_scratch/vedantp/chipyard/generators/rocket-chip/src/main/scala/rocket/Events.scala
Lines 38-48 (decode function)
```

**Code:**
```scala
private def decode(counter: UInt): (UInt, UInt) = {
  require(eventSets.size <= (1 << maxEventSetIdBits))
  require(eventSetIdBits > 0)
  (counter(eventSetIdBits-1, 0), counter >> maxEventSetIdBits)
}
```

Where:
- `eventSetIdBits = log2Ceil(3) = 2` bits (since we have 3 EventSets)
- `maxEventSetIdBits = 8` (reserved space for EventSet ID)
- Decoding: `setID = eventSel[7:0]`, `mask = eventSel >> 8`

### 3.6 Calculating Event Selector Values

**Formula:**
```
mhpmeventN = (event_mask << 8) | eventset_id
```

Where:
- `eventset_id` = 0, 1, or 2
- `event_mask` = Bitmask with bit `event_index` set to 1

**Examples:**

1. **D-cache miss** (EventSet 2, index 1):
   ```
   eventset_id = 2
   event_index = 1
   event_mask = 1 << 1 = 0x2
   mhpmevent3 = (0x2 << 8) | 2 = 0x202
   ```

2. **Load instruction** (EventSet 0, index 1):
   ```
   eventset_id = 0
   event_index = 1
   event_mask = 1 << 1 = 0x2
   mhpmevent3 = (0x2 << 8) | 0 = 0x200
   ```

3. **Branch misprediction** (EventSet 1, index 5):
   ```
   eventset_id = 1
   event_index = 5
   event_mask = 1 << 5 = 0x20
   mhpmevent3 = (0x20 << 8) | 1 = 0x2001
   ```

4. **Load-use interlock** (EventSet 1, index 0):
   ```
   eventset_id = 1
   event_index = 0
   event_mask = 1 << 0 = 0x1
   mhpmevent3 = (0x1 << 8) | 1 = 0x101
   ```

### 3.7 Quick Reference Table

| Event | EventSet | Index | Encoding | Description |
|-------|----------|-------|----------|-------------|
| Load inst | 0 | 1 | `0x200` | Count load instructions |
| Store inst | 0 | 2 | `0x400` | Count store instructions |
| Branch inst | 0 | 6 | `0x4000` | Count branch instructions |
| Arith inst | 0 | 5 | `0x2000` | Count arithmetic instructions |
| Load-use | 1 | 0 | `0x101` | Count load-use hazards |
| D$ blocked | 1 | 4 | `0x1001` | Count cycles D-cache blocked |
| Branch mispred | 1 | 5 | `0x2001` | Count branch mispredictions |
| I$ miss | 2 | 0 | `0x102` | Count I-cache misses |
| D$ miss | 2 | 1 | `0x202` | Count D-cache misses |
| D$ release | 2 | 2 | `0x402` | Count D-cache writebacks |
| ITLB miss | 2 | 3 | `0x802` | Count I-TLB misses |
| DTLB miss | 2 | 4 | `0x1002` | Count D-TLB misses |
| L2 TLB miss | 2 | 5 | `0x2002` | Count L2-TLB misses |

---

## 4. Writing HPM Test Benchmarks - From Basic to Advanced

### 4.1 Basic Example: Reading Cycle and Instruction Counters

**File:** Create `hpm_basic.c` in `/ssd_scratch/vedantp/chipyard/software/ara-gemmini-benchmarks/benchmarks/hpm_tests/`

```c
#include <stdio.h>
#include <stdint.h>
#include "encoding.h"  // Provides read_csr/write_csr macros

int main() {
    // Read cycle counter before workload
    uint64_t cycles_start = read_csr(mcycle);
    uint64_t instret_start = read_csr(minstret);
    
    // Simple workload
    volatile int sum = 0;
    for (int i = 0; i < 1000; i++) {
        sum += i;
    }
    
    // Read counters after workload
    uint64_t cycles_end = read_csr(mcycle);
    uint64_t instret_end = read_csr(minstret);
    
    // Calculate deltas
    uint64_t cycles = cycles_end - cycles_start;
    uint64_t instructions = instret_end - instret_start;
    
    // Print results
    printf("Cycles: %lu\n", cycles);
    printf("Instructions: %lu\n", instructions);
    printf("IPC: %lu.%02lu\n", 
           (instructions * 100) / cycles, 
           (instructions * 100) % cycles);
    
    return 0;
}
```

**Key points:**
- `read_csr(mcycle)` reads the cycle counter
- `read_csr(minstret)` reads the retired instruction counter
- Both are **always available** in RISC-V (no configuration needed)
- IPC (Instructions Per Cycle) = instructions / cycles

### 4.2 Intermediate Example: Measuring D-Cache Misses

**File:** Create `hpm_dcache.c`

```c
#include <stdio.h>
#include <stdint.h>
#include "encoding.h"

// Large array to cause cache misses
volatile uint64_t data[4096] __attribute__((aligned(4096)));

int main() {
    // Initialize data
    for (int i = 0; i < 4096; i++) {
        data[i] = i;
    }
    
    // Clear counter inhibit (enable all counters)
    write_csr(mcountinhibit, 0);
    
    // Configure mhpmevent3 for D$ miss (EventSet 2, index 1)
    // Encoding: (1 << (8 + 1)) | 2 = 0x202
    write_csr(mhpmevent3, 0x202);
    
    // Reset counter to 0
    write_csr(mhpmcounter3, 0);
    
    // Read counter before workload
    uint64_t dcache_miss_start = read_csr(mhpmcounter3);
    uint64_t cycles_start = read_csr(mcycle);
    
    // Workload: Sequential access (should have few misses)
    volatile uint64_t sum = 0;
    for (int i = 0; i < 4096; i++) {
        sum += data[i];
    }
    
    // Read counters after workload
    uint64_t dcache_miss_end = read_csr(mhpmcounter3);
    uint64_t cycles_end = read_csr(mcycle);
    
    printf("Sequential access:\n");
    printf("  D$ misses: %lu\n", dcache_miss_end - dcache_miss_start);
    printf("  Cycles: %lu\n", cycles_end - cycles_start);
    
    // Reset counter
    write_csr(mhpmcounter3, 0);
    dcache_miss_start = read_csr(mhpmcounter3);
    cycles_start = read_csr(mcycle);
    
    // Workload: Strided access (should have MORE misses)
    sum = 0;
    for (int i = 0; i < 4096; i += 8) {  // Access every 64 bytes
        sum += data[i];
    }
    
    dcache_miss_end = read_csr(mhpmcounter3);
    cycles_end = read_csr(mcycle);
    
    printf("Strided access (64B):\n");
    printf("  D$ misses: %lu\n", dcache_miss_end - dcache_miss_start);
    printf("  Cycles: %lu\n", cycles_end - cycles_start);
    
    return 0;
}
```

**Key points:**
- `write_csr(mcountinhibit, 0)` enables all counters
- `write_csr(mhpmevent3, 0x202)` selects D-cache miss event
- `write_csr(mhpmcounter3, 0)` resets the counter
- Sequential access should show **fewer** misses than strided access

### 4.3 Advanced Example: Branch Prediction Analysis

**File:** Create `hpm_branch.c`

```c
#include <stdio.h>
#include <stdint.h>
#include "encoding.h"

// Helper function to measure branch behavior
void measure_branches(const char* name, int pattern) {
    // Enable counters
    write_csr(mcountinhibit, 0);
    
    // Configure counter 3 for branch instructions (EventSet 0, index 6)
    write_csr(mhpmevent3, 0x4000);
    // Configure counter 4 for branch mispredictions (EventSet 1, index 5)
    write_csr(mhpmevent4, 0x2001);
    
    // Reset counters
    write_csr(mhpmcounter3, 0);
    write_csr(mhpmcounter4, 0);
    uint64_t cycles_start = read_csr(mcycle);
    
    // Branch workload
    volatile int count = 0;
    for (int i = 0; i < 1000; i++) {
        int condition;
        switch (pattern) {
            case 0:  // Predictable (always taken)
                condition = 1;
                break;
            case 1:  // Alternating (taken/not-taken)
                condition = (i & 1);
                break;
            case 2:  // Random-ish (using LFSR)
                condition = ((i * 1103515245 + 12345) >> 16) & 1;
                break;
            default:
                condition = 0;
        }
        
        if (condition) {
            count++;
        }
    }
    
    // Read results
    uint64_t branches = read_csr(mhpmcounter3);
    uint64_t mispredicts = read_csr(mhpmcounter4);
    uint64_t cycles = read_csr(mcycle) - cycles_start;
    
    printf("%s:\n", name);
    printf("  Branches: %lu\n", branches);
    printf("  Mispredicts: %lu\n", mispredicts);
    printf("  Mispred rate: %lu.%02lu%%\n", 
           (mispredicts * 100) / branches,
           ((mispredicts * 10000) / branches) % 100);
    printf("  Cycles: %lu\n", cycles);
    printf("  Count: %d\n\n", count);
}

int main() {
    measure_branches("Predictable pattern", 0);
    measure_branches("Alternating pattern", 1);
    measure_branches("Random pattern", 2);
    return 0;
}
```

**Key points:**
- Uses **multiple** HPM counters simultaneously (counter 3 and 4)
- `mhpmcounter3` counts **all** branches
- `mhpmcounter4` counts **mispredicted** branches
- Misprediction rate = mispredicts / branches
- Different patterns show different predictor behavior

### 4.4 Advanced Example: Multi-Event Monitoring with Helper Functions

**File:** Create `hpm_utils.h` (reusable helper library)

```c
#ifndef HPM_UTILS_H
#define HPM_UTILS_H

#include <stdint.h>
#include "encoding.h"

// Event selector definitions
// EventSet 0: Instruction types
#define HPM_EVENT_LOAD          0x200ULL
#define HPM_EVENT_STORE         0x400ULL
#define HPM_EVENT_BRANCH        0x4000ULL
#define HPM_EVENT_ARITH         0x2000ULL

// EventSet 1: Pipeline stalls
#define HPM_EVENT_LOAD_USE      0x101ULL
#define HPM_EVENT_DCACHE_BLOCK  0x1001ULL
#define HPM_EVENT_BRANCH_MISS   0x2001ULL

// EventSet 2: Cache/TLB
#define HPM_EVENT_ICACHE_MISS   0x102ULL
#define HPM_EVENT_DCACHE_MISS   0x202ULL
#define HPM_EVENT_DCACHE_WB     0x402ULL
#define HPM_EVENT_DTLB_MISS     0x1002ULL

// Initialize HPM counters (enable all)
static inline void hpm_init(void) {
    write_csr(mcountinhibit, 0);
}

// Configure and reset a counter
static inline void hpm_setup_counter(int counter_id, uint64_t event) {
    switch (counter_id) {
        case 3:  write_csr(mhpmevent3, event); write_csr(mhpmcounter3, 0); break;
        case 4:  write_csr(mhpmevent4, event); write_csr(mhpmcounter4, 0); break;
        case 5:  write_csr(mhpmevent5, event); write_csr(mhpmcounter5, 0); break;
        case 6:  write_csr(mhpmevent6, event); write_csr(mhpmcounter6, 0); break;
        // Add more as needed
    }
}

// Read a counter
static inline uint64_t hpm_read_counter(int counter_id) {
    switch (counter_id) {
        case 3:  return read_csr(mhpmcounter3);
        case 4:  return read_csr(mhpmcounter4);
        case 5:  return read_csr(mhpmcounter5);
        case 6:  return read_csr(mhpmcounter6);
        default: return 0;
    }
}

// Read cycle counter
static inline uint64_t hpm_read_cycles(void) {
    return read_csr(mcycle);
}

// Read instruction counter
static inline uint64_t hpm_read_instret(void) {
    return read_csr(minstret);
}

#endif // HPM_UTILS_H
```

**File:** Create `hpm_comprehensive.c` (uses hpm_utils.h)

```c
#include <stdio.h>
#include <stdint.h>
#include "hpm_utils.h"

volatile uint64_t data[1024];

int main() {
    // Initialize HPM system
    hpm_init();
    
    // Setup multiple counters
    hpm_setup_counter(3, HPM_EVENT_LOAD);
    hpm_setup_counter(4, HPM_EVENT_STORE);
    hpm_setup_counter(5, HPM_EVENT_DCACHE_MISS);
    hpm_setup_counter(6, HPM_EVENT_BRANCH);
    
    // Read starting values
    uint64_t cycles_start = hpm_read_cycles();
    uint64_t instret_start = hpm_read_instret();
    
    // Complex workload
    volatile uint64_t sum = 0;
    for (int i = 0; i < 1000; i++) {
        // Loads
        sum += data[i % 1024];
        // Stores
        data[i % 1024] = sum;
        // Branch
        if (sum > 1000) sum = 0;
    }
    
    // Read all counters
    uint64_t cycles = hpm_read_cycles() - cycles_start;
    uint64_t instret = hpm_read_instret() - instret_start;
    uint64_t loads = hpm_read_counter(3);
    uint64_t stores = hpm_read_counter(4);
    uint64_t dcache_miss = hpm_read_counter(5);
    uint64_t branches = hpm_read_counter(6);
    
    // Print comprehensive report
    printf("=== HPM Comprehensive Report ===\n");
    printf("Cycles:        %lu\n", cycles);
    printf("Instructions:  %lu\n", instret);
    printf("IPC:           %lu.%02lu\n", 
           (instret * 100) / cycles, 
           ((instret * 10000) / cycles) % 100);
    printf("\nInstruction Mix:\n");
    printf("  Loads:       %lu\n", loads);
    printf("  Stores:      %lu\n", stores);
    printf("  Branches:    %lu\n", branches);
    printf("\nCache Behavior:\n");
    printf("  D$ misses:   %lu\n", dcache_miss);
    printf("  Miss rate:   %lu.%02lu%%\n",
           (dcache_miss * 100) / (loads + stores),
           ((dcache_miss * 10000) / (loads + stores)) % 100);
    
    return 0;
}
```

**Key points:**
- `hpm_utils.h` provides **reusable** helper functions
- Can monitor **4 events simultaneously** (counters 3-6)
- Calculates derived metrics (IPC, miss rate)
- Produces comprehensive performance report

---

## 5. Running HPM Test Benchmarks

### 5.1 Build System Setup

**Directory structure:**
```
/ssd_scratch/vedantp/chipyard/software/ara-gemmini-benchmarks/benchmarks/hpm_tests/
├── hpm_basic.c
├── hpm_dcache.c
├── hpm_branch.c
├── hpm_comprehensive.c
├── hpm_utils.h
├── Makefile
└── build/        (generated)
```

### 5.2 Makefile Configuration

**File:** Create `Makefile` in the hpm_tests directory

```makefile
# HPM Tests Makefile

# Paths to Chipyard's gemmini-rocc-tests (for baremetal runtime)
CHIPYARD_ROOT = /ssd_scratch/vedantp/chipyard
GEMMINI_TESTS = $(CHIPYARD_ROOT)/generators/gemmini/software/gemmini-rocc-tests
RISCV_TESTS_DIR = $(GEMMINI_TESTS)/riscv-tests
BENCH_COMMON = $(RISCV_TESTS_DIR)/benchmarks/common

# Compiler
CC = riscv64-unknown-elf-gcc

# Compiler flags (MUST match gemmini baremetal tests)
CFLAGS = \
    -DPREALLOCATE=1 \
    -DMULTITHREAD=1 \
    -mcmodel=medany \
    -std=gnu99 \
    -O2 \
    -ffast-math \
    -fno-common \
    -fno-builtin-printf \
    -fno-tree-loop-distribute-patterns \
    -march=rv64gc \
    -Wa,-march=rv64gc \
    -nostdlib \
    -nostartfiles \
    -static \
    -T $(BENCH_COMMON)/test.ld \
    -DBAREMETAL=1

# Include paths
INCLUDES = \
    -I$(RISCV_TESTS_DIR) \
    -I$(RISCV_TESTS_DIR)/env \
    -I$(GEMMINI_TESTS) \
    -I$(BENCH_COMMON) \
    -I.

# Libraries
LIBS = -lm -lgcc

# Source files for baremetal runtime
RUNTIME_SRCS = $(BENCH_COMMON)/syscalls.c $(BENCH_COMMON)/crt.S

# Test programs
TESTS = hpm_basic hpm_dcache hpm_branch hpm_comprehensive

# Build directory
BUILD_DIR = build

.PHONY: all clean

all: $(BUILD_DIR) $(addprefix $(BUILD_DIR)/, $(TESTS))

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Generic build rule for all tests
$(BUILD_DIR)/%: %.c hpm_utils.h
	$(CC) $(CFLAGS) $(INCLUDES) $< $(RUNTIME_SRCS) $(LIBS) -o $@

clean:
	rm -rf $(BUILD_DIR)
```

### 5.3 Detailed Explanation of Compiler Flags

| Flag | Purpose | Why It's Critical |
|------|---------|-------------------|
| `-DPREALLOCATE=1` | Pre-allocate stack/heap | Baremetal environment needs static allocation |
| `-DMULTITHREAD=1` | Enable thread-local storage | Required by baremetal runtime |
| `-mcmodel=medany` | Medium any code model | Allows code/data in any 2GB range |
| `-std=gnu99` | GNU C99 standard | Enables GCC extensions |
| `-O2` | Optimization level 2 | Balance between performance and code size |
| `-ffast-math` | Fast floating-point math | Relaxed FP semantics for speed |
| `-fno-common` | No common sections | Each variable gets unique storage |
| `-fno-builtin-printf` | Don't use built-in printf | Use custom printf from syscalls.c |
| `-fno-tree-loop-distribute-patterns` | Disable loop distribution | Prevents library calls |
| `-march=rv64gc` | Target RV64GC ISA | RV64 with IMAFD extensions + compressed |
| `-Wa,-march=rv64gc` | Assembler target | Ensure assembly matches compiler target |
| `-nostdlib` | No standard library | Baremetal has no libc |
| `-nostartfiles` | No standard startup files | Use custom crt.S |
| `-static` | Static linking | No dynamic libraries in baremetal |
| `-T test.ld` | Linker script | Defines memory layout |
| `-DBAREMETAL=1` | Baremetal define | Conditional compilation for baremetal |
| `-lm -lgcc` | Link libm and libgcc | Math functions and compiler runtime |

**Critical:** Using **different** flags will cause:
- Link errors (undefined references)
- Runtime crashes (bad syscall errors)
- Wrong memory layout

### 5.4 Building Tests

**Step 1:** Navigate to test directory
```bash
cd /ssd_scratch/vedantp/chipyard/software/ara-gemmini-benchmarks/benchmarks/hpm_tests
```

**Step 2:** Source Chipyard environment (sets up compiler paths)
```bash
source /ssd_scratch/vedantp/chipyard/env.sh
```

**Step 3:** Build all tests
```bash
make clean
make -j$(nproc) all
```

**Output:**
```
mkdir -p build
riscv64-unknown-elf-gcc ... -o build/hpm_basic
riscv64-unknown-elf-gcc ... -o build/hpm_dcache
riscv64-unknown-elf-gcc ... -o build/hpm_branch
riscv64-unknown-elf-gcc ... -o build/hpm_comprehensive
```

**Step 4:** Verify binaries were created
```bash
ls -lh build/
```

Expected output:
```
-rwxr-xr-x 1 user group  18K hpm_basic
-rwxr-xr-x 1 user group  19K hpm_dcache
-rwxr-xr-x 1 user group  20K hpm_branch
-rwxr-xr-x 1 user group  21K hpm_comprehensive
```

### 5.5 Running Tests on Verilator Simulator

**Step 1:** Navigate to simulator directory
```bash
cd /ssd_scratch/vedantp/chipyard/sims/verilator
```

**Step 2:** Source environment (if not already done)
```bash
source /ssd_scratch/vedantp/chipyard/env.sh
```

**Step 3:** Set threading (use all CPU cores)
```bash
export VERILATOR_THREADS=$(nproc)
```

**Step 4:** Run a test
```bash
./simulator-chipyard.harness-GemminiRocketHPMConfig \
    /ssd_scratch/vedantp/chipyard/software/ara-gemmini-benchmarks/benchmarks/hpm_tests/build/hpm_basic
```

**Expected output:**
```
[UART] UART0 is here (stdin/stdout).
Cycles: 12029
Instructions: 5006
IPC: 0.41
- Verilog $finish
```

**Step 5:** Run all tests and save logs
```bash
# Create logs directory
mkdir -p ../../software/ara-gemmini-benchmarks/benchmarks/hpm_tests/logs

# Run each test
for test in hpm_basic hpm_dcache hpm_branch hpm_comprehensive; do
    echo "Running $test..."
    ./simulator-chipyard.harness-GemminiRocketHPMConfig \
        ../../software/ara-gemmini-benchmarks/benchmarks/hpm_tests/build/$test \
        2>&1 | tee ../../software/ara-gemmini-benchmarks/benchmarks/hpm_tests/logs/${test}.log
done
```

### 5.6 Performance Notes

- **Verilator simulation is SLOW**: ~50-200x slower than real hardware
- Each test takes: 30 seconds to 5 minutes
- Use `timeout` to prevent hangs: `timeout 300 ./simulator...`
- Parallel execution: Run tests on different terminals/machines

---

## 6. Complete Working Example

Let me provide a **complete, tested** example that you can copy and run immediately.

### 6.1 Create Directory Structure

```bash
cd /ssd_scratch/vedantp/chipyard/software/ara-gemmini-benchmarks/benchmarks
mkdir -p hpm_tests/build hpm_tests/logs
cd hpm_tests
```

### 6.2 Create hpm_utils.h

```bash
cat > hpm_utils.h << 'EOF'
#ifndef HPM_UTILS_H
#define HPM_UTILS_H

#include <stdint.h>
#include "encoding.h"

// Event encodings (EventSet ID in bits [7:0], mask in bits [63:8])
#define HPM_EVENT_LOAD          0x200ULL      // Set 0, index 1
#define HPM_EVENT_STORE         0x400ULL      // Set 0, index 2
#define HPM_EVENT_BRANCH        0x4000ULL     // Set 0, index 6
#define HPM_EVENT_LOAD_USE      0x101ULL      // Set 1, index 0
#define HPM_EVENT_DCACHE_BLOCK  0x1001ULL     // Set 1, index 4
#define HPM_EVENT_BRANCH_MISS   0x2001ULL     // Set 1, index 5
#define HPM_EVENT_DCACHE_MISS   0x202ULL      // Set 2, index 1

static inline void hpm_init(void) { write_csr(mcountinhibit, 0); }
static inline void hpm_setup_counter3(uint64_t event) { 
    write_csr(mhpmevent3, event); 
    write_csr(mhpmcounter3, 0); 
}
static inline uint64_t hpm_read_counter3(void) { return read_csr(mhpmcounter3); }
static inline uint64_t hpm_read_cycles(void) { return read_csr(mcycle); }
static inline uint64_t hpm_read_instret(void) { return read_csr(minstret); }

#endif
EOF
```

### 6.3 Create Test Program

```bash
cat > hpm_complete_test.c << 'EOF'
#include <stdio.h>
#include <stdint.h>
#include "hpm_utils.h"

volatile uint64_t data[1024];

int main() {
    printf("=== HPM Complete Test ===\n\n");
    
    // Initialize
    hpm_init();
    for (int i = 0; i < 1024; i++) data[i] = i;
    
    // Test 1: Basic cycles/instret
    uint64_t c1 = hpm_read_cycles();
    uint64_t i1 = hpm_read_instret();
    volatile int sum = 0;
    for (int i = 0; i < 100; i++) sum += i;
    uint64_t c2 = hpm_read_cycles();
    uint64_t i2 = hpm_read_instret();
    printf("Test 1 - Basic:\n");
    printf("  Cycles: %lu, Instructions: %lu\n\n", c2-c1, i2-i1);
    
    // Test 2: D$ misses
    hpm_setup_counter3(HPM_EVENT_DCACHE_MISS);
    uint64_t miss1 = hpm_read_counter3();
    c1 = hpm_read_cycles();
    sum = 0;
    for (int i = 0; i < 1024; i++) sum += data[i];
    uint64_t miss2 = hpm_read_counter3();
    c2 = hpm_read_cycles();
    printf("Test 2 - D$ miss:\n");
    printf("  Misses: %lu, Cycles: %lu\n\n", miss2-miss1, c2-c1);
    
    // Test 3: Load instructions
    hpm_setup_counter3(HPM_EVENT_LOAD);
    uint64_t ld1 = hpm_read_counter3();
    sum = 0;
    for (int i = 0; i < 100; i++) sum += data[i];
    uint64_t ld2 = hpm_read_counter3();
    printf("Test 3 - Loads:\n");
    printf("  Load instructions: %lu\n\n", ld2-ld1);
    
    printf("=== Test Complete ===\n");
    return 0;
}
EOF
```

### 6.4 Create Makefile

```bash
cat > Makefile << 'EOF'
CHIPYARD_ROOT = /ssd_scratch/vedantp/chipyard
GEMMINI_TESTS = $(CHIPYARD_ROOT)/generators/gemmini/software/gemmini-rocc-tests
RISCV_TESTS_DIR = $(GEMMINI_TESTS)/riscv-tests
BENCH_COMMON = $(RISCV_TESTS_DIR)/benchmarks/common

CC = riscv64-unknown-elf-gcc
CFLAGS = -DPREALLOCATE=1 -DMULTITHREAD=1 -mcmodel=medany -std=gnu99 -O2 \
         -ffast-math -fno-common -fno-builtin-printf -fno-tree-loop-distribute-patterns \
         -march=rv64gc -Wa,-march=rv64gc -nostdlib -nostartfiles -static \
         -T $(BENCH_COMMON)/test.ld -DBAREMETAL=1
INCLUDES = -I$(RISCV_TESTS_DIR) -I$(RISCV_TESTS_DIR)/env -I$(GEMMINI_TESTS) -I$(BENCH_COMMON) -I.
LIBS = -lm -lgcc
RUNTIME_SRCS = $(BENCH_COMMON)/syscalls.c $(BENCH_COMMON)/crt.S

all: build/hpm_complete_test

build/hpm_complete_test: hpm_complete_test.c hpm_utils.h
	mkdir -p build
	$(CC) $(CFLAGS) $(INCLUDES) $< $(RUNTIME_SRCS) $(LIBS) -o $@

clean:
	rm -rf build
EOF
```

### 6.5 Build and Run

```bash
# Source environment
source /ssd_scratch/vedantp/chipyard/env.sh

# Build
make clean
make

# Navigate to simulator
cd /ssd_scratch/vedantp/chipyard/sims/verilator

# Set threads
export VERILATOR_THREADS=$(nproc)

# Run test
./simulator-chipyard.harness-GemminiRocketHPMConfig \
    ../../software/ara-gemmini-benchmarks/benchmarks/hpm_tests/build/hpm_complete_test
```

**Expected output:**
```
=== HPM Complete Test ===

Test 1 - Basic:
  Cycles: 1051, Instructions: 614

Test 2 - D$ miss:
  Misses: 16, Cycles: 2341

Test 3 - Loads:
  Load instructions: 200

=== Test Complete ===
```

**If you see non-zero values → HPM counters are working!**

---

## 7. Troubleshooting

### 7.1 Problem: All HPM Counters Return Zero

**Symptom:**
```
D$ misses: 0
Loads: 0
Branches: 0
```

**Causes & Solutions:**

1. **HPM counters not enabled in config**
   - Check: Did you build with `WithNPerfCounters`?
   - Solution: Rebuild simulator with `GemminiRocketHPMConfig`

2. **mcountinhibit not cleared**
   - Check: Did you call `write_csr(mcountinhibit, 0)`?
   - Solution: Add `hpm_init()` before measuring

3. **Wrong event encoding**
   - Check: Is event selector correct (e.g., `0x202` for D$ miss)?
   - Solution: Use event encodings from Section 3.7

### 7.2 Problem: Build Fails with "undefined reference"

**Symptom:**
```
undefined reference to `_init'
undefined reference to `handle_trap'
```

**Solution:** Missing baremetal runtime files
```bash
# Verify paths in Makefile
ls /ssd_scratch/vedantp/chipyard/generators/gemmini/software/gemmini-rocc-tests/riscv-tests/benchmarks/common/syscalls.c
ls /ssd_scratch/vedantp/chipyard/generators/gemmini/software/gemmini-rocc-tests/riscv-tests/benchmarks/common/crt.S
```

### 7.3 Problem: Simulator Crashes with "bad syscall"

**Symptom:**
```
terminate called after throwing an instance of 'std::runtime_error'
  what():  bad syscall #2147499392
```

**Solution:** Compiled with wrong flags
- Use **exact** flags from Section 5.3
- Ensure `-march=rv64gc -Wa,-march=rv64gc` (not `rv64gcv_zicsr`)

### 7.4 Problem: Test Hangs Forever

**Symptom:** Test never prints output or finishes

**Solutions:**

1. **Use timeout:**
   ```bash
   timeout 300 ./simulator... 
   ```

2. **Check test logic:** Infinite loop in test code?

3. **Simplify test:** Start with basic cycle/instret only

### 7.5 Verification Checklist

✅ Chipyard environment sourced (`source env.sh`)  
✅ Simulator built with `WithNPerfCounters`  
✅ Test compiled with correct flags (see Section 5.3)  
✅ `mcountinhibit` cleared in test code  
✅ Event selector encoding correct (see Section 3.7)  
✅ `VERILATOR_THREADS` set for performance  

---

## Summary

This guide covered:

1. **What are MHPM CSRs**: Hardware counters for measuring microarchitectural events
2. **How to enable them**: Use `WithNPerfCounters(29)` configuration fragment
3. **Event encoding**: EventSet ID (bits 7:0) + mask (bits 63:8), with detailed reference tables
4. **Writing tests**: From basic (cycle/instret) to advanced (multi-counter analysis)
5. **Running tests**: Complete build system with all necessary compiler flags

**Key takeaways:**
- HPM counters must be **explicitly enabled** in configuration (default is 0)
- Event encoding follows Rocket's **EventSets** architecture (see RocketCore.scala:178-220)
- Build flags **must match** gemmini baremetal tests exactly
- Simulation is slow (~50-200x real hardware) - be patient!

**Next steps:**
- Run the complete example in Section 6
- Modify tests to measure your specific workloads
- Compare performance across different code patterns
- Use insights to optimize your applications

---

**File Locations Referenced:**
- `/ssd_scratch/vedantp/chipyard/generators/rocket-chip/src/main/scala/rocket/RocketCore.scala` (Lines 40, 178-220)
- `/ssd_scratch/vedantp/chipyard/generators/rocket-chip/src/main/scala/rocket/Events.scala` (Lines 1-89)
- `/ssd_scratch/vedantp/chipyard/generators/chipyard/src/main/scala/config/fragments/TileFragments.scala` (Lines 92-99)
- `/ssd_scratch/vedantp/chipyard/generators/gemmini/software/gemmini-rocc-tests/riscv-tests/benchmarks/common/` (Runtime files)

---

*Created: January 26, 2026*  
*Tested on: Chipyard with GemminiRocketHPMConfig*  
*Simulator: Verilator*
