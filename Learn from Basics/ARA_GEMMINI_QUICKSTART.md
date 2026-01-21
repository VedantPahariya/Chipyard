# Ara4096GemminiRocketConfig: Quick Start Guide

> Heterogeneous SoC: Rocket Core + Ara Vector Unit (RVV) + Gemmini Systolic Array

---

## Quick Setup

```bash
cd /ssd_scratch/vedantp/chipyard
source env.sh
```

---

## 1. Running Pre-Built Tests

### Test Gemmini (Matrix Multiply)
```bash
cd sims/verilator
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
  ../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/template-baremetal
```

### Test Ara Vector + Gemmini + Scalar Comparison
```bash
cd sims/verilator
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
  ../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/ara_gemmini_scalar_compare-baremetal
```

### Test Vector Operations
```bash
cd sims/verilator
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
  ../../.conda-env/riscv-tools/riscv64-unknown-elf/share/riscv-tests/benchmarks/vec-memcpy.riscv
```

---

## 2. Creating Your Own Test

### Step 1: Write Test Code

Create file: `generators/gemmini/software/gemmini-rocc-tests/bareMetalC/my_test.c`

```c
#include <stdint.h>
#include <stdio.h>
#include "include/gemmini_testutils.h"

int main() {
    printf("Hello from Ara+Gemmini!\n");
    
    // Gemmini example
    gemmini_flush(0);
    elem_t A[DIM][DIM];
    elem_t B[DIM][DIM];
    elem_t C[DIM][DIM];
    
    // Initialize matrices...
    // Call Gemmini operations...
    
    exit(0);
}
```

### Step 2: Compile Test

**Option A: Standard Compilation (No RVV)**
```bash
cd /ssd_scratch/vedantp/chipyard && source env.sh
cd generators/gemmini/software/gemmini-rocc-tests

riscv64-unknown-elf-gcc \
  -DPREALLOCATE=1 -DMULTITHREAD=1 -mcmodel=medany -std=gnu99 -O2 \
  -ffast-math -fno-common -fno-builtin-printf -fno-tree-loop-distribute-patterns \
  -march=rv64gc -Wa,-march=rv64gc \
  -lm -lgcc \
  -I./riscv-tests -I./riscv-tests/env -I. -I./riscv-tests/benchmarks/common \
  -DID_STRING= -DPRINT_TILE=0 -DBAREMETAL=1 \
  -nostdlib -nostartfiles -static -T ./riscv-tests/benchmarks/common/test.ld \
  ./bareMetalC/my_test.c \
  ./riscv-tests/benchmarks/common/syscalls.c \
  ./riscv-tests/benchmarks/common/crt.S \
  -o build/bareMetalC/my_test-baremetal
```

**Option B: With RVV Support (For Ara Vector Instructions)**
```bash
# Add `-march=rv64gcv -Wa,-march=rv64gcv` to enable RISC-V Vector Extension
riscv64-unknown-elf-gcc \
  -DPREALLOCATE=1 -DMULTITHREAD=1 -mcmodel=medany -std=gnu99 -O2 \
  -ffast-math -fno-common -fno-builtin-printf -fno-tree-loop-distribute-patterns \
  -march=rv64gcv -Wa,-march=rv64gcv \
  -lm -lgcc \
  -I./riscv-tests -I./riscv-tests/env -I. -I./riscv-tests/benchmarks/common \
  -DID_STRING= -DPRINT_TILE=0 -DBAREMETAL=1 \
  -nostdlib -nostartfiles -static -T ./riscv-tests/benchmarks/common/test.ld \
  ./bareMetalC/my_test.c \
  ./riscv-tests/benchmarks/common/syscalls.c \
  ./riscv-tests/benchmarks/common/crt.S \
  -o build/bareMetalC/my_test-baremetal
```

### Step 3: Run on Simulator
```bash
cd /ssd_scratch/vedantp/chipyard/sims/verilator
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
  ../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/my_test-baremetal
```

---

## 3. Available Test APIs

### Gemmini Operations
```c
#include "include/gemmini_testutils.h"

gemmini_flush(0);                     // Flush TLB
gemmini_config_ld(stride);            // Configure load
gemmini_config_st(stride);            // Configure store
gemmini_mvin(src, sp_addr);           // Move in from memory
gemmini_mvout(dst, sp_addr);          // Move out to memory
gemmini_compute_preloaded(A, B);      // Matrix multiply
gemmini_preload_zeros(C_addr);        // Preload zeros
gemmini_fence();                      // Wait for completion
```

### Performance Counters
```c
// Read cycle counter
uint64_t cycles = read_csr(mcycle);

// Read instruction counter
uint64_t instrs = read_csr(minstret);
```

### Vector Operations (RVV - requires -march=rv64gcv)
```c
// Enable vector extension
uint64_t mstatus;
asm volatile("csrr %0, mstatus" : "=r"(mstatus));
mstatus |= (1UL << 9);  // Set VS bit
asm volatile("csrw mstatus, %0" : : "r"(mstatus));

// Vector SAXPY: y = a*x + y
asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(n));
asm volatile("vle32.v v1, (%0)" : : "r"(x) : "memory", "v1");
asm volatile("vle32.v v2, (%0)" : : "r"(y) : "memory", "v2");
asm volatile("vmul.vx v3, v1, %0" : : "r"(a) : "v3");
asm volatile("vadd.vv v2, v3, v2" : : : "v2");
asm volatile("vse32.v v2, (%0)" : : "r"(y) : "memory");
```

---

## 4. Creating New Configs

### Create New Configuration

**File:** `generators/chipyard/src/main/scala/config/AraGemminiRocketConfigs.scala`

```scala
package chipyard

import org.chipsalliance.cde.config.Config

// New config: Ara8192 + Gemmini + Rocket
class Ara8192GemminiRocketConfig extends Config(
  new ara.WithAraRocketVectorUnit(8192, 4) ++     // vLen=8192, 4 lanes
  new gemmini.DefaultGemminiConfig ++              // Standard Gemmini
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)

// New config: Ara + Gemmini FP32 + Rocket
class AraGemmini FP32RocketConfig extends Config(
  new ara.WithAraRocketVectorUnit(4096, 2) ++
  new gemmini.GemminiFP32DefaultConfig ++         // Floating-point Gemmini
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)
```

### Build with New Config

```bash
cd /ssd_scratch/vedantp/chipyard/sims/verilator
make CONFIG=Ara8192GemminiRocketConfig USE_ARA=1 -j$(nproc)

# Simulator binary created at:
# simulator-chipyard.harness-Ara8192GemminiRocketConfig
```

### Run Test on New Config
```bash
./simulator-chipyard.harness-Ara8192GemminiRocketConfig \
  ../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/template-baremetal
```

---

## 5. Configurable Parameters

### Ara Vector Unit
| Parameter | Values | Impact |
|-----------|--------|--------|
| `vLen` | 2048, 4096, 8192 | Vector register size |
| `nLanes` | 1, 2, 4 | Parallel execution units |

Example:
```scala
new ara.WithAraRocketVectorUnit(vLen=4096, nLanes=2)
new ara.WithAraRocketVectorUnit(vLen=8192, nLanes=4)  // Wider
```

### Gemmini Configurations
| Config | Type | Use Case |
|--------|------|----------|
| `DefaultGemminiConfig` | INT8 | Inference |
| `GemminiFP32DefaultConfig` | FP32 | Training/FP ops |
| `LeanGemminiConfig` | Small area | Low power |

### Rocket Core
```scala
new freechips.rocketchip.rocket.WithNBigCores(2)     // 2 cores
new freechips.rocketchip.rocket.WithNHugeCores(1)    // 1 large core
new freechips.rocketchip.subsystem.WithL1ICacheSets(128)  // I$ size
new freechips.rocketchip.subsystem.WithL1DCacheSets(128)  // D$ size
```

### System Bus
```scala
new chipyard.config.WithSystemBusWidth(64)    // 64-bit (narrow)
new chipyard.config.WithSystemBusWidth(128)   // 128-bit (default)
new chipyard.config.WithSystemBusWidth(256)   // 256-bit (wide/high-BW)
```

---

## 6. Build System Details

### Build Directory Structure
```
chipyard/
  sims/verilator/
    Makefile                    # Build configuration
    generated-src/              # Generated RTL
  generators/
    gemmini/
      software/gemmini-rocc-tests/
        build/bareMetalC/       # Compiled tests
        bareMetalC/             # Test sources
    ara/                        # Ara vector unit
```

### Key Build Flags
```bash
# Build with Ara support (REQUIRED)
USE_ARA=1

# Configuration to build
CONFIG=Ara4096GemminiRocketConfig

# Parallel jobs
-j$(nproc)

# Verbose output
+verbose
```

---

## 7. Performance Metrics

### Default Configuration (Ara4096GemminiRocketConfig)

| Operation | Processor | Cycles | Speedup |
|-----------|-----------|--------|---------|
| SAXPY (N=256) | Scalar | 2345 | 1.0x |
| SAXPY (N=256) | Ara (RVV) | 306 | **7.6x** |
| MatMul (16×16) | Scalar (INT32) | 57326 | 1.0x |
| MatMul (16×16) | Gemmini (INT8) | 298 | **192x** |

---

## 8. Troubleshooting

| Issue | Solution |
|-------|----------|
| `acc_pkg` not found | Use `USE_ARA=1` flag |
| RVV instructions fail | Compile with `-march=rv64gcv` |
| Build hangs | Reduce `-j` flag, check memory |
| Test timeout | RTL is slow (10-15 min per test) |
| Vector instructions error | Check MSTATUS.VS bit is set |

---

## 9. File Locations

| Item | Path |
|------|------|
| Configs | `generators/chipyard/src/main/scala/config/` |
| Ara wrapper | `generators/ara/chipyard/` |
| Gemmini | `generators/gemmini/src/main/scala/gemmini/` |
| Tests | `generators/gemmini/software/gemmini-rocc-tests/` |
| Simulator | `sims/verilator/simulator-chipyard.harness-*` |
| Performance docs | [ARA_GEMMINI_INTEGRATION_BUGS_AND_FIXES.md](ARA_GEMMINI_INTEGRATION_BUGS_AND_FIXES.md) |

---

## 10. Example: Full Workflow

```bash
# 1. Setup environment
cd /ssd_scratch/vedantp/chipyard
source env.sh

# 2. Create test
cat > generators/gemmini/software/gemmini-rocc-tests/bareMetalC/mytest.c << 'EOF'
#include <stdio.h>
#include "include/gemmini_testutils.h"

int main() {
    printf("Test started\n");
    gemmini_flush(0);
    printf("Gemmini flushed\n");
    exit(0);
}
EOF

# 3. Compile with RVV
cd generators/gemmini/software/gemmini-rocc-tests && \
riscv64-unknown-elf-gcc \
  -march=rv64gcv -Wa,-march=rv64gcv \
  -DPREALLOCATE=1 -DMULTITHREAD=1 -mcmodel=medany -std=gnu99 -O2 \
  -ffast-math -fno-common -fno-builtin-printf -fno-tree-loop-distribute-patterns \
  -lm -lgcc \
  -I./riscv-tests -I./riscv-tests/env -I. -I./riscv-tests/benchmarks/common \
  -DID_STRING= -DPRINT_TILE=0 -DBAREMETAL=1 \
  -nostdlib -nostartfiles -static -T ./riscv-tests/benchmarks/common/test.ld \
  ./bareMetalC/mytest.c \
  ./riscv-tests/benchmarks/common/syscalls.c \
  ./riscv-tests/benchmarks/common/crt.S \
  -o build/bareMetalC/mytest-baremetal

# 4. Run on simulator
cd /ssd_scratch/vedantp/chipyard/sims/verilator
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
  ../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/mytest-baremetal
```

---

**Happy accelerating! 🚀**
