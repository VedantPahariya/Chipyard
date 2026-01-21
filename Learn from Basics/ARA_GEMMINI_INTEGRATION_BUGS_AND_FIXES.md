# Ara + Gemmini + Rocket Integration: Bugs, Fixes & Instructions

> **Document Version:** 1.0  
> **Date:** January 19, 2026  
> **Chipyard Version:** Main branch (commit 291733d8)  
> **Configuration:** `Ara4096GemminiRocketConfig`

---

## Table of Contents
1. [Executive Summary](#executive-summary)
2. [System Architecture](#system-architecture)
3. [Build & Test Commands](#build--test-commands)
4. [Bugs Encountered & Fixes Applied](#bugs-encountered--fixes-applied)
5. [Test Results](#test-results)
6. [File Modifications Summary](#file-modifications-summary)
7. [Troubleshooting Guide](#troubleshooting-guide)

---

## Executive Summary

This document details the complete integration of **Ara Vector Coprocessor** with **Gemmini Accelerator** on a **Rocket Core** in Chipyard. The integration involved resolving 5 major build issues to achieve a working Verilator RTL simulation.

### Final Configuration
| Component | Specification |
|-----------|--------------|
| **Core** | Rocket (1 Huge Core, RV64GC) |
| **Vector Unit** | Ara (vLen=4096, 2 lanes, RVV 1.0) |
| **Matrix Accelerator** | Gemmini (INT8 systolic array) |
| **System Bus** | 128-bit TileLink |
| **Simulator** | Verilator |

### Test Status
| Test | Status |
|------|--------|
| Gemmini template-baremetal | ✅ **PASSED** |
| Vector vec-memcpy | ✅ **PASSED** |
| Vector vec-daxpy (FP) | ✅ **PASSED** |

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                     Rocket Tile                               │
│  ┌──────────────────┐                                        │
│  │   Rocket Core    │ ◄── Scalar processing (RV64GC)         │
│  │   (5-stage)      │                                        │
│  └────────┬─────────┘                                        │
│           │                                                   │
│    ┌──────┴──────────────────────────────┐                   │
│    │    RoCC Interface + Vector Port     │                   │
│    └──────┬───────────────────┬──────────┘                   │
│           │                   │                              │
│  ┌────────▼────────┐  ┌───────▼─────────┐                   │
│  │  Ara Vector     │  │    Gemmini      │                   │
│  │  Coprocessor    │  │   Accelerator   │                   │
│  │                 │  │                 │                   │
│  │ • RISC-V V 1.0  │  │ • INT8 MAC      │                   │
│  │ • vLen: 4096    │  │ • Scratchpad    │                   │
│  │ • 2 Lanes       │  │ • DMA Engine    │                   │
│  │ • 64-bit ELEN   │  │ • Weight SRAM   │                   │
│  └─────────────────┘  └─────────────────┘                   │
└──────────────────────────────────────────────────────────────┘
                          │
                 ┌────────▼────────┐
                 │ TileLink Bus    │
                 │ (128-bit wide)  │
                 └─────────────────┘
```

### Configuration Class

**File:** `generators/chipyard/src/main/scala/config/AraGemminiRocketConfigs.scala`

```scala
package chipyard

import org.chipsalliance.cde.config.Config

class Ara4096GemminiRocketConfig extends Config(
  new ara.WithAraRocketVectorUnit(4096, 2) ++    // Ara: vLen=4096, 2 lanes
  new gemmini.DefaultGemminiConfig ++             // Gemmini: INT8 systolic
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)

class Ara8192GemminiRocketConfig extends Config(
  new ara.WithAraRocketVectorUnit(8192, 4) ++    // Ara: vLen=8192, 4 lanes
  new gemmini.DefaultGemminiConfig ++
  new chipyard.config.WithSystemBusWidth(128) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)
```

---

## Build & Test Commands

### Prerequisites
```bash
# Ensure Chipyard environment is sourced
cd /path/to/chipyard
source env.sh
```

### Build Verilator Simulator
```bash
# CRITICAL: USE_ARA=1 flag is REQUIRED
cd sims/verilator
make CONFIG=Ara4096GemminiRocketConfig USE_ARA=1 -j$(nproc)
```

**Build Time:** ~30-45 minutes on 36 cores

### Run Tests

#### Gemmini Test
```bash
cd sims/verilator
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
    ../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/template-baremetal
```

#### Ara Vector Tests
```bash
# Vector memory copy test
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
    ../../.conda-env/riscv-tools/riscv64-unknown-elf/share/riscv-tests/benchmarks/vec-memcpy.riscv

# Vector DAXPY (double-precision A*X+Y)
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
    ../../.conda-env/riscv-tools/riscv64-unknown-elf/share/riscv-tests/benchmarks/vec-daxpy.riscv

# Vector matrix multiply (floating point)
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
    ../../.conda-env/riscv-tools/riscv64-unknown-elf/share/riscv-tests/benchmarks/vec-sgemm.riscv
```

---

## Bugs Encountered & Fixes Applied

### Bug #1: BOOM `Annotated` Import Error

**Error Message:**
```
[error] generators/boom/src/main/scala/v3/common/tile.scala:23: object Annotated is not a member of package freechips.rocketchip.util
[error] import freechips.rocketchip.util.Annotated
```

**Root Cause:**  
The `Annotated` utility class was removed or relocated in newer versions of Rocket-Chip. BOOM v3 and v4 tile definitions still reference it.

**Fix Applied:**
```scala
// In generators/boom/src/main/scala/v3/common/tile.scala (line 23)
// BEFORE:
import freechips.rocketchip.util.Annotated

// AFTER:
// import freechips.rocketchip.util.Annotated  // Not available in this version

// In the class body (line 158)
// BEFORE:
Annotated.params(this, outer.boomParams)

// AFTER:
// Annotated.params(this, outer.boomParams)  // Not available in this version
```

**Files Modified:**
- `generators/boom/src/main/scala/v3/common/tile.scala` (lines 23, 158)
- `generators/boom/src/main/scala/v4/common/tile.scala` (lines 23, 157)

---

### Bug #2: Missing `acc_pkg` from CVA6

**Error Message:**
```
%Error: cannot find "acc_pkg.sv" in include paths
```

**Root Cause:**  
Ara depends on CVA6's `acc_pkg.sv` for accelerator interface type definitions. The build system requires the `USE_ARA=1` flag to include the correct file paths, and the CVA6 submodule needs to be properly checked out.

**Fix Applied:**

1. **Add `USE_ARA=1` flag to build command:**
```bash
make CONFIG=Ara4096GemminiRocketConfig USE_ARA=1 -j$(nproc)
```

2. **Checkout CVA6 submodule with correct branch:**
```bash
cd generators/ara/ara/hardware/deps
rm -rf cva6
git clone https://github.com/pulp-platform/cva6.git
cd cva6
git checkout mp/pulp-v1-araOS
cd ../..
bender checkout
```

**Why this branch?**  
The `mp/pulp-v1-araOS` branch contains the accelerator interface package (`acc_pkg.sv`) that Ara requires. The default CVA6 branch doesn't include this.

---

### Bug #3: Bender CVA6 Checkout Failure

**Error Message:**
```
error: Dependency 'cva6' points to nonexistent git object
```

**Root Cause:**  
The Bender lock file (`Bender.lock`) references a specific CVA6 commit that doesn't exist in the default repository state.

**Fix Applied:**

Manual clone and checkout:
```bash
cd generators/ara/ara/hardware/deps
rm -rf cva6
git clone https://github.com/pulp-platform/cva6.git
cd cva6
git checkout mp/pulp-v1-araOS
cd ../..
bender checkout  # Now succeeds for other deps
```

---

### Bug #4: `pad_functional.sv` Contains Unsupported `rpmos` Primitive

**Error Message:**
```
%Error: .../pad_functional.sv:13:3: Unsupported: Verilog 2001 UDP
   13 |   rpmos (padio, i, oen);
      |   ^~~~~
```

**Root Cause:**  
The file `hardware/deps/tech_cells_generic/src/deprecated/pad_functional.sv` uses `rpmos` (resistive pull-down MOS) - a Verilog primitive not supported by Verilator.

**Fix Applied:**

Remove the file from `generators/ara/ara_files.f`:
```bash
# In generators/ara/ara_files.f
# Delete this line:
hardware/deps/tech_cells_generic/src/deprecated/pad_functional.sv
```

**Justification:**  
The file is in a `deprecated/` directory and is not critical for RTL simulation. The pad functionality is only needed for physical implementation.

---

### Bug #5: Verilator `ENUMVALUE` Lint Errors

**Error Message:**
```
%Error-ENUMVALUE: .../lane_sequencer.sv:1139:15: Enum 'vew_e' expects 4 bits, but 'RVVW8' generates 3 bits
%Error-ENUMVALUE: .../sldu.sv:243:35: Enum 'ara_op_e' expects 7 bits, but 'VSLIDEUP' generates 6 bits
```

**Root Cause:**  
Ara's SystemVerilog uses enum types where the assigned values don't match the inferred bit-width. This is valid SystemVerilog but Verilator is strict about it.

**Fix Applied:**

Add lint suppression flags in `sims/verilator/Makefile`:
```makefile
# In VERILOG_IP_VERILATOR_FLAGS section (around line 120)
VERILOG_IP_VERILATOR_FLAGS := \
    --unroll-count 256 \
    -Wno-PINCONNECTEMPTY \
    -Wno-ASSIGNDLY \
    -Wno-DECLFILENAME \
    -Wno-UNUSED \
    -Wno-UNOPTFLAT \
    -Wno-BLKANDNBLK \
    -Wno-style \
    -Wno-ENUMVALUE \        # <-- ADDED
    -Wno-WIDTHEXPAND \      # <-- ADDED
    -Wno-WIDTHTRUNC \
    -Wall
```

---

## Test Results

### Test 1: Gemmini template-baremetal

**Command:**
```bash
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
    ../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/template-baremetal
```

**Output:**
```
Flush Gemmini TLB of stale virtual addresses
Initialize our input and output matrices in main memory
Calculate the scratchpad addresses of all our matrices
  Note: The scratchpad is "row-addressed", where each address contains one matrix row
Move "In" matrix from main memory into Gemmini's scratchpad
Move "Identity" matrix from main memory into Gemmini's scratchpad
Multiply "In" matrix with "Identity" matrix with a bias of 0
Move "Out" matrix from Gemmini's scratchpad into main memory
Fence till Gemmini completes all memory operations
Check whether "In" and "Out" matrices are identical
Input and output matrices are identical, as expected
[UART] UART0 is here (stdin/stdout).
$finish
```

**Status:** ✅ **PASSED**

---

### Test 2: Vector vec-memcpy

**Command:**
```bash
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
    ../../.conda-env/riscv-tools/riscv64-unknown-elf/share/riscv-tests/benchmarks/vec-memcpy.riscv
```

**Output:**
```
mcycle = 4377
minstret = 51
[UART] UART0 is here (stdin/stdout).
$finish
```

**Status:** ✅ **PASSED**

---

### Test 3: Vector vec-daxpy (Double-Precision FP)

**Command:**
```bash
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
    ../../.conda-env/riscv-tools/riscv64-unknown-elf/share/riscv-tests/benchmarks/vec-daxpy.riscv
```

**Output:**
```
mcycle = 3718
minstret = 45
[UART] UART0 is here (stdin/stdout).
$finish
```

**Status:** ✅ **PASSED**

---

## File Modifications Summary

| File | Modification | Reason |
|------|--------------|--------|
| `generators/chipyard/src/main/scala/config/AraGemminiRocketConfigs.scala` | Created | Define Ara+Gemmini+Rocket configs |
| `generators/boom/src/main/scala/v3/common/tile.scala` | Modified lines 23, 158 | Comment out `Annotated` import/usage |
| `generators/boom/src/main/scala/v4/common/tile.scala` | Modified lines 23, 157 | Comment out `Annotated` import/usage |
| `generators/ara/ara_files.f` | Removed `pad_functional.sv` | Unsupported `rpmos` primitive |
| `sims/verilator/Makefile` | Added `-Wno-ENUMVALUE -Wno-WIDTHEXPAND` | Suppress Ara enum warnings |
| `generators/ara/ara/hardware/deps/cva6/` | Manually cloned | Branch `mp/pulp-v1-araOS` for `acc_pkg.sv` |

---

## Troubleshooting Guide

### Issue: Build hangs or takes forever
**Solution:** Ensure you're using enough parallel jobs and have sufficient memory.
```bash
# Check available memory
free -h

# Use reasonable parallel jobs (don't exceed RAM)
make CONFIG=Ara4096GemminiRocketConfig USE_ARA=1 -j16
```

### Issue: `acc_pkg` not found
**Solution:** Verify CVA6 is on the correct branch:
```bash
cd generators/ara/ara/hardware/deps/cva6
git branch  # Should show mp/pulp-v1-araOS
ls core/include/  # Should contain acc_pkg.sv
```

### Issue: Verilator version incompatibility
**Solution:** Use the Verilator version bundled with Chipyard:
```bash
which verilator
# Should point to: .conda-env/bin/verilator
```

### Issue: Test hangs or times out
**Solution:** RTL simulations are slow. A simple test can take 5-10 minutes.
```bash
# Run with longer timeout
timeout 900 ./simulator-chipyard.harness-Ara4096GemminiRocketConfig test.riscv
```

### Issue: "Stack size too small" warning
**Solution:** Increase stack size (optional, warning is benign):
```bash
ulimit -s 40960
```

---

## References

- [Ara Vector Processor](https://github.com/pulp-platform/ara)
- [Gemmini Accelerator](https://github.com/ucb-bar/gemmini)
- [Chipyard Framework](https://github.com/ucb-bar/chipyard)
- [RISC-V Vector Extension Spec](https://github.com/riscv/riscv-v-spec)

---

## Appendix: Full Build Log Commands

```bash
# 1. Navigate to chipyard
cd /path/to/chipyard

# 2. Source environment
source env.sh

# 3. Ensure Ara deps are properly checked out
cd generators/ara/ara/hardware
# If bender checkout fails for CVA6:
cd deps
rm -rf cva6
git clone https://github.com/pulp-platform/cva6.git
cd cva6 && git checkout mp/pulp-v1-araOS && cd ..
bender checkout
cd ../../../..

# 4. Build simulator
cd sims/verilator
make CONFIG=Ara4096GemminiRocketConfig USE_ARA=1 -j$(nproc) 2>&1 | tee build.log

# 5. Run tests
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
    ../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/template-baremetal

./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
    ../../.conda-env/riscv-tools/riscv64-unknown-elf/share/riscv-tests/benchmarks/vec-memcpy.riscv
```

---

**Document End**
