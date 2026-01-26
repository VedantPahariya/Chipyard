# Chipyard

## Documentation Files

### Learn from Basics/
- **ARA_GEMMINI_INTEGRATION_BUGS_AND_FIXES.md** : Detailed documentation of all 5 bugs encountered and fixes applied during Ara+Gemmini+Rocket integration
- **ARA_GEMMINI_QUICKSTART.md** : Quick start guide with step-by-step instructions for running tests and creating custom configs
- **DOCUMENTATION_MAP.txt** : Visual documentation map with quick reference for all tasks and file locations
- **README_ARA_GEMMINI_COMPLETE.md** : Complete summary of Ara+Gemmini+Rocket integration with performance metrics and test results
- **ROCKET_ARA_GEMMINI_SOC_GUIDE.md** : Comprehensive architecture guide covering system components, configuration, and performance tuning
- **WRITING_CUSTOM_TESTCASES.md** : Complete guide to writing custom bare-metal testcases for Ara+Gemmini with examples, troubleshooting, and best practices

### Setup Guides/
- **Chipyard Feasibility.md** : Feasibility analysis and evaluation of Chipyard for heterogeneous SoC development
- **Chipyard_Instructions.md** : General instructions for working with Chipyard framework
- **Chipyard_SETUP_GUIDE.md** : Step-by-step setup guide for installing and configuring Chipyard
- **GEMMINI_DIMENSION_GUIDE.md** : Guide for configuring Gemmini systolic array dimensions and parameters
- **gemmini_test_results.md** : Test results and performance measurements for Gemmini accelerator
- **Run_Gemmini.md** : Instructions for building and running Gemmini tests
- **SIMULATION_RESULTS.md** : RTL simulation results and analysis
- **STORAGE_USAGE_REPORT.md** : Storage requirements and disk usage analysis for Chipyard builds

### Performance Testing Kernels/
- **ara_gemmini_compare.c** : Basic performance comparison testbench for Ara and Gemmini accelerators
- **ara_gemmini_scalar_compare.c** : Comprehensive performance benchmark comparing Scalar CPU, Ara Vector Unit, and Gemmini Systolic Array


## Quick Links

**Start Here:** [ARA_GEMMINI_QUICKSTART.md](Learn%20from%20Basics/ARA_GEMMINI_QUICKSTART.md)

**Setup:** [Chipyard_SETUP_GUIDE.md](Setup%20Guides/Chipyard_SETUP_GUIDE.md)

## Chipyard Basics

![Chipyard Directory Structure](Images/Chipyard%20Directory%20Structure.png)
*Figure: Overview of the Chipyard directory structure.*

---
### Running a new Configuration
To create a new Chipyard configuration that integrates Ara Vector Unit and Gemmini Systolic Array with Rocket core, follow these steps:
1. **Define the new Configuration:**
   - Create a new Scala configuration file in `generators/chipyard/src/main/scala/chipyard/config/`.
   ```scala
    // For Example:
    package chipyard

    import org.chipsalliance.cde.config.Config

    // New config: Ara8192 + Gemmini + Rocket
    class Ara8192GemminiRocketConfig extends Config(
        new ara.WithAraRocketVectorUnit(8192, 4) ++     // vLen=8192, 4 lanes
        new gemmini.DefaultGemminiConfig ++              // Standard Gemmini
        new freechips.rocketchip.rocket.WithNHugeCores(1) ++
        new chipyard.config.WithSystemBusWidth(128) ++
        new chipyard.config.AbstractConfig)
    ```

   - Extend the base Rocket configuration and mix in Ara and Gemmini traits.

2. **Modify Build Files:**
    - Update `build.sbt` to include any new dependencies required for Ara and Gemmini.

3. **Generate the SoC:**
    - Activate the Chipyard environment:
        ```bash
        cd ./chipyard
        source env.sh
        ```
    - Run the Chipyard build system to generate the new SoC with the command:
        ```bash
        cd ./chipyard/sims/verilator
        make CONFIG=Ara8192GemminiRocketConfig USE_ARA=1 -j$(nproc)
        ```

        Once this command completes, the new configuration will be built and ready for simulation. You can see the generated build files in the `sims/verilator` directory and the verilog RTL files in `sims/verilator/generated-src/`.

4. **Run Tests:**
    - Compile and run performance tests to validate the new configuration.
        ```bash
        cd ../../generators/gemmini/software/gemmini-rocc-tests
        ./build.sh  
        ```
        ```bash
        cd ../../sims/verilator
        time make CONFIG=Ara8192GemminiRocketConfig USE_ARA=1 run-binary \
        BINARY=../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/ara_gemmini_scalar_compare-baremetal
        ```
---

### Understanding Chipyard Configuration Fragments

Chipyard uses a powerful configuration system based on **Config Fragments** - small, reusable building blocks that define your SoC. Understanding this system is essential for creating custom chip configurations.

#### What are Config Fragments?

**Config Fragments** are small Scala classes that modify ONE specific aspect of your SoC design. Each fragment:
- Extends the `Config` class
- Sets, overrides, or modifies specific configuration parameters
- Can be chained together using the `++` operator
- Acts like a layer that builds on previous configurations

Think of fragments like LEGO blocks - each one adds or modifies a specific feature, and you combine them to build your complete system.

#### How Configs Are Built

Configurations are read **RIGHT-TO-LEFT**. The rightmost config provides the base, and each fragment to the left adds or overrides settings:

```scala
class MyConfig extends Config(
  FragmentA ++    // Applied LAST (highest priority)
  FragmentB ++    // Applied second
  FragmentC ++    // Applied first (can be overridden by A or B)
  BaseConfig      // Foundation (lowest priority)
)
```

**Example from your Ara+Gemmini configuration:**

```scala
class Ara4096GemminiRocketConfig extends Config(
  new ara.WithAraRocketVectorUnit(4096, 2) ++    // 1. Add Ara vector unit (vLen=4096, 2 lanes)
  new gemmini.DefaultGemminiConfig ++             // 2. Add Gemmini systolic array
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++  // 3. Add 1 Rocket core
  new chipyard.config.WithSystemBusWidth(128) ++ // 4. Set system bus to 128 bits
  new chipyard.config.AbstractConfig)            // 5. Base configuration
```

Reading order:
1. `AbstractConfig` sets up the base SoC infrastructure (buses, clocks, I/O)
2. `WithSystemBusWidth(128)` modifies the system bus width to 128 bits
3. `WithNHugeCores(1)` adds a single high-performance Rocket core
4. `DefaultGemminiConfig` adds the Gemmini systolic array accelerator
5. `WithAraRocketVectorUnit(4096, 2)` adds Ara vector unit and attaches it to Rocket

#### What is AbstractConfig?

`AbstractConfig` is Chipyard's **base configuration** that provides:
- Test harness setup (simulation infrastructure)
- I/O cell configuration (UART, Debug, SPI, etc.)
- Memory subsystem (backing memory, scratchpads)
- Bus topology (system bus, peripheral bus, etc.)
- Clock and reset infrastructure
- Default frequencies (500 MHz for all buses)

**Important:** `AbstractConfig` contains **NO CPU CORES** - it's an empty system. You must add at least one tile/core fragment to make it functional.

#### Anatomy of a Config Fragment

Here's a real fragment from Chipyard that sets the system bus width:

```scala
// From generators/chipyard/src/main/scala/config/fragments/SubsystemFragments.scala
class WithSystemBusWidth(bitWidth: Int) extends Config((site, here, up) => {
  case SystemBusKey => up(SystemBusKey, site).copy(beatBytes=bitWidth/8)
})
```

**Breaking it down:**
- `bitWidth: Int` - Parameter you pass when using the fragment
- `(site, here, up) =>` - Access to config chain:
  - `site`: Complete configuration chain
  - `here`: Current fragment
  - `up`: Previous configuration in the chain
- `case SystemBusKey =>` - Pattern match on the config parameter to modify
- `up(SystemBusKey, site).copy(beatBytes=bitWidth/8)` - Get previous value and modify it

#### Common Config Fragment Examples

**1. Single Rocket Core (Simplest Config):**
```scala
class RocketConfig extends Config(
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++  // Add 1 big Rocket core
  new chipyard.config.AbstractConfig)
```

**2. Dual-Core Rocket:**
```scala
class DualRocketConfig extends Config(
  new freechips.rocketchip.rocket.WithNHugeCores(2) ++  // Add 2 cores
  new chipyard.config.AbstractConfig)
```

**3. Tiny Rocket (Minimal Configuration):**
```scala
class TinyRocketConfig extends Config(
  new testchipip.soc.WithNoScratchpads ++                         // Remove scratchpads
  new freechips.rocketchip.subsystem.WithIncoherentBusTopology ++ // Incoherent bus
  new freechips.rocketchip.subsystem.WithNBanks(0) ++             // No L2 cache
  new freechips.rocketchip.subsystem.WithNoMemPort ++             // No backing memory
  new freechips.rocketchip.rocket.With1TinyCore ++                // Tiny core (smaller)
  new chipyard.config.AbstractConfig)
```

**4. Custom Bus Width:**
```scala
class WideRocketConfig extends Config(
  new chipyard.config.WithSystemBusWidth(256) ++  // 256-bit system bus
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)
```

#### How Generators Query Configurations

Hardware generators query the config object at elaboration time to determine what to build:

```scala
// Inside a hardware generator
class MyModule(implicit p: Parameters) extends Module {
  val busWidth = p(SystemBusKey).beatBytes * 8  // Query the system bus width
  // Use busWidth to parameterize the design...
}
```

If you used `WithSystemBusWidth(128)`, the generator reads `128`. Otherwise, it uses the default from `AbstractConfig`.

#### Creating Your Own Fragment

**Example: Custom fragment to set Gemmini dimensions**

```scala
package chipyard.config

import org.chipsalliance.cde.config.Config
import gemmini._

class WithCustomGemmini(dim: Int, meshRows: Int, meshCols: Int) extends Config((site, here, up) => {
  case GemminiKey => up(GemminiKey, site).copy(
    meshRows = meshRows,
    meshCols = meshCols,
    tileRows = dim,
    tileColumns = dim
  )
})

// Use it:
class MyGemminiConfig extends Config(
  new WithCustomGemmini(dim=16, meshRows=16, meshCols=16) ++
  new gemmini.DefaultGemminiConfig ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.AbstractConfig)
```

#### Practical Guide: Building Custom Configs

**Step 1:** Choose your base (usually `AbstractConfig`)

**Step 2:** Add core fragments:
- `freechips.rocketchip.rocket.WithNHugeCores(n)` - Rocket cores
- `freechips.rocketchip.rocket.WithNSmallCores(n)` - Small Rocket cores
- `boom.common.WithNBoomCores(n)` - BOOM out-of-order cores

**Step 3:** Add accelerators:
- `ara.WithAraRocketVectorUnit(vlen, lanes)` - Ara vector unit
- `gemmini.DefaultGemminiConfig` - Gemmini systolic array
- Custom accelerator fragments

**Step 4:** Customize subsystem:
- `chipyard.config.WithSystemBusWidth(bits)` - Bus width
- `freechips.rocketchip.subsystem.WithNBanks(n)` - L2 cache banks
- `freechips.rocketchip.subsystem.WithNMemoryChannels(n)` - Memory channels

**Step 5:** Build and test:
```bash
cd chipyard/sims/verilator
make CONFIG=YourCustomConfig USE_ARA=1 -j$(nproc)
```

#### Key Takeaways

| Concept | Meaning |
|---------|---------|
| **Fragment** | A small `Config` class that modifies ONE specific aspect |
| **`++` operator** | Chains fragments together; left side has higher priority |
| **AbstractConfig** | Base configuration with infrastructure but NO cores |
| **Config Keys** | Named parameters (e.g., `SystemBusKey`, `GemminiKey`) that generators query |
| **Right-to-Left** | Configs are evaluated from right (base) to left (overrides) |

**Remember:** Earlier (leftmost) fragments override later (rightmost) ones, so order matters!

---

### Testing the Configuration wth Custom Testcases

Refer the [WRITING_CUSTOM_TESTCASES.md](Learn%20from%20Basics/WRITING_CUSTOM_TESTCASES.md) for detailed instructions on creating/running custom tests benchmarks for custom configuration.

In short, after building your configuration, we have to build the testcases and then run them on the generated SoC. 

#### Predefined Testcases
There are predefined testcases under `generators/gemmini/software/gemmini-rocc-tests/` that you can use or modify. Following are the steps to run them:

1. **Build the Testcases:**
    ```bash
    cd ./chipyard/generators/gemmini/software/gemmini-rocc-tests
    ./build.sh  
    ```
2. **Run the Testcases:**
    ```bash
    cd ../../sims/verilator
    time make CONFIG=YourCustomConfig USE_ARA=1 run-binary \
    BINARY=../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/your_testcase-baremetal
    ```

Example:
```bash
time make CONFIG=Ara8192GemminiRocketConfig USE_ARA=1 run-binary \
BINARY=../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/ara_gemmini_scalar_compare-baremetal
```
*time is just for measuring the duration for running make command.*

#### Custom Testcases

Custom Testcases are defined inside the `chipyard/software/` directory. These can be compiled and run following these steps:

1. **Build the Testcases:**
    ```bash
    cd ./software/ara-gemmini-benchmarks
    source ../../env.sh
    make
    ```

2. **Run the Testcases:**
    ```bash
    cd ../../sims/verilator
    time make CONFIG=YourCustomConfig USE_ARA=1 run-binary \
    BINARY=../../software/your_custom_testcase-baremetal
    ```

### ./build.sh VS make

As we can notice that building tests in `gemmini-rocc-tests` requires `./build.sh` while custom tests in `software/` use `make`. Here's why:

#### Why Different Build Commands?

**1. gemmini-rocc-tests Uses `./build.sh`**

**Location:** `generators/gemmini/software/gemmini-rocc-tests/`

This is the **original Gemmini test suite** from the upstream Gemmini repository. It has a complex build system because:

```bash
# What build.sh does (simplified):
#!/bin/bash

# 1. Configure the build (like autotools configure)
cd build
../configure --with-arch=rv64gcv --with-isa=rv64gcv

# 2. Run make in multiple subdirectories
make -C bareMetalC
make -C imagenet
make -C mlps
# ... etc

# 3. Handle dependencies between test directories
# 4. Copy files to correct locations
```

**What `build.sh` does:**
- **Runs configure script** - Sets up build environment, detects toolchain
- **Builds multiple test suites** - bareMetalC, imagenet, mlps, etc.
- **Handles dependencies** - Some tests need libraries built first
- **Copies files** - Moves binaries to correct locations

**Directory structure:**
```
gemmini-rocc-tests/
├── build.sh              # Main build script
├── configure             # Configure script (autotools-style)
├── Makefile              # Top-level Makefile
├── bareMetalC/           # Subdirectory with its own Makefile
│   └── Makefile
├── imagenet/             # Another subdirectory
│   └── Makefile
└── ...
```

**2. Custom Tests Use `make`**

**Location:** `software/ara-gemmini-benchmarks/`

This is **your simplified, single-purpose test directory** with a flat, straightforward structure:

```bash
# Your Makefile (simplified):
TESTS := ara_gemmini_compare ara_gemmini_scalar_compare

all: $(TESTS)

%: benchmarks/%.c
    $(CC) $(CFLAGS) $< $(RUNTIME_FILES) $(LDFLAGS) -o build/$@-baremetal
```

**What `make` does:**
- **Single Makefile** - Everything in one place
- **Flat structure** - All tests in `benchmarks/`, all outputs in `build/`
- **Simple pattern rules** - One rule compiles all tests
- **No configure step** - Hardcoded paths and flags
- **No subdirectories** - No recursive make needed

**Directory structure:**
```
ara-gemmini-benchmarks/
├── Makefile              # Single Makefile for everything
├── benchmarks/           # Just source files, no Makefiles
│   ├── test1.c
│   └── test2.c
└── build/                # Output directory
```

#### Side-by-Side Comparison

| Aspect | gemmini-rocc-tests | ara-gemmini-benchmarks |
|--------|-------------------|------------------------|
| **Build command** | `./build.sh` | `make` |
| **Makefile structure** | Multiple Makefiles in subdirs | Single Makefile |
| **Configure step** | Yes (via configure script) | No (hardcoded) |
| **Test organization** | Multiple categories (bareMetalC, imagenet, etc.) | Single category (benchmarks/) |
| **Complexity** | High (upstream project) | Low (custom, simplified) |
| **Dependencies** | Complex (between test suites) | Simple (independent tests) |
| **Header generation** | No (reads existing header) | No (copied from gemmini) |
| **Build time** | ~5 minutes (builds 50+ tests) | ~30 seconds (builds 2-3 tests) |

#### Could You Use Just `make` in gemmini-rocc-tests?

**Yes, but** you'd need to navigate to specific subdirectories:

```bash
# Instead of:
./build.sh

# You could do:
cd build/bareMetalC
make

cd ../imagenet  
make

# But build.sh does all this for you automatically!
```

#### Could You Use `build.sh` for Custom Tests?

**Yes, but** it's overkill for simple tests. You'd need to:
- Create a configure script
- Set up recursive make
- Add complexity you don't need

```bash
# Your simple case:
make                    # Builds everything in 30 seconds

# vs. if you used build.sh style:
./configure             # Configure build
./build.sh              # Run complex build script
                        # Would take longer for no benefit
```

#### Understanding gemmini_params.h Generation

**The header is generated by:** `make CONFIG=... USE_ARA=1` when you build the hardware configuration.

**What is gemmini_params.h?**

It's a **bridge between hardware and software** containing parameters like:

```c
#define DIM 16                    // Systolic array is 16×16
#define BANK_NUM 4                // Scratchpad has 4 banks
#define BANK_ROWS 4096            // Each bank has 4096 rows
typedef int8_t elem_t;            // Elements are INT8
typedef int32_t acc_t;            // Accumulators are INT32
```

**The Complete Generation Flow:**

```
Step 1: Build Hardware Configuration (GENERATES the header)
────────────────────────────────────────────────────────────
cd ./chipyard/sims/verilator
make CONFIG=Ara8192GemminiRocketConfig USE_ARA=1 -j$(nproc)
    │
    ├─► Chisel Elaboration runs (Scala → FIRRTL)
    ├─► Gemmini Generator executes
    ├─► ✅ CREATES gemmini_params.h automatically
    │    Location: generators/gemmini/software/gemmini-rocc-tests/include/
    │    Content:  #define DIM 16, typedef int8_t elem_t, etc.
    ├─► Generates Verilog (.v files)
    └─► Builds Verilator simulator

Step 2: Build Test Binaries (READS the existing header)
────────────────────────────────────────────────────────────
cd ../../generators/gemmini/software/gemmini-rocc-tests
./build.sh
    │
    ├─► Reads gemmini_params.h (generated in Step 1)
    ├─► Compiles test C files using the header
    └─► Produces binaries in build/bareMetalC/

Step 3: Custom Tests (COPY the header manually)
────────────────────────────────────────────────────────────
cp generators/gemmini/.../gemmini_params.h \
   software/ara-gemmini-benchmarks/include/

cd software/ara-gemmini-benchmarks
make
    │
    ├─► Reads gemmini_params.h (copied from Step 1)
    ├─► Compiles custom test C files
    └─► Produces binaries in build/
```

**Key Point:** The hardware build (`make CONFIG=...`) is the ONLY command that generates `gemmini_params.h`. Both test build systems (`./build.sh` and custom `make`) only read and use the existing header.

**Why this matters:**
1. **You must build the hardware configuration first** before building any tests
2. If you change Gemmini parameters (mesh size, data type, etc.), you must rebuild the hardware to regenerate the header
3. Both `./build.sh` and custom `make` will fail if `gemmini_params.h` doesn't exist

**How it's generated during Chisel elaboration:**

```scala
// Inside generators/gemmini/src/main/scala/gemmini/Gemmini.scala (simplified)
class GemminiGenerator {
  def elaborate(config: GemminiConfig): Unit = {
    // Generate Verilog
    generateVerilog(config)
    
    // ✅ Generate header file
    val headerContent = s"""
      #ifndef GEMMINI_PARAMS_H
      #define GEMMINI_PARAMS_H
      
      #define DIM ${config.meshRows}
      #define BANK_NUM ${config.sp_banks}
      typedef ${configToC(config.inputType)} elem_t;
      
      #endif
    """
    writeFile("software/.../gemmini_params.h", headerContent)
  }
}
```

**For gemmini-rocc-tests:**
- Header is automatically available after hardware build
- `./build.sh` reads it from `include/gemmini_params.h`

**For custom tests:**
- You manually copy the generated header once
- Your `make` reads it from `software/ara-gemmini-benchmarks/include/gemmini_params.h`

#### When to Use Which?

**Use gemmini-rocc-tests (`./build.sh`) when:**
- Running standard Gemmini tests
- Need pre-built benchmarks (ResNet, MobileNet, etc.)
- Testing basic Gemmini functionality
- Want comprehensive test coverage

**Use ara-gemmini-benchmarks (`make`) when:**
- Writing custom Ara+Gemmini comparison tests
- Need quick iteration on new benchmarks
- Want simple, maintainable test code
- Testing specific performance scenarios

#### Summary

Both approaches are valid:
- **`./build.sh`** = "Enterprise" approach - One script does everything, handles complexity
- **`make`** = "Lean" approach - Just compile what you wrote, fast and simple

Choose based on your needs: comprehensive testing vs. quick custom benchmarks.

---

### Modifying the Architecture

```bash
# 1. Modify Gemmini
vim generators/gemmini/src/main/scala/gemmini/Configs.scala

# 2. Rebuild hardware (auto-updates header via symlink)
cd sims/verilator
make CONFIG=Ara4096GemminiRocketConfig USE_ARA=1 -j$(nproc)

# 3. Build tests (symlink already points to new header!)
cd ../../software/ara-gemmini-benchmarks
source ../../env.sh
make clean && make

# 4. Run test
cd ../../sims/verilator
./simulator-chipyard.harness-Ara4096GemminiRocketConfig \
  ../../software/ara-gemmini-benchmarks/build/your_test-baremetal
```