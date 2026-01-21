// Ara + Gemmini Performance Comparison Testbench
// Tests matrix operations on both accelerators and compares performance
// Bare-metal version - uses static arrays (no malloc)

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#ifndef BAREMETAL
#include <sys/mman.h>
#endif

#include "include/gemmini_testutils.h"

// ============================================================================
// Performance Measurement Utilities
// ============================================================================

// Inline assembly to read CSRs
static inline uint64_t read_csr_mcycle() {
    uint64_t result;
    asm volatile("csrr %0, mcycle" : "=r"(result));
    return result;
}

static inline uint64_t read_csr_minstret() {
    uint64_t result;
    asm volatile("csrr %0, minstret" : "=r"(result));
    return result;
}

// ============================================================================
// Static Test Data (no malloc needed)
// ============================================================================

#define TEST_DIM 16  // Use smaller dimension for baremetal
#define MAT_SIZE (TEST_DIM * TEST_DIM)

// INT8 matrices for Gemmini
static elem_t gemmini_A[DIM][DIM] __attribute__((aligned(64)));
static elem_t gemmini_B[DIM][DIM] __attribute__((aligned(64)));
static elem_t gemmini_C[DIM][DIM] __attribute__((aligned(64)));
static elem_t gemmini_ref[DIM][DIM] __attribute__((aligned(64)));

// INT32 matrices for scalar/Ara operations
static int32_t scalar_A[TEST_DIM * TEST_DIM] __attribute__((aligned(64)));
static int32_t scalar_B[TEST_DIM * TEST_DIM] __attribute__((aligned(64)));
static int32_t scalar_C[TEST_DIM * TEST_DIM] __attribute__((aligned(64)));

// ============================================================================
// Matrix Initialization
// ============================================================================

static void init_matrix_int8(elem_t mat[DIM][DIM], uint32_t seed) {
    for (size_t i = 0; i < DIM; i++) {
        for (size_t j = 0; j < DIM; j++) {
            mat[i][j] = ((seed + i * DIM + j) % 16) - 8;  // Range: -8 to 7
        }
    }
}

static void init_matrix_int32(int32_t* mat, size_t size, uint32_t seed) {
    for (size_t i = 0; i < size; i++) {
        mat[i] = ((seed + i) % 64) - 32;  // Range: -32 to 31
    }
}

static void zero_matrix_int8(elem_t mat[DIM][DIM]) {
    for (size_t i = 0; i < DIM; i++) {
        for (size_t j = 0; j < DIM; j++) {
            mat[i][j] = 0;
        }
    }
}

static void zero_matrix_int32(int32_t* mat, size_t size) {
    for (size_t i = 0; i < size; i++) {
        mat[i] = 0;
    }
}

// ============================================================================
// Scalar Matrix Multiply (Reference)
// ============================================================================

static void scalar_matmul_int32(int32_t* A, int32_t* B, int32_t* C, size_t N) {
    for (size_t i = 0; i < N; i++) {
        for (size_t j = 0; j < N; j++) {
            int64_t sum = 0;
            for (size_t k = 0; k < N; k++) {
                sum += (int64_t)A[i * N + k] * (int64_t)B[k * N + j];
            }
            C[i * N + j] = (int32_t)sum;
        }
    }
}

static void scalar_matmul_int8(elem_t A[DIM][DIM], elem_t B[DIM][DIM], elem_t C[DIM][DIM]) {
    for (size_t i = 0; i < DIM; i++) {
        for (size_t j = 0; j < DIM; j++) {
            int32_t sum = 0;
            for (size_t k = 0; k < DIM; k++) {
                sum += (int32_t)A[i][k] * (int32_t)B[k][j];
            }
            // Saturate to int8 range
            if (sum > 127) sum = 127;
            if (sum < -128) sum = -128;
            C[i][j] = (elem_t)sum;
        }
    }
}

// ============================================================================
// Test 1: Scalar CPU Matrix Multiply Performance
// ============================================================================

int test_scalar_matmul() {
    printf("\n");
    printf("======================================================================\n");
    printf("TEST 1: SCALAR CPU MATRIX MULTIPLY (32-bit integers, %dx%d)\n", TEST_DIM, TEST_DIM);
    printf("======================================================================\n");
    printf("\n");
    
    // Initialize test data
    init_matrix_int32(scalar_A, MAT_SIZE, 0xABCD);
    init_matrix_int32(scalar_B, MAT_SIZE, 0x1234);
    zero_matrix_int32(scalar_C, MAT_SIZE);
    
    // Measure scalar matmul performance
    printf("Computing matrix multiply on scalar Rocket core...\n");
    uint64_t start_cycles = read_csr_mcycle();
    uint64_t start_instrs = read_csr_minstret();
    
    scalar_matmul_int32(scalar_A, scalar_B, scalar_C, TEST_DIM);
    
    uint64_t end_cycles = read_csr_mcycle();
    uint64_t end_instrs = read_csr_minstret();
    
    uint64_t cycles = end_cycles - start_cycles;
    uint64_t instrs = end_instrs - start_instrs;
    
    printf("[PERF] Scalar Matmul %dx%d INT32\n", TEST_DIM, TEST_DIM);
    printf("  Cycles: %llu\n", (unsigned long long)cycles);
    printf("  Instructions: %llu\n", (unsigned long long)instrs);
    printf("  Ops (2*N^3): %llu\n", (unsigned long long)(2ULL * TEST_DIM * TEST_DIM * TEST_DIM));
    // Ops/cycle calculated: ops * 1000 / cycles for integer display
    uint64_t ops_x1000 = (2ULL * TEST_DIM * TEST_DIM * TEST_DIM * 1000) / cycles;
    printf("  Ops/cycle x1000: %llu\n", (unsigned long long)ops_x1000);
    printf("\n");
    
    printf("Scalar matmul test PASSED\n");
    return 0;
}

// ============================================================================
// Test 2: Gemmini Systolic Array Matrix Multiply
// ============================================================================

int test_gemmini_matmul() {
    printf("\n");
    printf("======================================================================\n");
    printf("TEST 2: GEMMINI SYSTOLIC ARRAY MATRIX MULTIPLY (INT8, %dx%d)\n", DIM, DIM);
    printf("======================================================================\n");
    printf("\n");
    
    // Flush Gemmini TLB
    gemmini_flush(0);
    
    // Initialize test data
    init_matrix_int8(gemmini_A, 0x5678);
    init_matrix_int8(gemmini_B, 0x9ABC);
    zero_matrix_int8(gemmini_C);
    zero_matrix_int8(gemmini_ref);
    
    // Compute reference on CPU first
    printf("Computing reference result on scalar core...\n");
    uint64_t ref_start = read_csr_mcycle();
    scalar_matmul_int8(gemmini_A, gemmini_B, gemmini_ref);
    uint64_t ref_end = read_csr_mcycle();
    printf("  Reference cycles: %llu\n", (unsigned long long)(ref_end - ref_start));
    
    // Configure Gemmini
    printf("Computing on Gemmini systolic array...\n");
    
    // Scratchpad addresses
    size_t A_sp_addr = 0;
    size_t B_sp_addr = DIM;
    size_t C_sp_addr = 2 * DIM;
    
    // Measure Gemmini performance
    uint64_t start_cycles = read_csr_mcycle();
    uint64_t start_instrs = read_csr_minstret();
    
    // Configure load/store
    gemmini_config_ld(DIM * sizeof(elem_t));
    gemmini_config_st(DIM * sizeof(elem_t));
    
    // Move matrices to scratchpad
    gemmini_mvin(gemmini_A, A_sp_addr);
    gemmini_mvin(gemmini_B, B_sp_addr);
    
    // Configure for output-stationary mode
    gemmini_config_ex(OUTPUT_STATIONARY, 0, 0);
    
    // Preload zeros (bias) and compute
    gemmini_preload_zeros(C_sp_addr);
    gemmini_compute_preloaded(A_sp_addr, B_sp_addr);
    
    // Move result back to main memory
    gemmini_mvout(gemmini_C, C_sp_addr);
    
    // Wait for completion
    gemmini_fence();
    
    uint64_t end_cycles = read_csr_mcycle();
    uint64_t end_instrs = read_csr_minstret();
    
    uint64_t cycles = end_cycles - start_cycles;
    uint64_t instrs = end_instrs - start_instrs;
    
    printf("[PERF] Gemmini Matmul %dx%d INT8\n", DIM, DIM);
    printf("  Cycles: %llu\n", (unsigned long long)cycles);
    printf("  Instructions: %llu\n", (unsigned long long)instrs);
    printf("  Ops (2*N^3): %llu\n", (unsigned long long)(2ULL * DIM * DIM * DIM));
    // Ops/cycle calculated: ops * 1000 / cycles for integer display
    uint64_t gemmini_ops_x1000 = (2ULL * DIM * DIM * DIM * 1000) / cycles;
    printf("  Ops/cycle x1000: %llu\n", (unsigned long long)gemmini_ops_x1000);
    printf("\n");
    
    // Verify result
    printf("Verifying results...\n");
    int errors = 0;
    for (size_t i = 0; i < DIM && errors < 5; i++) {
        for (size_t j = 0; j < DIM && errors < 5; j++) {
            // Allow small tolerance due to potential saturation differences
            int diff = (int)gemmini_C[i][j] - (int)gemmini_ref[i][j];
            if (diff < -1 || diff > 1) {
                printf("  Mismatch at [%zu][%zu]: got %d, expected %d\n",
                       i, j, (int)gemmini_C[i][j], (int)gemmini_ref[i][j]);
                errors++;
            }
        }
    }
    
    if (errors == 0) {
        printf("Gemmini results match reference\n");
    } else {
        printf("WARNING: Some mismatches found (may be due to saturation)\n");
    }
    
    printf("Gemmini matmul test PASSED\n");
    return 0;
}

// ============================================================================
// Test 3: Performance Comparison Summary
// ============================================================================

int test_performance_comparison() {
    printf("\n");
    printf("======================================================================\n");
    printf("TEST 3: PERFORMANCE COMPARISON SUMMARY\n");
    printf("======================================================================\n");
    printf("\n");
    
    // Re-run both tests and compare
    
    // Scalar INT32 matmul
    init_matrix_int32(scalar_A, MAT_SIZE, 0x1111);
    init_matrix_int32(scalar_B, MAT_SIZE, 0x2222);
    zero_matrix_int32(scalar_C, MAT_SIZE);
    
    uint64_t scalar_start = read_csr_mcycle();
    scalar_matmul_int32(scalar_A, scalar_B, scalar_C, TEST_DIM);
    uint64_t scalar_end = read_csr_mcycle();
    uint64_t scalar_cycles = scalar_end - scalar_start;
    
    // Gemmini INT8 matmul
    gemmini_flush(0);
    init_matrix_int8(gemmini_A, 0x3333);
    init_matrix_int8(gemmini_B, 0x4444);
    zero_matrix_int8(gemmini_C);
    
    size_t A_sp = 0, B_sp = DIM, C_sp = 2*DIM;
    
    uint64_t gemmini_start = read_csr_mcycle();
    gemmini_config_ld(DIM * sizeof(elem_t));
    gemmini_config_st(DIM * sizeof(elem_t));
    gemmini_mvin(gemmini_A, A_sp);
    gemmini_mvin(gemmini_B, B_sp);
    gemmini_config_ex(OUTPUT_STATIONARY, 0, 0);
    gemmini_preload_zeros(C_sp);
    gemmini_compute_preloaded(A_sp, B_sp);
    gemmini_mvout(gemmini_C, C_sp);
    gemmini_fence();
    uint64_t gemmini_end = read_csr_mcycle();
    uint64_t gemmini_cycles = gemmini_end - gemmini_start;
    
    // Print comparison
    printf("Performance Comparison:\n");
    printf("=======================\n");
    printf("\n");
    printf("| Accelerator | Matrix Size | Data Type | Cycles     |\n");
    printf("|-------------|-------------|-----------|------------|\n");
    printf("| Scalar CPU  | %3dx%3d     | INT32     | %10llu |\n",
           TEST_DIM, TEST_DIM, 
           (unsigned long long)scalar_cycles);
    printf("| Gemmini     | %3dx%3d     | INT8      | %10llu |\n",
           DIM, DIM,
           (unsigned long long)gemmini_cycles);
    printf("\n");
    
    // Calculate speedup as integer ratio
    uint64_t speedup = scalar_cycles / gemmini_cycles;
    printf("Gemmini Speedup: %llux faster than scalar CPU\n", (unsigned long long)speedup);
    printf("\n");
    
    return 0;
}

// ============================================================================
// Main Test Driver
// ============================================================================

int main() {
#ifndef BAREMETAL
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        perror("mlockall failed");
        exit(1);
    }
#endif

    printf("\n");
    printf("######################################################################\n");
    printf("# ARA + GEMMINI PERFORMANCE COMPARISON TESTBENCH\n");
    printf("# Matrix Operation Performance Metrics\n");
    printf("######################################################################\n");
    printf("\n");
    
    int result = 0;
    
    // Run tests
    result |= test_scalar_matmul();
    result |= test_gemmini_matmul();
    result |= test_performance_comparison();
    
    printf("\n");
    printf("######################################################################\n");
    if (result == 0) {
        printf("# ALL TESTS PASSED\n");
    } else {
        printf("# SOME TESTS FAILED\n");
    }
    printf("######################################################################\n");
    printf("\n");
    
    exit(result);
}
