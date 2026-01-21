# Chipyard and Gemmini Storage Usage Report

**Report Date:** November 9, 2025  
**Location:** /ssd_scratch

---

## Summary

| Component | Size | Description |
|-----------|------|-------------|
| **Total Chipyard** | **31 GB** | Complete Chipyard installation |
| **CIRCT/Firtool** | **767 MB** | CIRCT toolchain for FIRRTL compilation |
| **Combined Total** | **~32 GB** | Total storage consumed |

---

## Detailed Breakdown

### Overall Disk Usage
```
Filesystem: /dev/sdc1
Total Size: 880 GB
Used: 266 GB (31%)
Available: 615 GB
```

### Chipyard Directory (31 GB)

#### Top-Level Components
```
8.3 GB   .git                    # Git repository and history
6.7 GB   tools                   # Development tools (DRAMSim2, etc.)
5.2 GB   .conda-env              # Conda environment with dependencies
3.1 GB   software                # Software tools and utilities
2.3 GB   sims                    # Simulation environments
2.0 GB   generators              # Hardware generators (Gemmini, etc.)
1.8 GB   toolchains              # RISC-V toolchains
447 MB   .classpath_cache        # Java/Scala classpath cache
347 MB   .conda-lock-env         # Conda lock files
64 MB    .sbt                    # Scala Build Tool cache
63 MB    .ivy2                   # Ivy dependency cache
4.6 MB   docs                    # Documentation
2.2 MB   fpga                    # FPGA-related files
1.6 MB   project                 # Build configuration
1.4 MB   scripts                 # Utility scripts
904 KB   .bloop                  # Bloop build server
580 KB   tests                   # Test files
304 KB   conda-reqs              # Conda requirements
264 KB   .github                 # GitHub workflows
```

### Simulation Directory (2.3 GB)
```
1.3 GB   firesim                 # FireSim for FPGA simulation
997 MB   verilator               # Verilator simulation
  ├─ 915 MB   generated-src     # Generated Verilog sources
  └─ 44 MB    output            # Test outputs and VCD files
```

### Generators Directory (2.0 GB)
```
1.1 GB   radiance                # Radiance generator
682 MB   radiance/target         # Compiled artifacts
409 MB   radiance/src            # Source code
255 MB   nvdla                   # NVDLA deep learning accelerator
255 MB   gemmini                 # Gemmini systolic array accelerator
94 MB    saturn                  # Saturn vector processor
51 MB    cva6                    # CVA6 RISC-V core
49 MB    rocket-chip             # Rocket Chip framework
```

### Gemmini Detailed Breakdown (255 MB)
```
243 MB   software/gemmini-rocc-tests
  ├─ 163 MB   build             # Compiled test binaries
  │   ├─ bareMetalC/            # Baremetal test executables
  │   ├─ linux/                 # Linux test executables
  │   └─ pk/                    # Proxy kernel test executables
  ├─ 76 MB    imagenet          # ImageNet test data
  └─ 3.1 MB   riscv-tests       # RISC-V test suite

12 MB    src                     # Gemmini Scala source code
  └─ 11 MB    target/scala-2.13 # Compiled Scala artifacts

768 KB   src/main/scala          # Main Scala sources
476 KB   img                     # Documentation images
192 KB   software/libgemmini     # Gemmini library
```

### Tools Directory (6.7 GB)
```
Contains:
- DRAMSim2 (memory simulator)
- Other verification and testing tools
```

### Conda Environment (5.2 GB)
```
Includes:
- Python packages
- RISC-V tools
- Verilator
- Scala/SBT
- Java runtime
- Various dependencies
```

---

## Storage Optimization Opportunities

### Safe to Clean (if needed)

1. **Git History** (8.3 GB)
   ```bash
   # NOT RECOMMENDED unless you need space
   # This removes git history
   rm -rf /ssd_scratch/chipyard/.git
   ```

2. **Build Caches** (~500 MB)
   ```bash
   cd /ssd_scratch/chipyard
   rm -rf .classpath_cache
   rm -rf .sbt/boot
   rm -rf .ivy2/cache
   ```

3. **Old Test Outputs** (44 MB)
   ```bash
   cd /ssd_scratch/chipyard/sims/verilator
   rm -rf output/*.vcd  # Remove waveform files
   rm -rf output/*.out  # Remove old outputs
   ```

4. **Temporary Build Files**
   ```bash
   cd /ssd_scratch/chipyard
   make -C sims/verilator clean  # Removes generated sources
   # NOTE: This requires rebuild (1-2 hours)
   ```

### NOT Safe to Remove

❌ `.conda-env` (5.2 GB) - Required for running Chipyard  
❌ `tools` (6.7 GB) - Required development tools  
❌ `generators/gemmini` (255 MB) - Gemmini accelerator  
❌ `sims/verilator/simulator-*` - Compiled simulator binaries  
❌ `toolchains` (1.8 GB) - RISC-V cross-compilation toolchain  

---

## Component-Specific Usage

### Gemmini Test Binaries (163 MB)
```
Build types:
- bareMetalC/    : Bare-metal tests (fastest, recommended)
- linux/         : Linux tests (requires Linux kernel)
- pk/            : Proxy kernel tests (slower than baremetal)

Each test type has ~50+ test executables
Average size per test: ~600 KB to 20 KB
```

### Simulation Outputs (44 MB)
```
- VCD waveforms  : Can grow very large (7 MB per test)
- Log files      : Small (~1-2 KB per test)
- Out files      : Small (~2-3 KB per test)
```

---

## Growth Projections

### Normal Usage
- **Test outputs:** +10-50 MB per day (if running many tests)
- **Build caches:** Minimal growth after initial setup
- **VCD files:** Can grow rapidly if saving waveforms (disable if not needed)

### Heavy Development
- **Additional configs:** +1-2 GB per new SoC configuration build
- **Multiple simulations:** +500 MB - 1 GB per configuration
- **Git branches:** +100-500 MB per branch with significant changes

---

## Disk Space Recommendations

### Minimum Requirements
- **Basic Setup:** 35 GB (current usage)
- **Development:** 50 GB (with headroom for builds)
- **Heavy Development:** 100 GB (multiple configurations)

### Current Status
```
Available Space: 615 GB ✓ (Plenty of room)
Current Usage: 266 GB (31%)
Chipyard Usage: 31 GB (4% of total disk)
```

---

## Cleanup Commands Reference

### Clean Verilator Build (Frees ~900 MB)
```bash
cd /ssd_scratch/chipyard
make -C sims/verilator clean
# NOTE: Requires 1-2 hour rebuild
```

### Clean All Simulation Outputs
```bash
cd /ssd_scratch/chipyard/sims/verilator
rm -rf output/*
```

### Clean Scala Build Caches
```bash
cd /ssd_scratch/chipyard
rm -rf .classpath_cache
rm -rf .bloop
rm -rf target
rm -rf project/target
```

### Clean Gemmini Test Builds (Frees ~163 MB)
```bash
cd /ssd_scratch/chipyard/generators/gemmini/software/gemmini-rocc-tests
make clean
# NOTE: Requires rebuild with ./build.sh
```

### Clean All Conda Caches
```bash
conda clean --all
```

---

## Monitoring Commands

### Check Total Usage
```bash
du -sh /ssd_scratch/chipyard
```

### Check Disk Space
```bash
df -h /ssd_scratch
```

### Find Largest Directories
```bash
du -h /ssd_scratch/chipyard | sort -hr | head -20
```

### Find Large Files
```bash
find /ssd_scratch/chipyard -type f -size +100M -exec ls -lh {} \;
```

### Monitor VCD Growth
```bash
du -sh /ssd_scratch/chipyard/sims/verilator/output/*.vcd
```

---

## Comparison with Other Installations

### Typical Sizes
- **Minimal Chipyard:** ~15 GB (without heavy tools)
- **Standard Chipyard:** ~30 GB (our current setup)
- **Full Development:** ~50-100 GB (multiple configs, FireSim, etc.)
- **Research Setup:** 100-200 GB (many SoC variants, extensive testing)

### Individual Components
- **Verilator alone:** ~100 MB
- **RISC-V Toolchain:** ~1-2 GB
- **Conda Environment:** ~3-5 GB
- **Gemmini alone:** ~255 MB
- **Single SoC build:** ~1-2 GB

---

## Best Practices

1. **Disable VCD Generation** (unless debugging):
   ```bash
   # Remove +vcdfile argument from test commands
   # Saves ~7 MB per test run
   ```

2. **Regular Cleanup** of old outputs:
   ```bash
   # Weekly cleanup
   find /ssd_scratch/chipyard/sims/verilator/output -mtime +7 -delete
   ```

3. **Use Baremetal Tests** (smaller than pk/linux variants)

4. **Clean Between Major Builds**:
   ```bash
   make -C sims/verilator clean
   # Before building new configuration
   ```

5. **Monitor Disk Usage**:
   ```bash
   # Add to cron for weekly reports
   df -h /ssd_scratch | mail -s "Disk Usage Report" user@example.com
   ```

---

## CIRCT/Firtool (767 MB)

Located at: `/ssd_scratch/circt-install`

```
767 MB   circt-install/
  ├─ bin/         # Executables (firtool, etc.)
  ├─ lib/         # Libraries
  └─ include/     # Header files
```

This is a standalone installation and can be removed/reinstalled independently of Chipyard.

---

**Report Generated:** November 9, 2025  
**Next Review:** Recommended monthly or when disk usage exceeds 50%
