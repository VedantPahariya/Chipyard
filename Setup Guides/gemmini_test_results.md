# Gemmini Test Results

## Environment Setup ✓
- Chipyard installed at: `/ssd_scratch/chipyard`
- CIRCT/firtool installed at: `/ssd_scratch/circt-install`
- Verilator simulator built with GemminiRocketConfig

## Successful Tests ✓

### 1. Template Test (PASSED)
```bash
make -C sims/verilator CONFIG=GemminiRocketConfig run-binary \
  BINARY=/ssd_scratch/chipyard/generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/template-baremetal
```

**Output:**
```
[UART] UART0 is here (stdin/stdout).
Flush Gemmini TLB of stale virtual addresses
Initialize our input and output matrices in main memory
Calculate the scratchpad addresses of all our matrices
Move "In" matrix from main memory into Gemmini's scratchpad
Move "Identity" matrix from main memory into Gemmini's scratchpad
Multiply "In" matrix with "Identity" matrix with a bias of 0
Move "Out" matrix from Gemmini's scratchpad into main memory
Fence till Gemmini completes all memory operations
Check whether "In" and "Out" matrices are identical
Input and output matrices are identical, as expected
```

### 2. Mvin_Mvout Test (PASSED)
```bash
make -C sims/verilator CONFIG=GemminiRocketConfig run-binary \
  BINARY=/ssd_scratch/chipyard/generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/mvin_mvout-baremetal
```

## Test Categories Available

All tests are located in:
`/ssd_scratch/chipyard/generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/`

### Quick Tests (Complete in < 2 minutes)
- template-baremetal ✓
- mvin_mvout-baremetal ✓
- mvin_mvout_zeros-baremetal
- matrix_add-baremetal
- transpose-baremetal
- aligned-baremetal
- gemmini_counter-baremetal
- global_average-baremetal
- resadd-baremetal

### Matrix Multiplication Tests
- matmul-baremetal (takes > 5 minutes)
- matmul_os-baremetal
- matmul_ws-baremetal
- matmul_spad-baremetal
- tiled_matmul_cpu-baremetal
- tiled_matmul_os-baremetal
- tiled_matmul_ws-baremetal
- tiled_matmul_ws_At-baremetal
- tiled_matmul_ws_Bt-baremetal

### Convolution Tests
- conv-baremetal
- conv_dw-baremetal
- conv_first_layer-baremetal
- conv_perf-baremetal
- conv_rect-baremetal
- conv_stride-baremetal
- conv_with_pool-baremetal

## Running Tests

### Basic Command
```bash
cd /ssd_scratch/chipyard
make -C sims/verilator CONFIG=GemminiRocketConfig run-binary \
  BINARY=/ssd_scratch/chipyard/generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/<test-name>
```

### With Timeout (recommended for complex tests)
```bash
cd /ssd_scratch/chipyard
timeout 300 make -C sims/verilator CONFIG=GemminiRocketConfig run-binary \
  BINARY=/ssd_scratch/chipyard/generators/gemmini/software/gemmini-rocc-tests/build/bareMetalC/<test-name>
```

### Check Results
```bash
# View log output
cat sims/verilator/output/chipyard.harness.TestHarness.GemminiRocketConfig/<test-name>.log

# View detailed trace
cat sims/verilator/output/chipyard.harness.TestHarness.GemminiRocketConfig/<test-name>.out
```

## Notes

1. **Test Timing**: Verilator simulations are slow. Simple tests take 30-60 seconds, complex matrix operations can take 10+ minutes.

2. **Successful Completion**: Look for `Verilog $finish` in the log output - this indicates the test completed successfully.

3. **Use Baremetal Tests**: The `-baremetal` versions of tests run faster than `-pk` (proxy kernel) versions.

4. **Increase Cycle Limit**: For longer tests, modify `+max-cycles=10000000` in the makefile to allow more simulation cycles.

## Next Steps

To run more comprehensive tests:
1. Use the quick tests to verify basic functionality
2. For production validation, run tests on FPGA or use FireSim for faster simulation
3. Check the Gemmini documentation for test-specific expected behavior

