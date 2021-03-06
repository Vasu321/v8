// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/compilation-environment.h"
#include "src/wasm/wasm-tier.h"
#include "test/cctest/cctest.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_simd {

#define WASM_SIMD_TEST(name)                                         \
  void RunWasm_##name##_Impl(LowerSimd lower_simd,                   \
                             TestExecutionTier execution_tier);      \
  TEST(RunWasm_##name##_simd_lowered) {                              \
    EXPERIMENTAL_FLAG_SCOPE(simd);                                   \
    RunWasm_##name##_Impl(kLowerSimd, TestExecutionTier::kTurbofan); \
  }                                                                  \
  void RunWasm_##name##_Impl(LowerSimd lower_simd,                   \
                             TestExecutionTier execution_tier)

WASM_SIMD_TEST(I8x16ToF32x4) {
  WasmRunner<int32_t, int32_t> r(execution_tier, lower_simd);
  float* g = r.builder().AddGlobal<float>(kWasmS128);
  byte param1 = 0;
  BUILD(r,
        WASM_SET_GLOBAL(
            0, WASM_SIMD_UNOP(kExprF32x4Sqrt,
                              WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(param1)))),
        WASM_ONE);

  // Arbitrary pattern that doesn't end up creating a NaN.
  r.Call(0x5b);
  float f = bit_cast<float>(0x5b5b5b5b);
  float actual = ReadLittleEndianValue<float>(&g[0]);
  float expected = std::sqrt(f);
  CHECK_EQ(expected, actual);
}

WASM_SIMD_TEST(F32x4_Call_Return) {
  // Check that functions that return F32x4 are correctly lowered into 4 int32
  // nodes. The signature of such functions are always lowered to 4 Word32, and
  // if the last operation before the return was a f32x4, it will need to be
  // bitcasted from float to int.
  TestSignatures sigs;
  WasmRunner<float, float> r(execution_tier, lower_simd);

  // A simple function that just calls f32x4.neg on the param.
  WasmFunctionCompiler& fn = r.NewFunction(sigs.s_s());
  BUILD(fn, WASM_SIMD_UNOP(kExprF32x4Neg, WASM_GET_LOCAL(0)));

  // TODO(v8:10507)
  // Use i32x4 splat since scalar lowering has a problem with f32x4 as a param
  // to a function call, the lowering is not correct yet.
  BUILD(r,
        WASM_SIMD_F32x4_EXTRACT_LANE(
            0, WASM_CALL_FUNCTION(fn.function_index(),
                                  WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(0)))));
  CHECK_EQ(-1.0, r.Call(1));
}

WASM_SIMD_TEST(I8x16_Call_Return) {
  // Check that calling a function with i8x16 arguments, and returns i8x16, is
  // correctly lowered. The signature of the functions are always lowered to 4
  // Word32, so each i8x16 needs to be correctly converted.
  TestSignatures sigs;
  WasmRunner<uint32_t, uint32_t> r(execution_tier, lower_simd);

  WasmFunctionCompiler& fn = r.NewFunction(sigs.s_ss());
  BUILD(fn,
        WASM_SIMD_BINOP(kExprI8x16Add, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  BUILD(r,
        WASM_SIMD_I8x16_EXTRACT_LANE(
            0, WASM_CALL_FUNCTION(fn.function_index(),
                                  WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(0)),
                                  WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(0)))));
  CHECK_EQ(2, r.Call(1));
}

WASM_SIMD_TEST(I16x8_Call_Return) {
  // Check that calling a function with i16x8 arguments, and returns i16x8, is
  // correctly lowered. The signature of the functions are always lowered to 4
  // Word32, so each i16x8 needs to be correctly converted.
  TestSignatures sigs;
  WasmRunner<uint32_t, uint32_t> r(execution_tier, lower_simd);

  WasmFunctionCompiler& fn = r.NewFunction(sigs.s_ss());
  BUILD(fn,
        WASM_SIMD_BINOP(kExprI16x8Add, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  BUILD(r,
        WASM_SIMD_I16x8_EXTRACT_LANE(
            0, WASM_CALL_FUNCTION(fn.function_index(),
                                  WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(0)),
                                  WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(0)))));
  CHECK_EQ(2, r.Call(1));
}

WASM_SIMD_TEST(I8x16Eq_ToTest_S128Const) {
  // Test implementation of S128Const in scalar lowering, this test case was
  // causing a crash.
  TestSignatures sigs;
  WasmRunner<uint32_t> r(execution_tier, lower_simd);

  byte c1[16] = {0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x40};
  byte c2[16] = {0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
                 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02};
  byte c3[16] = {0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  BUILD(r,
        WASM_SIMD_BINOP(kExprI8x16Eq, WASM_SIMD_CONSTANT(c1),
                        WASM_SIMD_CONSTANT(c2)),
        WASM_SIMD_CONSTANT(c3), WASM_SIMD_OP(kExprI8x16Eq),
        WASM_SIMD_OP(kExprI8x16ExtractLaneS), TO_BYTE(4));
  CHECK_EQ(0xffffffff, r.Call());
}

WASM_SIMD_TEST(F32x4_S128Const) {
  // Test that S128Const lowering is done correctly when it is used as an input
  // into a f32x4 operation. This was triggering a CHECK failure in the
  // register-allocator-verifier.
  TestSignatures sigs;
  WasmRunner<float> r(execution_tier, lower_simd);

  // f32x4(1.0, 2.0, 3.0, 4.0)
  byte c1[16] = {0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x40,
                 0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x80, 0x40};
  // f32x4(5.0, 6.0, 7.0, 8.0)
  byte c2[16] = {0x00, 0x00, 0xa0, 0x40, 0x00, 0x00, 0xc0, 0x40,
                 0x00, 0x00, 0xe0, 0x40, 0x00, 0x00, 0x00, 0x41};

  BUILD(r,
        WASM_SIMD_BINOP(kExprF32x4Min, WASM_SIMD_CONSTANT(c1),
                        WASM_SIMD_CONSTANT(c2)),
        WASM_SIMD_OP(kExprF32x4ExtractLane), TO_BYTE(0));
  CHECK_EQ(1.0, r.Call());
}

WASM_SIMD_TEST(AllTrue_DifferentShapes) {
  // Test all_true lowring with splats of different shapes.
  {
    WasmRunner<int32_t, int32_t> r(execution_tier, lower_simd);

    BUILD(r, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(0)),
          WASM_SIMD_OP(kExprV8x16AllTrue));

    CHECK_EQ(0, r.Call(0x00FF00FF));
  }

  {
    WasmRunner<int32_t, int32_t> r(execution_tier, lower_simd);

    BUILD(r, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(0)),
          WASM_SIMD_OP(kExprV16x8AllTrue));

    CHECK_EQ(0, r.Call(0x000000FF));
  }
}

WASM_SIMD_TEST(AnyTrue_DifferentShapes) {
  // Test any_true lowring with splats of different shapes.
  {
    WasmRunner<int32_t, int32_t> r(execution_tier, lower_simd);

    BUILD(r, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(0)),
          WASM_SIMD_OP(kExprV8x16AnyTrue));

    CHECK_EQ(0, r.Call(0x00000000));
  }

  {
    WasmRunner<int32_t, int32_t> r(execution_tier, lower_simd);

    BUILD(r, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(0)),
          WASM_SIMD_OP(kExprV16x8AnyTrue));

    CHECK_EQ(1, r.Call(0x000000FF));
  }
}

}  // namespace test_run_wasm_simd
}  // namespace wasm
}  // namespace internal
}  // namespace v8
