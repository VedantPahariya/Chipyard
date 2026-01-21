# Run Gemmini

## Activate the Environment

```bash
export CHIPYARD_DIR=$(pwd)
export PATH=$CHIPYARD_DIR/circt-install/bin:$PATH
export CIRCT_INSTALL_PATH=$CHIPYARD_DIR/circt-install

# Verify installation
firtool --version

cd $CHIPYARD_DIR/chipyard
source env.sh
```

## Configuration Files

The Gemmini configurations are located in:
- `/chipyard/generators/gemmini/chipyard/GemminiConfigs.scala`
- `/chipyard/generators/gemmini/src/main/scala/gemmini/Configs.scala`

After changing the configuration files, run Verilator to simulate the hardware.

## Building the Hardware

Whenever making any changes in the hardware of Gemmini (i.e., any change in Scala code inside `src`), rebuild the hardware using the following instructions:

```bash
cd $CHIPYARD_DIR/chipyard
export VERILATOR_THREADS=10
make -C sims/verilator clean && make -C sims/verilator CONFIG=GemminiRocketConfig -j$(nproc)
```

```bash
cd $CHIPYARD_DIR/chipyard
export VERILATOR_THREADS=10
make -C sims/verilator clean && make -C sims/verilator CONFIG=BitNetGemminiRocketConfig -j$(nproc)
```

### Role of VERILATOR_THREADS

If `VERILATOR_THREADS=4`, it takes fewer clock cycles (about 4 times less) because it releases 4 threads at a time.

## Rebuild Tests

**Note:** Run this command only after building the hardware.

```bash
cd /ssd_scratch/chipyard/generators/gemmini/software/gemmini-rocc-tests
./build.sh
```

## Testing on Verilator

On normal Gemmini configuration:
```bash
cd /ssd_scratch/chipyard/sims/verilator && source ../../env.sh
time make CONFIG=GemminiRocketConfig run-binary \
BINARY=../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/template-baremetal
```

On BitNet Gemmini configuration:
```bash
cd /ssd_scratch/chipyard/sims/verilator && source ../../env.sh
time make CONFIG=BitNetGemminiRocketConfig run-binary \
BINARY=../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/template-baremetal
```

## Rebuilding Libgemmini

For running on Spike while changing the configuration of the architecture, rebuild Libgemmini:

To run the matrix multiplication function (template) on Spike, modify the file `gemmini_params.h` and then run:

```bash
cd /ssd_scratch/chipyard/generators/gemmini/software/libgemmini
source /ssd_scratch/chipyard/env.sh
make clean && make install
```

## Testing on Spike

```bash
cd /ssd_scratch/chipyard/sims/verilator && source ../../env.sh
spike --extension=gemmini ../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/template-baremetal

cd /ssd_scratch/chipyard/sims/verilator && source ../../env.sh
spike --extension=gemmini ../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/bitnet_matmul-baremetal
```

cd $CHIPYARD_DIR/chipyard
export VERILATOR_THREADS=4
make -C sims/verilator clean && make -C sims/verilator CONFIG=LeanSaturnGemminiRocketConfig -j$(nproc)

cd /ssd_scratch/chipyard/generators/gemmini/software/gemmini-rocc-tests
./build.sh -j$(nproc)

cd /ssd_scratch/chipyard/sims/verilator && source ../../env.sh
time make CONFIG=LeanSaturnGemminiRocketConfig run-binary \
BINARY=../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/template-baremetal

cd /ssd_scratch/chipyard/sims/verilator && source ../../env.sh
time make CONFIG=LeanSaturnGemminiRocketConfig run-binary \
BINARY=../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/tiled_matmul_os-baremetal