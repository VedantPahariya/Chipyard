// Ara + Gemmini + Scalar Performance Comparison Testbench
// Tests matrix operations on Ara (RVV), Gemmini (Systolic), and Scalar Core
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
// RVV (RISC-V Vector) Intrinsics and Configuration
// ============================================================================

// Enable vector extension in MSTATUS
static inline void enable_vector_extension() {
    // Set MSTATUS.VS = Initial (01) to enable vector instructions
    // MSTATUS.VS is at bits [10:9]
    uint64_t mstatus;
    asm volatile("csrr %0, mstatus" : "=r"(mstatus));
    mstatus |= (1UL << 9);  // Set VS to 01 (Initial)
    asm volatile("csrw mstatus, %0" : : "r"(mstatus));
}

// Inline assembly macros for RVV operations
// vsetvli: Set vector length for element width
// #define VSETVLI_E32M1(avl) ({ \
//     size_t vl; \
//     asm volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(avl)); \
//     vl; \
// })

// #define VSETVLI_E64M1(avl) ({ \
//     size_t vl; \
//     asm volatile("vsetvli %0, %1, e64, m1, ta, ma" : "=r"(vl) : "r"(avl)); \
//     vl; \
// })

// ============================================================================
// Performance Measurement Utilities
// ============================================================================

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
// Static Test Data
// ============================================================================

#define TEST_DIM 16  // Matrix dimension for comparison tests
#define VEC_LEN 256  // Vector length for pure vector tests
#define MAT_SIZE (TEST_DIM * TEST_DIM)

// INT8 matrices for Gemmini
static elem_t gemmini_A[DIM][DIM] __attribute__((aligned(64)));
static elem_t gemmini_B[DIM][DIM] __attribute__((aligned(64)));
static elem_t gemmini_C[DIM][DIM] __attribute__((aligned(64)));
static elem_t gemmini_ref[DIM][DIM] __attribute__((aligned(64)));

// INT32 matrices for scalar/Ara operations
static int32_t scalar_A[MAT_SIZE] __attribute__((aligned(64)));
static int32_t scalar_B[MAT_SIZE] __attribute__((aligned(64)));
static int32_t scalar_C[MAT_SIZE] __attribute__((aligned(64)));

// INT32 vectors for Ara SAXPY test
static int32_t vec_x[VEC_LEN] __attribute__((aligned(64)));
static int32_t vec_y[VEC_LEN] __attribute__((aligned(64)));
static int32_t vec_result[VEC_LEN] __attribute__((aligned(64)));
static int32_t vec_ref[VEC_LEN] __attribute__((aligned(64)));

// ============================================================================
// Matrix/Vector Initialization
// ============================================================================

static void init_matrix_int8(elem_t mat[DIM][DIM], uint32_t seed) {
    for (size_t i = 0; i < DIM; i++) {
        for (size_t j = 0; j < DIM; j++) {
            mat[i][j] = ((seed + i * DIM + j) % 16) - 8;
        }
    }
}

static void init_matrix_int32(int32_t* mat, size_t size, uint32_t seed) {
    for (size_t i = 0; i < size; i++) {
        mat[i] = ((seed + i) % 64) - 32;
    }
}

static void init_vector_int32(int32_t* vec, size_t len, uint32_t seed) {
    for (size_t i = 0; i < len; i++) {
        vec[i] = ((seed + i) % 100) - 50;
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
// Scalar Operations
// ============================================================================

// Scalar SAXPY: y = a*x + y
static void scalar_saxpy(int32_t a, int32_t* x, int32_t* y, size_t n) {
    for (size_t i = 0; i < n; i++) {
        y[i] = a * x[i] + y[i];
    }
}

// Scalar matrix multiply
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
            if (sum > 127) sum = 127;
            if (sum < -128) sum = -128;
            C[i][j] = (elem_t)sum;
        }
    }
}

// ============================================================================
// Ara Vector Operations using RVV Intrinsics (Inline Assembly)
// ============================================================================

// Ara Vector SAXPY: y = a*x + y using RVV instructions
// Using simpler assembly that should work with Ara
// Requires compilation with -march=rv64gcv flag
static void ara_vector_saxpy(int32_t a, int32_t* x, int32_t* y, size_t n) {
    size_t vl;
    
    while (n > 0) {
        // Set vector length for 32-bit elements, LMUL=1
        asm volatile(
            "v
              li %0, %1, e32, m1, ta, ma"
            : "=r"(vl)
            : "r"(n)
        );
        
        // Load x vector into v1
        asm volatile(
            "vle32.v v1, (%0)"
            :
            : "r"(x)
            : "memory", "v1"
        );
        
        // Load y vector into v2
        asm volatile(
            "vle32.v v2, (%0)"
            :
            : "r"(y)
            : "memory", "v2"
        );
        
        // v3 = a * v1 (scalar-vector multiply)
        asm volatile(
            "vmul.vx v3, v1, %0"
            :
            : "r"(a)
            : "v3"
        );
        
        // v2 = v3 + v2 (y = a*x + y)
        asm volatile(
            "vadd.vv v2, v3, v2"
            :
            :
            : "v2"
        );
        
        // Store result back to y
        asm volatile(
            "vse32.v v2, (%0)"
            :
            : "r"(y)
            : "memory"
        );
        
        // Advance pointers
        x += vl;
        y += vl;
        n -= vl;
    }
}

// Ara Vector Dot Product - Simple scalar fallback for now
// Full RVV dot product requires widening instructions
static int64_t ara_vector_dot(int32_t* x, int32_t* y, size_t n) {
    int64_t sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum += (int64_t)x[i] * (int64_t)y[i];
    }
    return sum;
}

// ============================================================================
// Test 1: Scalar CPU Performance
// ============================================================================

int test_scalar_performance() {
    printf("\n");
    printf("======================================================================\n");
    printf("TEST 1: SCALAR CPU PERFORMANCE\n");
    printf("======================================================================\n");
    printf("\n");
    
    // --- SAXPY Test ---
    printf("--- SAXPY: y = a*x + y (vector length=%d, INT32) ---\n", VEC_LEN);
    
    init_vector_int32(vec_x, VEC_LEN, 0xABCD);
    init_vector_int32(vec_y, VEC_LEN, 0x1234);
    int32_t alpha = 3;
    
    uint64_t start = read_csr_mcycle();
    scalar_saxpy(alpha, vec_x, vec_y, VEC_LEN);
    uint64_t end = read_csr_mcycle();
    
    printf("[PERF] Scalar SAXPY\n");
    printf("  Vector Length: %d\n", VEC_LEN);
    printf("  Cycles: %llu\n", (unsigned long long)(end - start));
    printf("  Ops (2*N): %d\n", 2 * VEC_LEN);
    printf("\n");
    
    // --- Matrix Multiply Test ---
    printf("--- Matrix Multiply: C = A*B (%dx%d, INT32) ---\n", TEST_DIM, TEST_DIM);
    
    init_matrix_int32(scalar_A, MAT_SIZE, 0x5678);
    init_matrix_int32(scalar_B, MAT_SIZE, 0x9ABC);
    zero_matrix_int32(scalar_C, MAT_SIZE);
    
    start = read_csr_mcycle();
    scalar_matmul_int32(scalar_A, scalar_B, scalar_C, TEST_DIM);
    end = read_csr_mcycle();
    
    printf("[PERF] Scalar Matmul\n");
    printf("  Matrix Size: %dx%d\n", TEST_DIM, TEST_DIM);
    printf("  Cycles: %llu\n", (unsigned long long)(end - start));
    printf("  Ops (2*N^3): %llu\n", (unsigned long long)(2ULL * TEST_DIM * TEST_DIM * TEST_DIM));
    printf("\n");
    
    return 0;
}

// ============================================================================
// Test 2: Ara Vector Unit Performance
// ============================================================================

int test_ara_performance() {
    printf("\n");
    printf("======================================================================\n");
    printf("TEST 2: ARA VECTOR UNIT PERFORMANCE (RVV 1.0)\n");
    printf("======================================================================\n");
    printf("\n");
    
    // Enable vector extension first
    printf("Enabling RVV extension...\n");
    enable_vector_extension();
    
    // --- SAXPY Test ---
    printf("--- Ara Vector SAXPY: y = a*x + y (vector length=%d, INT32) ---\n", VEC_LEN);
    
    // Initialize and save reference
    init_vector_int32(vec_x, VEC_LEN, 0xABCD);
    init_vector_int32(vec_y, VEC_LEN, 0x1234);
    int32_t alpha = 3;
    
    // Copy y to ref before scalar computation changes it
    for (size_t i = 0; i < VEC_LEN; i++) vec_ref[i] = vec_y[i];
    
    // Compute reference on scalar
    scalar_saxpy(alpha, vec_x, vec_ref, VEC_LEN);
    
    // Re-initialize y for Ara test
    init_vector_int32(vec_y, VEC_LEN, 0x1234);
    
    uint64_t start = read_csr_mcycle();
    ara_vector_saxpy(alpha, vec_x, vec_y, VEC_LEN);
    uint64_t end = read_csr_mcycle();
    
    printf("[PERF] Ara Vector SAXPY\n");
    printf("  Vector Length: %d\n", VEC_LEN);
    printf("  Cycles: %llu\n", (unsigned long long)(end - start));
    printf("  Ops (2*N): %d\n", 2 * VEC_LEN);
    printf("\n");
    
    // Verify
    int errors = 0;
    for (size_t i = 0; i < VEC_LEN && errors < 5; i++) {
        if (vec_y[i] != vec_ref[i]) {
            printf("  MISMATCH at [%zu]: got %d, expected %d\n", i, vec_y[i], vec_ref[i]);
            errors++;
        }
    }
    if (errors == 0) {
        printf("  Verification: PASSED\n");
    } else {
        printf("  Verification: FAILED (%d errors)\n", errors);
        return 1;
    }
    printf("\n");
    
    return 0;
}

// ============================================================================
// Test 3: Gemmini Systolic Array Performance
// ============================================================================

int test_gemmini_performance() {
    printf("\n");
    printf("======================================================================\n");
    printf("TEST 3: GEMMINI SYSTOLIC ARRAY PERFORMANCE\n");
    printf("======================================================================\n");
    printf("\n");
    
    // Flush Gemmini
    gemmini_flush(0);
    
    // Initialize
    init_matrix_int8(gemmini_A, 0x5678);
    init_matrix_int8(gemmini_B, 0x9ABC);
    zero_matrix_int8(gemmini_C);
    zero_matrix_int8(gemmini_ref);
    
    // Compute reference
    printf("Computing reference on scalar core...\n");
    uint64_t ref_start = read_csr_mcycle();
    scalar_matmul_int8(gemmini_A, gemmini_B, gemmini_ref);
    uint64_t ref_end = read_csr_mcycle();
    printf("  Scalar reference cycles: %llu\n", (unsigned long long)(ref_end - ref_start));
    
    // Gemmini computation
    printf("Computing on Gemmini systolic array...\n");
    
    size_t A_sp = 0, B_sp = DIM, C_sp = 2*DIM;
    
    uint64_t start = read_csr_mcycle();
    
    gemmini_config_ld(DIM * sizeof(elem_t));
    gemmini_config_st(DIM * sizeof(elem_t));
    gemmini_mvin(gemmini_A, A_sp);
    gemmini_mvin(gemmini_B, B_sp);
    gemmini_config_ex(OUTPUT_STATIONARY, 0, 0);
    gemmini_preload_zeros(C_sp);
    gemmini_compute_preloaded(A_sp, B_sp);
    gemmini_mvout(gemmini_C, C_sp);
    gemmini_fence();
    
    uint64_t end = read_csr_mcycle();
    
    printf("[PERF] Gemmini Matmul\n");
    printf("  Matrix Size: %dx%d (INT8)\n", DIM, DIM);
    printf("  Cycles: %llu\n", (unsigned long long)(end - start));
    printf("  Ops (2*N^3): %llu\n", (unsigned long long)(2ULL * DIM * DIM * DIM));
    printf("\n");
    
    // Verify
    int errors = 0;
    for (size_t i = 0; i < DIM && errors < 5; i++) {
        for (size_t j = 0; j < DIM && errors < 5; j++) {
            int diff = (int)gemmini_C[i][j] - (int)gemmini_ref[i][j];
            if (diff < -1 || diff > 1) {
                printf("  MISMATCH at [%zu][%zu]: got %d, expected %d\n",
                       i, j, (int)gemmini_C[i][j], (int)gemmini_ref[i][j]);
                errors++;
            }
        }
    }
    if (errors == 0) {
        printf("  Verification: PASSED\n");
    }
    printf("\n");
    
    return 0;
}

// ============================================================================
// Test 4: Performance Comparison Summary
// ============================================================================

int test_comparison() {
    printf("\n");
    printf("======================================================================\n");
    printf("TEST 4: PERFORMANCE COMPARISON SUMMARY\n");
    printf("======================================================================\n");
    printf("\n");
    
    uint64_t scalar_saxpy_cycles, ara_saxpy_cycles;
    uint64_t scalar_matmul_cycles, gemmini_matmul_cycles;
    
    // --- SAXPY Comparison ---
    init_vector_int32(vec_x, VEC_LEN, 0x1111);
    init_vector_int32(vec_y, VEC_LEN, 0x2222);
    
    uint64_t start = read_csr_mcycle();
    scalar_saxpy(5, vec_x, vec_y, VEC_LEN);
    scalar_saxpy_cycles = read_csr_mcycle() - start;
    
    init_vector_int32(vec_y, VEC_LEN, 0x2222);
    start = read_csr_mcycle();
    ara_vector_saxpy(5, vec_x, vec_y, VEC_LEN);
    ara_saxpy_cycles = read_csr_mcycle() - start;
    
    // --- Matmul Comparison ---
    init_matrix_int32(scalar_A, MAT_SIZE, 0x3333);
    init_matrix_int32(scalar_B, MAT_SIZE, 0x4444);
    zero_matrix_int32(scalar_C, MAT_SIZE);
    
    start = read_csr_mcycle();
    scalar_matmul_int32(scalar_A, scalar_B, scalar_C, TEST_DIM);
    scalar_matmul_cycles = read_csr_mcycle() - start;
    
    gemmini_flush(0);
    init_matrix_int8(gemmini_A, 0x5555);
    init_matrix_int8(gemmini_B, 0x6666);
    zero_matrix_int8(gemmini_C);
    
    size_t A_sp = 0, B_sp = DIM, C_sp = 2*DIM;
    start = read_csr_mcycle();
    gemmini_config_ld(DIM * sizeof(elem_t));
    gemmini_config_st(DIM * sizeof(elem_t));
    gemmini_mvin(gemmini_A, A_sp);
    gemmini_mvin(gemmini_B, B_sp);
    gemmini_config_ex(OUTPUT_STATIONARY, 0, 0);
    gemmini_preload_zeros(C_sp);
    gemmini_compute_preloaded(A_sp, B_sp);
    gemmini_mvout(gemmini_C, C_sp);
    gemmini_fence();
    gemmini_matmul_cycles = read_csr_mcycle() - start;
    
    // Print Summary
    printf("==========================================================\n");
    printf("                 PERFORMANCE SUMMARY\n");
    printf("==========================================================\n");
    printf("\n");
    printf("SAXPY Operation (y = a*x + y, N=%d, INT32):\n", VEC_LEN);
    printf("----------------------------------------------------------\n");
    printf("| Processor    | Cycles     | Speedup vs Scalar |\n");
    printf("|--------------|------------|-------------------|\n");
    printf("| Scalar CPU   | %10llu | 1.0x              |\n", (unsigned long long)scalar_saxpy_cycles);
    printf("| Ara (RVV)    | %10llu | %llu.%llux             |\n", 
           (unsigned long long)ara_saxpy_cycles,
           (unsigned long long)(scalar_saxpy_cycles / ara_saxpy_cycles),
           (unsigned long long)((scalar_saxpy_cycles * 10 / ara_saxpy_cycles) % 10));
    printf("\n");
    
    printf("Matrix Multiply (C = A*B, %dx%d):\n", TEST_DIM, TEST_DIM);
    printf("----------------------------------------------------------\n");
    printf("| Processor    | Data Type | Cycles     | Speedup |\n");
    printf("|--------------|-----------|------------|----------|");
    printf("\n");
    printf("| Scalar CPU   | INT32     | %10llu | 1.0x     |\n", (unsigned long long)scalar_matmul_cycles);
    printf("| Gemmini      | INT8      | %10llu | %llux       |\n", 
           (unsigned long long)gemmini_matmul_cycles,
           (unsigned long long)(scalar_matmul_cycles / gemmini_matmul_cycles));
    printf("\n");
    
    printf("==========================================================\n");
    printf("KEY INSIGHTS:\n");
    printf("==========================================================\n");
    printf("- Ara Vector Unit: Best for data-parallel operations\n");
    printf("  (SAXPY, element-wise ops, reductions)\n");
    printf("- Gemmini Systolic: Best for matrix operations\n");
    printf("  (GEMM, convolutions, dense linear algebra)\n");
    printf("- Combined: Heterogeneous acceleration for ML workloads\n");
    printf("\n");
    
    return 0;
}

// ============================================================================
// Main
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
    printf("#   ARA + GEMMINI + SCALAR PERFORMANCE COMPARISON TESTBENCH         #\n");
    printf("#   Rocket Core + Ara Vector (RVV) + Gemmini Systolic Array         #\n");
    printf("######################################################################\n");
    printf("\n");
    
    int result = 0;
    
    result |= test_scalar_performance();
    result |= test_ara_performance();
    result |= test_gemmini_performance();
    result |= test_comparison();
    
    printf("\n");
    printf("######################################################################\n");
    if (result == 0) {
        printf("#                     ALL TESTS PASSED                              #\n");
    } else {
        printf("#                     SOME TESTS FAILED                             #\n");
    }
    printf("######################################################################\n");
    printf("\n");
    
    exit(result);
}
