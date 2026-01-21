# Complete Guide: Changing Gemmini Dimension

## Current Configuration
- **Current dim:** 16 (16×16 systolic array = 256 PEs)
- **Configuration file:** `/ssd_scratch/chipyard/generators/gemmini/src/main/scala/gemmini/Configs.scala`
- **Parameters:** `meshRows = 16, meshColumns = 16`

---

## Method 1: Create Custom Configuration (RECOMMENDED)

### Step 1: Add a new config to Gemmini Configs.scala

Edit: `/ssd_scratch/chipyard/generators/gemmini/src/main/scala/gemmini/Configs.scala`

Add your custom config to the `GemminiConfigs` object (around line 240):

```scala
  // Add this to GemminiConfigs object
  val dim8Config = defaultConfig.copy(
    meshRows = 8,
    meshColumns = 8,
    sp_capacity = CapacityInKilobytes(64),
    acc_capacity = CapacityInKilobytes(16)
  )

  val dim32Config = defaultConfig.copy(
    meshRows = 32,
    meshColumns = 32,
    sp_capacity = CapacityInKilobytes(512),
    acc_capacity = CapacityInKilobytes(128)
  )

  val dim4Config = defaultConfig.copy(
    meshRows = 4,
    meshColumns = 4,
    sp_capacity = CapacityInKilobytes(16),
    acc_capacity = CapacityInKilobytes(8)
  )
```

### Step 2: Add Chipyard Config Class

Edit: `/ssd_scratch/chipyard/generators/gemmini/chipyard/GemminiConfigs.scala`

Add after the existing configs:

```scala
// 8x8 Gemmini Configuration
class Gemmini8x8RocketConfig extends Config(
  new gemmini.DefaultGemminiConfig(gemmini.GemminiConfigs.dim8Config) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)

// 32x32 Gemmini Configuration  
class Gemmini32x32RocketConfig extends Config(
  new gemmini.DefaultGemminiConfig(gemmini.GemminiConfigs.dim32Config) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)

// 4x4 Gemmini Configuration
class Gemmini4x4RocketConfig extends Config(
  new gemmini.DefaultGemminiConfig(gemmini.GemminiConfigs.dim4Config) ++
  new freechips.rocketchip.rocket.WithNHugeCores(1) ++
  new chipyard.config.WithSystemBusWidth(128) ++
  new chipyard.config.AbstractConfig)
```

### Step 3: Build the Simulator

```bash
cd /ssd_scratch/chipyard
source env.sh

# Build with 8x8 configuration
make -C sims/verilator CONFIG=Gemmini8x8RocketConfig

# Or build with 32x32 configuration
make -C sims/verilator CONFIG=Gemmini32x32RocketConfig

# Or build with 4x4 configuration
make -C sims/verilator CONFIG=Gemmini4x4RocketConfig
```

**Build time:** ~2 hours per configuration

### Step 4: Run Tests

```bash
# The simulator name will include your config
cd /ssd_scratch/chipyard/sims/verilator

# For 8x8:
./simulator-chipyard.harness-Gemmini8x8RocketConfig \
  +permissive ../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/template-baremetal

# For Spike, you need to rebuild it with the new config (complex)
# Or use the existing spike (it will use dim=16 from hardware)
```

---

## Method 2: Modify Default Config Directly (QUICK TEST)

### Step 1: Edit the default config

Edit: `/ssd_scratch/chipyard/generators/gemmini/src/main/scala/gemmini/Configs.scala`

Find around line 34:

```scala
val defaultConfig = GemminiArrayConfig[SInt, Float, Float](
    // ... other params ...
    
    // Change these two lines:
    meshRows = 16,        // <-- Change this (e.g., to 8, 32, 4)
    meshColumns = 16,     // <-- Change this (e.g., to 8, 32, 4)
    
    // ... rest of config ...
```

**Example for 8x8:**
```scala
    meshRows = 8,
    meshColumns = 8,
```

### Step 2: Adjust memory capacity (important!)

When changing dimension, you should also adjust scratchpad and accumulator sizes:

| Dimension | meshRows/Columns | sp_capacity (KB) | acc_capacity (KB) |
|-----------|------------------|------------------|-------------------|
| 4×4       | 4, 4             | 16               | 8                 |
| 8×8       | 8, 8             | 64               | 16                |
| 16×16     | 16, 16           | 256              | 64                |
| 32×32     | 32, 32           | 512              | 128               |

```scala
    meshRows = 8,
    meshColumns = 8,
    
    // Also update these:
    sp_capacity = CapacityInKilobytes(64),   // Reduced from 256
    acc_capacity = CapacityInKilobytes(16),  // Reduced from 64
```

### Step 3: Rebuild

```bash
cd /ssd_scratch/chipyard
source env.sh

# Clean previous build
make -C sims/verilator clean

# Rebuild with new dimension
make -C sims/verilator CONFIG=GemminiRocketConfig
```

---

## Method 3: Use Existing Configurations

Some pre-defined configs already exist:

### Large 32×32 Config
In `Configs.scala` around line 241:
```scala
val largeChipConfig = chipConfig.copy(
    meshRows=32, meshColumns=32
)
```

### Small 8×8 Config (in DualGemmini)
In `Configs.scala` around line 327:
```scala
meshColumns = 8, meshRows = 8,
```

To use these, you'd need to create wrapper configs similar to Method 1.

---

## Verification

After building with a new dimension:

### Test with Spike:
```bash
spike --extension=gemmini \
  /ssd_scratch/chipyard/generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/template-baremetal
```

Output should show:
```
Gemmini extension configured with:
    dim = 8  # or whatever dimension you chose
```

### Test with Verilator:
```bash
cd /ssd_scratch/chipyard/sims/verilator
make CONFIG=Gemmini8x8RocketConfig run-binary \
  BINARY=../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/template-baremetal
```

---

## Important Notes

### 1. **Memory Scaling**
Larger dimensions need more scratchpad/accumulator memory:
- 4×4: Small (~24 KB total)
- 8×8: Medium (~80 KB total)  
- 16×16: Default (~320 KB total)
- 32×32: Large (~640 KB total)

### 2. **Build Time**
Each configuration takes ~2 hours to build the Verilator simulator.

### 3. **Performance Impact**
- **Smaller dimension (4×4, 8×8):** Slower computation, less hardware
- **Larger dimension (32×32):** Faster computation, more hardware, longer build times

### 4. **Test Compatibility**
Some tests are optimized for dim=16. They will still run on other dimensions but may not be optimal.

### 5. **Spike vs Verilator**
- **Spike:** Reads dimension from ISA extension, may not match hardware config
- **Verilator:** Uses actual hardware dimension from Chisel generation

---

## Quick Reference Commands

### Check current dimension:
```bash
grep -A 2 "meshRows" /ssd_scratch/chipyard/generators/gemmini/src/main/scala/gemmini/Configs.scala | head -5
```

### List available simulators:
```bash
ls -lh /ssd_scratch/chipyard/sims/verilator/simulator-*
```

### Clean and rebuild:
```bash
cd /ssd_scratch/chipyard
source env.sh
make -C sims/verilator clean
make -C sims/verilator CONFIG=<YourConfig>
```

---

## Recommended Approach

**For testing different dimensions:**

1. ✅ **Method 1** - Create custom configs (best for production)
2. ✅ **Method 2** - Modify default (quick for testing)
3. ⚠️ **Don't** modify existing configs directly (breaks reproducibility)

**Start with:** 8×8 configuration (faster build, good for testing)
**Production:** Keep 16×16 or 32×32 (better performance)

