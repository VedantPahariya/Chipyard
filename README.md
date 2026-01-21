# Chipyard

## Documentation Files

### Learn from Basics/
- **ARA_GEMMINI_INTEGRATION_BUGS_AND_FIXES.md** : Detailed documentation of all 5 bugs encountered and fixes applied during Ara+Gemmini+Rocket integration
- **ARA_GEMMINI_QUICKSTART.md** : Quick start guide with step-by-step instructions for running tests and creating custom configs
- **DOCUMENTATION_MAP.txt** : Visual documentation map with quick reference for all tasks and file locations
- **README_ARA_GEMMINI_COMPLETE.md** : Complete summary of Ara+Gemmini+Rocket integration with performance metrics and test results
- **ROCKET_ARA_GEMMINI_SOC_GUIDE.md** : Comprehensive architecture guide covering system components, configuration, and performance tuning

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

### Modifying the Architecture
