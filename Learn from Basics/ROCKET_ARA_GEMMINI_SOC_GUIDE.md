# Rocket + Ara + Gemmini SoC Architecture Guide

## Overview

This guide documents a **heterogeneous SoC configuration** combining three specialized compute engines on a single Rocket-based tile:
- **Rocket** - Scalar RISC-V core (RV64GC)
- **Ara** - Vector Processing Unit (RISC-V V extension)
- **Gemmini** - Systolic Array Matrix Accelerator

This configuration enables workloads that leverage scalar processing, vectorized SIMD operations, and matrix multiplication acceleration simultaneously.

---

## Architecture Description

### System Components

```
┌─────────────────────────────────────────────────────────┐
│                    Rocket Tile                          │
│  ┌──────────────┐                                       │
│  │   Rocket     │  (Scalar Core - RV64GC)               │
│  │   Core       │  - General purpose execution          │
│  └──────┬───────┘  - Control flow & memory ops          │
│         │                                                │
│    ┌────┴────────────────────────────┐                  │
│    │    RoCC Interface (Custom)      │                  │
│    └────┬──────────────────┬──────────┘                 │
│         │                  │                             │
│  ┌──────▼─────────┐ ┌─────▼──────────────┐             │
│  │  Ara Vector    │ │  Gemmini Systolic  │             │
│  │  Unit (RVV)    │ │  Array Accelerator │             │
│  │                │ │                    │             │
│  │ • vLen: 4096   │ │ • INT8/FP32 MAC   │             │
│  │ • Lanes: 2     │ │ • Scratchpad SPM  │             │
│  │ • 64-bit ELEN  │ │ • DMA engines     │             │
│  └────────────────┘ └────────────────────┘             │
└─────────────────────────────────────────────────────────┘
              │
      ┌───────▼────────────┐
      │  System Bus (TL)   │  (128-bit wide)
      └────────────────────┘
```

### Key Features

1. **Rocket Core**
   - Standard 5-stage pipeline
   - RV64GC ISA (Integer, Multiply, Compressed, FP)
   - L1 I$/D$ caches
   - RoCC interface for custom accelerators

2. **Ara Vector Unit**
   - **vLen**: 4096 bits (configurable: 4096/8192)
   - **Lanes**: 2 or 4 parallel execution lanes
   - **ELEN**: 64-bit element width
   - **VFLEN**: 64-bit FP vector width
   - Implements RISC-V V 1.0 extension
   - Early decode integration with Rocket

3. **Gemmini Accelerator**
   - **Type**: DefaultGemminiConfig (INT8 by default)
   - **Compute**: Systolic array for matrix ops (GEMM)
   - **Memory**: Scratchpad + accumulator banks
   - **DMA**: Dedicated engines for data movement
   - **Operations**: Matrix multiply, convolutions, activations

---

## Configuration Files

### Primary Config Definition

**Location**: [`generators/chipyard/src/main/scala/config/AraGemminiRocketConfigs.scala`](generators/chipyard/src/main/scala/config/AraGemminiRocketConfigs.scala)

```scala
package chipyard

import org.chipsalliance.cde.config.Config

// Rocket + Ara + Gemmini combined configs
class Ara4096GemminiRocketConfig extends Config(
  new ara.WithAraRocketVectorUnit(4096, 2) ++    // Ara: vLen=4096, 2 lanes
  new gemmini.DefaultGemminiConfig ++             // Gemmini: INT8 systolic
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)

class Ara8192GemminiRocketConfig extends Config(
  new ara.WithAraRocketVectorUnit(8192, 4) ++    // Ara: vLen=8192, 4 lanes
  new gemmini.DefaultGemminiConfig ++             // Gemmini: INT8 systolic
  new chipyard.config.WithSystemBusWidth(128) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)
```

---

## Modifying the Architecture

### 1. **Change Ara Vector Configuration**

**File**: `generators/ara/src/main/scala/Configs.scala`

**Parameters to Modify**:
```scala
class WithAraRocketVectorUnit(
  vLen: Int = 4096,           // Vector register length in bits
  nLanes: Int = 2,            // Number of parallel lanes
  axiIdBits: Int = 4,         // AXI ID width for memory transactions
  cores: Option[Seq[Int]] = None,  // Which cores get Ara (None = all)
  enableDelay: Boolean = false     // Add pipeline delays
)
```

**Example Modifications**:
```scala
// High-performance wide vector config
new ara.WithAraRocketVectorUnit(8192, 4)  // 8192-bit vLen, 4 lanes

// Low-area narrow vector config  
new ara.WithAraRocketVectorUnit(2048, 2)  // 2048-bit vLen, 2 lanes

// Enable pipeline delay modeling
new ara.WithAraRocketVectorUnit(4096, 2, enableDelay = true)
```

---

### 2. **Change Gemmini Accelerator Configuration**

**File**: `generators/gemmini/src/main/scala/gemmini/Configs.scala`

**Available Gemmini Configs**:
```scala
// INT8 quantized inference (default)
new gemmini.DefaultGemminiConfig

// FP32 floating-point training
new gemmini.GemminiFP32DefaultConfig

// Small area-optimized config
new gemmini.LeanGemminiConfig

// With printf debug output
new gemmini.LeanGemminiPrintfConfig
```

**Key Gemmini Parameters** (in `GemminiConfigs.scala`):
```scala
val defaultConfig = GemminiArrayConfig[Float, Float, Float](
  tileRows = 1,
  tileColumns = 1,
  meshRows = 16,          // Systolic array height
  meshColumns = 16,       // Systolic array width
  
  // Memory sizes
  sp_capacity = 256 * 1024,      // Scratchpad size (bytes)
  acc_capacity = 64 * 1024,      // Accumulator size (bytes)
  
  // Data types
  dataflow = Dataflow.BOTH,      // WS (weight stationary) or OS
  inputType = SInt(8.W),         // Input element type
  outputType = SInt(32.W),       // Output element type
  accType = SInt(32.W),          // Accumulator type
  
  // DMA parameters
  dma_maxbytes = 64,
  dma_buswidth = 128,
  
  // Performance
  ex_read_from_spad = true,
  ex_read_from_acc = true
)
```

**Creating Custom Gemmini Config**:

Add to `generators/gemmini/src/main/scala/gemmini/Configs.scala`:
```scala
class CustomGemminiConfig extends Config((site, here, up) => {
  case BuildRoCC => up(BuildRoCC) ++ Seq(
    (p: Parameters) => {
      val gemmini = LazyModule(new Gemmini(
        GemminiArrayConfig[SInt, SInt, SInt](
          meshRows = 8,              // Smaller array (8x8)
          meshColumns = 8,
          sp_capacity = 128 * 1024,   // 128KB scratchpad
          acc_capacity = 32 * 1024,   // 32KB accumulator
          // ... other params
        )
      )(p))
      gemmini
    }
  )
})
```

Then use in top config:
```scala
class CustomAraGemminiConfig extends Config(
  new ara.WithAraRocketVectorUnit(4096, 2) ++
  new gemmini.CustomGemminiConfig ++         // Your custom config
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)
```

---

### 3. **Modify Rocket Core Configuration**

**File**: `generators/chipyard/src/main/scala/config/RocketConfigs.scala`

**Common Rocket Mixins**:
```scala
// Change number of cores
new freechips.rocketchip.rocket.WithNBigCores(2)  // 2 cores
new freechips.rocketchip.rocket.WithNHugeCores(1) // 1 large core

// Change cache sizes
new freechips.rocketchip.subsystem.WithL1ICacheSets(128)  // I$ sets
new freechips.rocketchip.subsystem.WithL1DCacheSets(128)  // D$ sets

// Change FPU precision
new freechips.rocketchip.rocket.WithFPUWithoutDivSqrt  // No FP div/sqrt
new freechips.rocketchip.rocket.WithoutFPU             // No FPU at all
```

---

### 4. **Change System Bus Width**

**File**: Your config file (e.g., `AraGemminiRocketConfigs.scala`)

```scala
// Widerbus = better bandwidth for accelerators
new chipyard.config.WithSystemBusWidth(64)   // Narrow (lower power)
new chipyard.config.WithSystemBusWidth(128)  // Default (balanced)
new chipyard.config.WithSystemBusWidth(256)  // Wide (high bandwidth)
```

---

## Building and Running Tests

### 1. **Setup Environment**

```bash
cd /path/to/chipyard
source env.sh
```

### 2. **Build Gemmini Tests**

```bash
cd generators/gemmini/software/gemmini-rocc-tests
./build.sh
```

This generates test binaries in `build/`:
- `baremetal/` - Bare-metal tests (no OS)
- `pk/` - Tests for proxy kernel
- `linux/` - Linux userspace tests

### 3. **Run Functional Simulation (Spike)**

```bash
# Basic matrix multiply test
spike --extension=gemmini \
  generators/gemmini/software/gemmini-rocc-tests/build/baremetal/matmul-baremetal

# Template test (simple Gemmini operations)
spike --extension=gemmini \
  generators/gemmini/software/gemmini-rocc-tests/build/baremetal/template-baremetal

# Convolution test
spike --extension=gemmini \
  generators/gemmini/software/gemmini-rocc-tests/build/baremetal/conv-baremetal
```

**Note**: Spike provides fast functional simulation but **does NOT model Ara or accurate performance**.

### 4. **Build RTL Simulator (Verilator/VCS)**

```bash
cd sims/verilator  # or sims/vcs

# Build simulator for your config
make CONFIG=Ara4096GemminiRocketConfig

# This generates:
# simulator-chipyard.harness-Ara4096GemminiRocketConfig
```

Build time: **30-60 minutes** depending on config complexity.

### 5. **Run RTL Simulation**

```bash
cd sims/verilator

# Run a test on the RTL simulator
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
  ../../generators/gemmini/software/gemmini-rocc-tests/build/baremetal/template-baremetal

# With waveform dump (for debugging)
./simulator-chipyard.harness-Ara4096GemminiRocketConfig +verbose \
  +vcdfile=template.vcd \
  ../../generators/gemmini/software/gemmini-rocc-tests/build/baremetal/template-baremetal
```

---

## Performance Testing & Benchmarking

### 1. **Gemmini Performance Tests**

**Key Tests**:
- `tiled_matmul_ws_perf-baremetal` - Matrix multiply performance
- `conv_perf-baremetal` - Convolution performance
- `mobilenet-baremetal` - MobileNet inference benchmark

**Run**:
```bash
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
  +verbose +max-cycles=100000000 \
  ../../generators/gemmini/software/gemmini-rocc-tests/build/baremetal/tiled_matmul_ws_perf-baremetal \
  | tee matmul_perf.log
```

**Extract Performance Metrics**:
```bash
# Gemmini cycle counts
grep "Gemmini cycles" matmul_perf.log

# Total execution cycles  
grep "CYCLE" matmul_perf.log
```

### 2. **Ara Vector Performance Tests**

Ara tests are typically in the Ara repository:
```bash
cd generators/ara/ara/apps

# Build Ara tests
make

# Run on simulator
../../sims/verilator/simulator-chipyard.harness-Ara4096GemminiRocketConfig \
  apps/bin/vectorized_kernel
```

### 3. **Custom Performance Instrumentation**

**Modify Test Code** to add cycle counters:

```c
#include "rocc.h"
#include "encoding.h"

uint64_t start_cycles = read_csr(mcycle);

// Your Gemmini/Ara operations here
gemmini_flush(0);  // Wait for Gemmini to finish

uint64_t end_cycles = read_csr(mcycle);
printf("Execution took %llu cycles\n", end_cycles - start_cycles);
```

### 4. **Compare Configurations**

Build multiple configs and compare:

```bash
# Build both configs
make CONFIG=Ara4096GemminiRocketConfig
make CONFIG=Ara8192GemminiRocketConfig

# Run same test on both
for config in Ara4096GemminiRocketConfig Ara8192GemminiRocketConfig; do
  echo "Testing $config"
  ./simulator-chipyard.harness-$config \
    ../../generators/gemmini/software/gemmini-rocc-tests/build/baremetal/matmul-baremetal \
    | grep "cycles"
done
```

---

## Performance Tuning Guidelines

### Ara Vector Unit Tuning

| Parameter | Impact | Recommendation |
|-----------|--------|----------------|
| `vLen` | Register file size, more data per op | 4096 for balanced, 8192 for throughput |
| `nLanes` | Parallel execution units | 2 for area, 4 for performance |
| `enableDelay` | Adds pipeline realism | False for fast sim, True for accuracy |

**Workload Characteristics**:
- **Long vectors**: Increase `vLen` (8192+)
- **High parallelism**: Increase `nLanes` (4+)
- **Short vectors**: Smaller `vLen` (2048) reduces overhead

### Gemmini Accelerator Tuning

| Parameter | Impact | Recommendation |
|-----------|--------|----------------|
| `meshRows/Columns` | Compute throughput (MACs/cycle) | 16x16 default, 8x8 for area |
| `sp_capacity` | Scratchpad SRAM size | 256KB default, tune to workload |
| `acc_capacity` | Accumulator size | 64KB default |
| `dataflow` | WS (weight stationary) vs OS | BOTH for flexibility |
| `dma_buswidth` | Memory bandwidth | Match system bus (128-bit) |

**Workload Characteristics**:
- **Large matrices**: Increase `sp_capacity` and `acc_capacity`
- **Small matrices**: Reduce `meshRows/Columns` for better utilization
- **Bandwidth-limited**: Increase `dma_buswidth` and system bus width
- **Training**: Use `GemminiFP32DefaultConfig`
- **Inference**: Use `DefaultGemminiConfig` (INT8)

### System Bus Tuning

| Width | Use Case | Bandwidth |
|-------|----------|-----------|
| 64-bit | Area-constrained, low-BW | 8 GB/s @ 1 GHz |
| 128-bit | Default, balanced | 16 GB/s @ 1 GHz |
| 256-bit | High-BW accelerators | 32 GB/s @ 1 GHz |

---

## Debugging Tips

### 1. **Enable Verbose Output**

```bash
# Verilator with tracing
./simulator +verbose +vcdfile=debug.vcd your_test

# VCS with waveforms
./simulator +verbose +vcdplusfile=debug.vpd your_test
```

### 2. **Gemmini Debug Prints**

Use `LeanGemminiPrintfConfig` or add to custom config:
```scala
val config = defaultConfig.copy(
  has_training_convs = true,
  has_max_pool = true,
  has_nonlinear_activations = true,
  use_shared_ext_mem = false,
  mvin_scale_shared = false,
  
  // Enable debug
  use_firesim_simulation_utilities = true
)
```

### 3. **Check Generated Verilog**

```bash
# Generated files location
ls -lh generated-src/chipyard.harness.TestHarness.Ara4096GemminiRocketConfig/

# Check for Ara/Gemmini modules
grep "module Ara" generated-src/.../gen-collateral/*.v
grep "module Gemmini" generated-src/.../gen-collateral/*.v
```

---

## Common Issues & Solutions

### Issue: "Gemmini not found" during build

**Solution**: Ensure Gemmini submodule is initialized:
```bash
git submodule update --init generators/gemmini
```

### Issue: Long build times

**Solution**: 
- Use fewer lanes/smaller configs for iteration
- Enable parallel make: `make -j8`
- Use FireSim for FPGA-accelerated simulation

### Issue: Test failures on RTL but not Spike

**Cause**: Spike doesn't model Ara or detailed Gemmini behavior.

**Solution**: Always validate on RTL simulator for accuracy.

### Issue: Out of memory during elaboration

**Solution**: Reduce config complexity or increase heap:
```bash
export SBT_OPTS="-Xmx16G -Xss8M"
```

---

## Additional Resources

- **Chipyard Documentation**: https://chipyard.readthedocs.io
- **Gemmini Paper**: https://arxiv.org/abs/1911.09925
- **Ara Repository**: `generators/ara/ara/`
- **RISC-V Vector Extension**: https://github.com/riscv/riscv-v-spec

---

## Quick Reference

### Build Commands
```bash
# 1. Setup
source env.sh

# 2. Build tests
cd generators/gemmini/software/gemmini-rocc-tests && ./build.sh

# 3. Build simulator
cd sims/verilator && make CONFIG=Ara4096GemminiRocketConfig

# 4. Run test
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
  ../../generators/gemmini/software/gemmini-rocc-tests/build/baremetal/matmul-baremetal
```

### Key File Locations
- **Configs**: `generators/chipyard/src/main/scala/config/`
- **Ara source**: `generators/ara/src/main/scala/`
- **Gemmini source**: `generators/gemmini/src/main/scala/gemmini/`
- **Gemmini tests**: `generators/gemmini/software/gemmini-rocc-tests/`
- **Build output**: `sims/verilator/` or `sims/vcs/`
- **Generated RTL**: `generated-src/chipyard.harness.TestHarness.<CONFIG>/`
