## Final Instructions

### Activate the Environment

```sh
export CHIPYARD_DIR=$(pwd)
export PATH=$CHIPYARD_DIR/circt-install/bin:$PATH
export CIRCT_INSTALL_PATH=$CHIPYARD_DIR/circt-install

# Verify installation
firtool --version

cd $CHIPYARD_DIR/chipyard
source env.sh
```

The main configuration files are:
- `/chipyard/generators/gemmini/chipyard/GemminiConfigs.scala`
- `/chipyard/generators/gemmini/src/main/scala/gemmini/Configs.scala`

Whenever making any changes in the hardware of Gemmini (i.e., any change in Scala code inside `src`), you must rebuild the hardware.

### Building the hardware

```sh
cd $CHIPYARD_DIR/chipyard
export VERILATOR_THREADS=1
make -C sims/verilator clean && make -C sims/verilator CONFIG=GemminiRocketConfig -j$(nproc)
```

> **Role of `VERILATOR_THREADS`**
> If `VERILATOR_THREADS=4`, the simulation takes fewer clock cycles (about 4 times less) because it processes multiple threads concurrently.

This is sufficient to run Verilator on the hardware to get cycle-accurate results.

### Rebuild tests

> **Note**: Run this command only after building the hardware.

```sh
cd $CHIPYARD_DIR/chipyard/generators/gemmini/software/gemmini-rocc-tests
./build.sh
```

### Testing on Verilator

```sh
cd $CHIPYARD_DIR/chipyard/sims/verilator && source ../../env.sh
time make CONFIG=GemminiRocketConfig run-binary \
BINARY=../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/template-baremetal
```

---

## Running on Spike

For running on Spike while changing the architecture's configuration, run the following additional commands.

### Rebuilding Libgemmini

To run the matrix multiplication function (`template`) on Spike, modify the `gemmini_params.h` file and then run the command below to rebuild `libgemmini`.

```sh
cd $CHIPYARD_DIR/chipyard/generators/gemmini/software/libgemmini
source $CHIPYARD_DIR/env.sh
make clean && make install
```

### Testing on Spike

```sh
cd $CHIPYARD_DIR/chipyard/sims/verilator && source ../../env.sh
spike --extension=gemmini ../../generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/template-baremetal
```
