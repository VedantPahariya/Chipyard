# Writing Custom Testcases for Ara+Gemmini Benchmarks

This guide explains how to write custom testcases for the Ara+Gemmini configuration, covering the directory structure, required files, and step-by-step instructions.

## Table of Contents
1. [Understanding the Directory Structure](#understanding-the-directory-structure)
2. [Essential Components](#essential-components)
3. [Writing Your First Testcase](#writing-your-first-testcase)
4. [Example: Simple Gemmini Matrix Multiply](#example-simple-gemmini-matrix-multiply)
5. [Example: Simple Ara Vector Operation](#example-simple-ara-vector-operation)
6. [Advanced Topics](#advanced-topics)
7. [Common Patterns and Best Practices](#common-patterns-and-best-practices)
8. [Troubleshooting](#troubleshooting)
9. [Quick Reference](#quick-reference)

---

## Understanding the Directory Structure

```
software/ara-gemmini-benchmarks/
├── benchmarks/              # Your test source files (.c)
│   ├── ara_gemmini_compare.c
│   └── ara_gemmini_scalar_compare.c
├── include/                 # Header files for APIs
│   ├── gemmini.h           # Gemmini accelerator API
│   ├── gemmini_params.h    # Auto-generated hardware parameters
│   ├── gemmini_testutils.h # Test utilities (malloc, printf, etc.)
│   └── ara_utils.h         # Custom Ara/RVV helper functions
├── build/                   # Compiled binaries and dumps
│   ├── *.baremetal         # Executable binaries
│   └── *.dump              # Disassembly listings
├── Makefile                 # Build automation
└── README.md                # Usage instructions
```

### Why This Structure?

| Component | Purpose | Why It's Here |
|-----------|---------|---------------|
| **benchmarks/** | Test source code | Separates test logic from infrastructure |
| **include/** | Header files | Provides APIs for Gemmini, Ara, and utilities |
| **build/** | Compiled outputs | Keeps source clean, easy to clean/rebuild |
| **Makefile** | Build system | Automates compilation with correct flags |
| **README.md** | Documentation | Quick reference for users |

---

## Essential Components

### 1. The Makefile

**Why you need it:**
- Bare-metal RISC-V compilation requires **many specific flags**
- Links against custom runtime (no standard C library)
- Manages include paths for multiple generators
- Handles RVV (RISC-V Vector) compilation

**Key sections explained:**

```makefile
# ARCHITECTURE: Defines ISA extensions
ARCH := rv64gcv
#       ^^^^^^
#       rv64  = 64-bit RISC-V
#       g     = General (IMAFD extensions)
#       c     = Compressed instructions
#       v     = Vector extension (for Ara)

# COMPILER FLAGS: Critical for bare-metal
CFLAGS := \
    -mcmodel=medany \      # Code model for bare-metal
    -nostdlib \            # No standard library
    -nostartfiles \        # No default startup code
    -static \              # Static linking
    -O2 \                  # Optimization level
    -fno-builtin-printf \  # Use custom printf
    -I$(INCLUDE_DIR)       # Include paths

# LINKER SCRIPT: Defines memory layout
-T $(RISCV_TESTS_DIR)/benchmarks/common/test.ld

# RUNTIME: Provides startup code and syscalls
$(RISCV_TESTS_DIR)/benchmarks/common/crt.S      # Assembly startup
$(RISCV_TESTS_DIR)/benchmarks/common/syscalls.c # System calls (printf, exit)
```

### 2. Header Files

**gemmini.h** - Gemmini Accelerator API
```c
// Core functions you'll use:
void gemmini_flush(uint32_t skip);           // Flush pending operations
void gemmini_config_ld(uint64_t stride);     // Configure load stride
void gemmini_config_st(uint64_t stride);     // Configure store stride
void gemmini_mvin(const void* src, uint32_t dst); // Move data to scratchpad
void gemmini_mvout(void* dst, uint32_t src); // Move data from scratchpad
void tiled_matmul_auto(...);                 // Optimized matrix multiply
```

**ara_utils.h** - RVV Helper Functions
```c
// Vector configuration
void enable_vector_extension();  // Enable RVV (required!)

// Vector operations (inline assembly)
static inline void vector_saxpy(...);  // a*x + y
static inline void vector_add(...);    // Vector addition
```

**gemmini_testutils.h** - Test Utilities
```c
// Memory and I/O (bare-metal compatible)
void* malloc_aligned(size_t size);  // Aligned allocation
void printf_fp32(float value);      // Print float (no stdlib)
unsigned long read_cycles();        // Performance counter
```

### 3. Runtime Files

**Why they're needed:**
- Bare-metal has **no operating system**
- Need custom startup code to:
  - Initialize stack pointer
  - Set up exception handlers
  - Jump to `main()`
- Need custom syscalls for:
  - `printf()` → writes to UART
  - `exit()` → signals test completion

**Where they come from:**
```
generators/gemmini/software/gemmini-rocc-tests/riscv-tests/
└── benchmarks/common/
    ├── crt.S           # Assembly startup code
    ├── syscalls.c      # printf, exit, etc.
    └── test.ld         # Linker script (memory layout)
```

---

## Writing Your First Testcase

### Step 1: Create the Source File

```bash
cd /ssd_scratch/vedantp/chipyard/software/ara-gemmini-benchmarks
nano benchmarks/my_first_test.c
```

### Step 2: Basic Template

```c
// benchmarks/my_first_test.c
#include <stdint.h>
#include <stdio.h>
#include "gemmini.h"
#include "gemmini_testutils.h"

int main() {
    printf("Starting my first test!\n");
    
    // Your test code here
    
    printf("Test completed successfully!\n");
    return 0;  // 0 = success, non-zero = failure
}
```

### Step 3: Add to Makefile

```makefile
# In Makefile, find the TESTS line and add your test:
TESTS := ara_gemmini_compare ara_gemmini_scalar_compare my_first_test
#                                                        ^^^^^^^^^^^^^
```

### Step 4: Compile and Run

```bash
# Source environment (required every time)
source ../../env.sh

# Compile
make

# Run on simulator
cd ../../sims/verilator
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
  ../../software/ara-gemmini-benchmarks/build/my_first_test-baremetal
```

---

## Example: Simple Gemmini Matrix Multiply

```c
// benchmarks/simple_matmul.c
#include <stdint.h>
#include <stdio.h>
#include "gemmini.h"
#include "gemmini_testutils.h"

#define DIM 8  // Use small size for testing

int main() {
    printf("=== Simple Gemmini Matrix Multiply Test ===\n");
    
    // Step 1: Allocate matrices in main memory
    static int8_t A[DIM][DIM] __attribute__((aligned(64)));
    static int8_t B[DIM][DIM] __attribute__((aligned(64)));
    static int32_t C[DIM][DIM] __attribute__((aligned(64)));
    
    // Step 2: Initialize input matrices
    for (int i = 0; i < DIM; i++) {
        for (int j = 0; j < DIM; j++) {
            A[i][j] = (i == j) ? 1 : 0;  // Identity matrix
            B[i][j] = i + j;              // Simple pattern
        }
    }
    
    // Step 3: Configure Gemmini
    gemmini_flush(0);  // Flush any pending operations
    
    // Step 4: Perform matrix multiply on Gemmini
    printf("Computing C = A * B on Gemmini...\n");
    unsigned long start = read_cycles();
    
    tiled_matmul_auto(
        DIM, DIM, DIM,           // Matrix dimensions
        (elem_t*)A, (elem_t*)B,  // Input matrices
        NULL, (elem_t*)C,        // No bias, output C
        DIM, DIM, DIM,           // Strides
        NO_ACTIVATION,           // No activation function
        1.0, 0,                  // Scale factors
        false, false,            // No transpose
        false, false,            // Alignment flags
        3                        // CPU activation type
    );
    
    unsigned long end = read_cycles();
    printf("Gemmini completed in %lu cycles\n", end - start);
    
    // Step 5: Verify result (A is identity, so C should equal B)
    int errors = 0;
    for (int i = 0; i < DIM; i++) {
        for (int j = 0; j < DIM; j++) {
            if (C[i][j] != B[i][j]) {
                printf("ERROR at [%d][%d]: expected %d, got %d\n",
                       i, j, (int)B[i][j], C[i][j]);
                errors++;
            }
        }
    }
    
    if (errors == 0) {
        printf("SUCCESS: All values correct!\n");
        return 0;
    } else {
        printf("FAILED: %d errors found\n", errors);
        return 1;
    }
}
```

**Compile and run:**
```bash
# Add to Makefile
TESTS := ... simple_matmul

# Build
source ../../env.sh
make

# Run
cd ../../sims/verilator
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
  ../../software/ara-gemmini-benchmarks/build/simple_matmul-baremetal
```

---

## Example: Simple Ara Vector Operation

```c
// benchmarks/simple_vector_add.c
#include <stdint.h>
#include <stdio.h>
#include "ara_utils.h"
#include "gemmini_testutils.h"

#define N 64  // Vector length

int main() {
    printf("=== Simple Ara Vector Add Test ===\n");
    
    // Step 1: Enable vector extension (REQUIRED for RVV!)
    enable_vector_extension();
    printf("Vector extension enabled\n");
    
    // Step 2: Allocate vectors
    static int32_t a[N] __attribute__((aligned(64)));
    static int32_t b[N] __attribute__((aligned(64)));
    static int32_t c[N] __attribute__((aligned(64)));
    
    // Step 3: Initialize inputs
    for (int i = 0; i < N; i++) {
        a[i] = i;
        b[i] = i * 2;
    }
    
    // Step 4: Vector addition using RVV
    printf("Computing c = a + b using Ara...\n");
    unsigned long start = read_cycles();
    
    // Inline assembly for RVV vadd instruction
    size_t n = N;
    while (n > 0) {
        size_t vl;
        asm volatile(
            "vsetvli %0, %4, e32, m1, ta, ma\n"  // Set vector length
            "vle32.v v0, (%1)\n"                  // Load a into v0
            "vle32.v v1, (%2)\n"                  // Load b into v1
            "vadd.vv v2, v0, v1\n"                // v2 = v0 + v1
            "vse32.v v2, (%3)\n"                  // Store v2 to c
            : "=r"(vl)
            : "r"(a), "r"(b), "r"(c), "r"(n)
            : "v0", "v1", "v2", "memory"
        );
        n -= vl;
        a += vl;
        b += vl;
        c += vl;
    }
    
    unsigned long end = read_cycles();
    printf("Ara completed in %lu cycles\n", end - start);
    
    // Step 5: Verify (reset pointers first)
    a -= N;
    b -= N;
    c -= N;
    
    int errors = 0;
    for (int i = 0; i < N; i++) {
        int expected = a[i] + b[i];
        if (c[i] != expected) {
            printf("ERROR at [%d]: expected %d, got %d\n", i, expected, c[i]);
            errors++;
        }
    }
    
    if (errors == 0) {
        printf("SUCCESS: All %d elements correct!\n", N);
        return 0;
    } else {
        printf("FAILED: %d errors\n", errors);
        return 1;
    }
}
```

---

## Advanced Topics

### 1. Performance Measurement

```c
#include "gemmini_testutils.h"

// Method 1: Cycle counter (most accurate)
unsigned long start = read_cycles();
// ... code to measure ...
unsigned long end = read_cycles();
printf("Elapsed: %lu cycles\n", end - start);

// Method 2: Instruction counter
unsigned long start_inst = read_instret();
// ... code to measure ...
unsigned long end_inst = read_instret();
printf("Instructions: %lu\n", end_inst - start_inst);
```

### 2. Debugging Techniques

```c
// Enable verbose Gemmini output
#define PRINT_TILE 1  // Add this at top of file

// Print matrix contents
void print_matrix_int8(int8_t* mat, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%4d ", mat[i*cols + j]);
        }
        printf("\n");
    }
}

// Use disassembly to check generated code
// make dump
// less build/my_test-baremetal.dump
// Search for vsetvli, vadd.vv, etc. to verify RVV instructions
```

### 3. Working with Different Data Types

```c
// Gemmini supports INT8, INT16, INT32, FP32 (config dependent)
// Check gemmini_params.h for your config:

#if defined(GEMMINI_TYPE_INT8)
    typedef int8_t elem_t;
    typedef int32_t acc_t;
#elif defined(GEMMINI_TYPE_FP32)
    typedef float elem_t;
    typedef float acc_t;
#endif

// Example: FP32 matrix multiply (if configured for FP32)
float A[DIM][DIM], B[DIM][DIM], C[DIM][DIM];
tiled_matmul_auto(DIM, DIM, DIM,
                  (elem_t*)A, (elem_t*)B,
                  NULL, (elem_t*)C, 
                  DIM, DIM, DIM,
                  NO_ACTIVATION, 1.0, 0,
                  false, false, false, false, 3);
```

### 4. Large Matrix Tiling

```c
// For matrices larger than scratchpad, use manual tiling:
#define TILE_SIZE 16  // Matches Gemmini mesh size
#define N 64          // Large matrix

for (int i = 0; i < N; i += TILE_SIZE) {
    for (int j = 0; j < N; j += TILE_SIZE) {
        for (int k = 0; k < N; k += TILE_SIZE) {
            // Process tile [i:i+TILE_SIZE, j:j+TILE_SIZE]
            tiled_matmul_auto(
                TILE_SIZE, TILE_SIZE, TILE_SIZE,
                &A[i][k], &B[k][j], NULL, &C[i][j],
                N, N, N,  // Full matrix strides
                NO_ACTIVATION, 1.0, 0,
                false, false, false, false, 3
            );
        }
    }
}
```

### 5. Combining Ara + Gemmini

```c
int main() {
    enable_vector_extension();  // Enable Ara
    
    // Part 1: Use Ara for preprocessing (vector operations)
    printf("Ara: Normalizing input vectors...\n");
    unsigned long ara_start = read_cycles();
    // ... RVV normalization code ...
    unsigned long ara_end = read_cycles();
    
    // Part 2: Use Gemmini for GEMM
    printf("Gemmini: Computing matrix multiply...\n");
    gemmini_flush(0);
    unsigned long gem_start = read_cycles();
    tiled_matmul_auto(...);
    unsigned long gem_end = read_cycles();
    
    printf("Ara cycles: %lu\n", ara_end - ara_start);
    printf("Gemmini cycles: %lu\n", gem_end - gem_start);
    printf("Total: %lu\n", (ara_end - ara_start) + (gem_end - gem_start));
}
```

### 6. Viewing Instruction Listings

```bash
# Generate disassembly for all tests
make dump

# View specific test disassembly
less build/ara_gemmini_scalar_compare-baremetal.dump

# Search for specific instructions in dump:
grep "vsetvli" build/*.dump      # Vector setup instructions
grep "vadd.vv" build/*.dump      # Vector add instructions
grep "gemmini" build/*.dump      # Gemmini custom instructions

# View specific section (e.g., main function)
objdump -d -j .text build/my_test-baremetal | less
```

---

## Common Patterns and Best Practices

### Memory Alignment
```c
// ALWAYS align large arrays to cache line (64 bytes)
static int8_t matrix[1024][1024] __attribute__((aligned(64)));

// For dynamic allocation:
void* ptr = malloc_aligned(size);
```

### Error Handling
```c
// Return 0 for success, non-zero for failure
if (result != expected) {
    printf("TEST FAILED\n");
    return 1;  // Simulator will catch this
}
return 0;  // Success
```

### Printf Limitations
```c
// Bare-metal printf is LIMITED:
printf("Value: %d\n", x);        // ✓ Works (integers)
printf("Value: %lu\n", cycles);  // ✓ Works (unsigned long)
printf("Value: %f\n", f);        // ✗ Float doesn't work!

// Workaround for floats:
int int_part = (int)f;
int frac_part = (int)((f - int_part) * 100);
printf("Value: %d.%02d\n", int_part, frac_part);

// Or use custom function:
printf_fp32(f);  // From gemmini_testutils.h
```

### Static vs Dynamic Allocation
```c
// PREFERRED: Static allocation (no malloc needed)
static int32_t data[1024] __attribute__((aligned(64)));

// Alternative: Dynamic allocation (if size unknown at compile time)
int32_t* data = (int32_t*)malloc_aligned(n * sizeof(int32_t));
// Remember: No free() in bare-metal! Memory is limited.
```

---

## Troubleshooting

### Compilation Errors

| Error | Cause | Solution |
|-------|-------|----------|
| `undefined reference to 'printf'` | Missing syscalls.c | Check RUNTIME_FILES in Makefile |
| `illegal instruction` during compile | Wrong -march flag | Use `rv64gcv` for Ara tests |
| `cannot find -lgcc` | Toolchain not in PATH | Run `source ../../env.sh` |
| `undefined reference to 'gemmini_*'` | Missing include | Check `-I` paths in CFLAGS |
| `gemmini.h: No such file` | Wrong include path | Verify symlinks in include/ |

### Runtime Errors

| Symptom | Cause | Solution |
|---------|-------|----------|
| Test hangs forever | Infinite loop or deadlock | Add debug printf, check loop conditions |
| `illegal instruction` exception | RVV not enabled | Call `enable_vector_extension()` first |
| Wrong results | Uninitialized memory | Initialize all arrays before use |
| Simulator exits with code 1337 | Hardware assertion failed | Check Gemmini config matches test expectations |
| Segmentation fault | Stack overflow | Reduce stack usage, use static arrays |

### Build System Issues

```bash
# Common fixes:

# 1. Clean rebuild
make clean
make

# 2. Check environment
source ../../env.sh
which riscv64-unknown-elf-gcc  # Should show a path

# 3. Verify include paths exist
ls -la generators/gemmini/software/gemmini-rocc-tests/include/

# 4. Check for syntax errors
riscv64-unknown-elf-gcc -fsyntax-only benchmarks/my_test.c \
  -I include -I ../../generators/gemmini/software/gemmini-rocc-tests/include

# 5. Verify Makefile paths
make -n  # Dry run - shows commands without executing
```

### Debugging RVV Issues

```bash
# Check if RVV instructions are in binary
make dump
grep "vsetvli\|vle32\|vse32\|vadd.vv" build/my_test-baremetal.dump

# If no RVV instructions found:
# 1. Check compilation flags
make clean
make V=1  # Verbose - shows full compiler commands
# Look for: -march=rv64gcv

# 2. Verify inline assembly syntax
# Inline asm must be exactly correct - check examples
```

---

## Checklist for New Testcases

- [ ] Created `.c` file in `benchmarks/` directory
- [ ] Added test name to `TESTS` in Makefile
- [ ] Included necessary headers (`gemmini.h`, `ara_utils.h`, etc.)
- [ ] Aligned large arrays to 64 bytes with `__attribute__((aligned(64)))`
- [ ] Called `enable_vector_extension()` if using RVV
- [ ] Called `gemmini_flush(0)` before Gemmini operations
- [ ] Used `read_cycles()` for performance measurement
- [ ] Verified results before returning
- [ ] Returned 0 for success, non-zero for failure
- [ ] Tested compilation: `source ../../env.sh && make`
- [ ] Tested execution on simulator
- [ ] Generated disassembly: `make dump`
- [ ] Verified RVV/Gemmini instructions in dump file
- [ ] Documented test purpose in comments at top of file

---

## Quick Reference

### Essential Commands
```bash
# Setup (once per session)
cd /ssd_scratch/vedantp/chipyard/software/ara-gemmini-benchmarks
source ../../env.sh

# Build all tests
make

# Build with verbose output
make V=1

# Generate disassembly
make dump

# Clean and rebuild
make clean && make

# Run specific test on Ara4096GemminiRocketConfig
cd ../../sims/verilator
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
  ../../software/ara-gemmini-benchmarks/build/<test>-baremetal

# Run on Ara8192GemminiRocketConfig
./simulator-chipyard.harness-Ara8192GemminiRocketConfig \
  ../../software/ara-gemmini-benchmarks/build/<test>-baremetal
```

### File Locations
```
Test source:     software/ara-gemmini-benchmarks/benchmarks/
Test binaries:   software/ara-gemmini-benchmarks/build/
Headers:         software/ara-gemmini-benchmarks/include/
Gemmini API:     generators/gemmini/software/gemmini-rocc-tests/include/
Runtime files:   generators/gemmini/software/gemmini-rocc-tests/riscv-tests/benchmarks/common/
Simulators:      sims/verilator/simulator-chipyard.harness-*
```

### Common Include Patterns
```c
// Basic test (no accelerators)
#include <stdint.h>
#include <stdio.h>
#include "gemmini_testutils.h"  // For read_cycles(), printf, etc.

// Gemmini test
#include <stdint.h>
#include <stdio.h>
#include "gemmini.h"
#include "gemmini_testutils.h"

// Ara (RVV) test
#include <stdint.h>
#include <stdio.h>
#include "ara_utils.h"           // For enable_vector_extension()
#include "gemmini_testutils.h"   // For read_cycles()

// Combined Ara + Gemmini test
#include <stdint.h>
#include <stdio.h>
#include "gemmini.h"
#include "ara_utils.h"
#include "gemmini_testutils.h"
```

### Quick Template
```c
#include <stdint.h>
#include <stdio.h>
#include "gemmini.h"
#include "ara_utils.h"
#include "gemmini_testutils.h"

#define SIZE 64

int main() {
    printf("=== My Test ===\n");
    
    // 1. Setup
    enable_vector_extension();  // If using Ara
    gemmini_flush(0);           // If using Gemmini
    
    // 2. Allocate & initialize
    static int32_t data[SIZE] __attribute__((aligned(64)));
    for (int i = 0; i < SIZE; i++) data[i] = i;
    
    // 3. Run test
    unsigned long start = read_cycles();
    // ... your code ...
    unsigned long end = read_cycles();
    
    // 4. Report
    printf("Cycles: %lu\n", end - start);
    
    // 5. Verify & return
    // ... verification ...
    printf("SUCCESS\n");
    return 0;
}
```

---

## Summary

Writing custom testcases requires understanding:

1. **Directory structure** - Organized separation of source, headers, and build outputs
2. **Makefile** - Handles complex bare-metal compilation with correct flags
3. **Headers** - Provide APIs for Gemmini, Ara, and test utilities
4. **Runtime** - Custom startup code and syscalls for bare-metal environment
5. **Best practices** - Alignment, error handling, performance measurement

The key insight is that **bare-metal testing has no operating system**, so you need to provide all infrastructure yourself. The structure at `software/ara-gemmini-benchmarks/` packages everything needed in a reusable template.

**Start simple** with the basic examples, then explore `ara_gemmini_scalar_compare.c` for advanced patterns like tiling, multi-accelerator coordination, and comprehensive performance measurement!

---

## Additional Resources

- **Gemmini Documentation**: `generators/gemmini/README.md`
- **Ara Documentation**: `generators/ara/README.md`  
- **RISC-V Vector Extension Spec**: https://github.com/riscv/riscv-v-spec
- **Chipyard Documentation**: https://chipyard.readthedocs.io
- **Integration Guide**: [ROCKET_ARA_GEMMINI_SOC_GUIDE.md](ROCKET_ARA_GEMMINI_SOC_GUIDE.md)
- **Quick Start**: [ARA_GEMMINI_QUICKSTART.md](ARA_GEMMINI_QUICKSTART.md)
