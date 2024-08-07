// Copyright (c) 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>

#include "test/opt/pass_fixture.h"
#include "test/opt/pass_utils.h"

namespace spvtools {
namespace opt {
namespace {

using DescriptorScalarReplacementTest = PassTest<::testing::Test>;

std::string GetStructureArrayTestSpirv() {
  // The SPIR-V for the following high-level shader:
  // Flattening structures and arrays should result in the following binding
  // numbers. Only the ones that are actually used in the shader should be in
  // the final SPIR-V.
  //
  // globalS[0][0].t[0]  0 (used)
  // globalS[0][0].t[1]  1
  // globalS[0][0].s[0]  2 (used)
  // globalS[0][0].s[1]  3
  // globalS[0][1].t[0]  4
  // globalS[0][1].t[1]  5
  // globalS[0][1].s[0]  6
  // globalS[0][1].s[1]  7
  // globalS[1][0].t[0]  8
  // globalS[1][0].t[1]  9
  // globalS[1][0].s[0]  10
  // globalS[1][0].s[1]  11
  // globalS[1][1].t[0]  12
  // globalS[1][1].t[1]  13 (used)
  // globalS[1][1].s[0]  14
  // globalS[1][1].s[1]  15 (used)

  /*
    struct S {
      Texture2D t[2];
      SamplerState s[2];
    };

    S globalS[2][2];

    float4 main() : SV_Target {
      return globalS[0][0].t[0].Sample(globalS[0][0].s[0], float2(0,0)) +
             globalS[1][1].t[1].Sample(globalS[1][1].s[1], float2(0,0));
    }
  */

  return R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %out_var_SV_Target
               OpExecutionMode %main OriginUpperLeft
               OpName %S "S"
               OpMemberName %S 0 "t"
               OpMemberName %S 1 "s"
               OpName %type_2d_image "type.2d.image"
               OpName %type_sampler "type.sampler"
               OpName %globalS "globalS"
               OpName %out_var_SV_Target "out.var.SV_Target"
               OpName %main "main"
               OpName %src_main "src.main"
               OpName %bb_entry "bb.entry"
               OpName %type_sampled_image "type.sampled.image"
               OpDecorate %out_var_SV_Target Location 0
               OpDecorate %globalS DescriptorSet 0
               OpDecorate %globalS Binding 0
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
      %int_1 = OpConstant %int 1
      %float = OpTypeFloat 32
    %float_0 = OpConstant %float 0
    %v2float = OpTypeVector %float 2
         %10 = OpConstantComposite %v2float %float_0 %float_0
       %uint = OpTypeInt 32 0
     %uint_2 = OpConstant %uint 2
%type_2d_image = OpTypeImage %float 2D 2 0 0 1 Unknown
%_arr_type_2d_image_uint_2 = OpTypeArray %type_2d_image %uint_2
%type_sampler = OpTypeSampler
%_arr_type_sampler_uint_2 = OpTypeArray %type_sampler %uint_2
          %S = OpTypeStruct %_arr_type_2d_image_uint_2 %_arr_type_sampler_uint_2
%_arr_S_uint_2 = OpTypeArray %S %uint_2
%_arr__arr_S_uint_2_uint_2 = OpTypeArray %_arr_S_uint_2 %uint_2
%_ptr_UniformConstant__arr__arr_S_uint_2_uint_2 = OpTypePointer UniformConstant %_arr__arr_S_uint_2_uint_2
    %v4float = OpTypeVector %float 4
%_ptr_Output_v4float = OpTypePointer Output %v4float
       %void = OpTypeVoid
         %24 = OpTypeFunction %void
         %28 = OpTypeFunction %v4float
%_ptr_UniformConstant_type_2d_image = OpTypePointer UniformConstant %type_2d_image
%_ptr_UniformConstant_type_sampler = OpTypePointer UniformConstant %type_sampler
%type_sampled_image = OpTypeSampledImage %type_2d_image
    %globalS = OpVariable %_ptr_UniformConstant__arr__arr_S_uint_2_uint_2 UniformConstant
%out_var_SV_Target = OpVariable %_ptr_Output_v4float Output
       %main = OpFunction %void None %24
         %25 = OpLabel
         %26 = OpFunctionCall %v4float %src_main
               OpStore %out_var_SV_Target %26
               OpReturn
               OpFunctionEnd
   %src_main = OpFunction %v4float None %28
   %bb_entry = OpLabel
         %31 = OpAccessChain %_ptr_UniformConstant_type_2d_image %globalS %int_0 %int_0 %int_0 %int_0
         %32 = OpLoad %type_2d_image %31
         %34 = OpAccessChain %_ptr_UniformConstant_type_sampler %globalS %int_0 %int_0 %int_1 %int_0
         %35 = OpLoad %type_sampler %34
         %37 = OpSampledImage %type_sampled_image %32 %35
         %38 = OpImageSampleImplicitLod %v4float %37 %10 None
         %39 = OpAccessChain %_ptr_UniformConstant_type_2d_image %globalS %int_1 %int_1 %int_0 %int_1
         %40 = OpLoad %type_2d_image %39
         %41 = OpAccessChain %_ptr_UniformConstant_type_sampler %globalS %int_1 %int_1 %int_1 %int_1
         %42 = OpLoad %type_sampler %41
         %43 = OpSampledImage %type_sampled_image %40 %42
         %44 = OpImageSampleImplicitLod %v4float %43 %10 None
         %45 = OpFAdd %v4float %38 %44
               OpReturnValue %45
               OpFunctionEnd
  )";
}

TEST_F(DescriptorScalarReplacementTest, ExpandArrayOfTextures) {
  const std::string text = R"(
; CHECK: OpDecorate [[var1:%\w+]] DescriptorSet 0
; CHECK: OpDecorate [[var1]] Binding 0
; CHECK: OpDecorate [[var2:%\w+]] DescriptorSet 0
; CHECK: OpDecorate [[var2]] Binding 1
; CHECK: OpDecorate [[var3:%\w+]] DescriptorSet 0
; CHECK: OpDecorate [[var3]] Binding 2
; CHECK: OpDecorate [[var4:%\w+]] DescriptorSet 0
; CHECK: OpDecorate [[var4]] Binding 3
; CHECK: OpDecorate [[var5:%\w+]] DescriptorSet 0
; CHECK: OpDecorate [[var5]] Binding 4
; CHECK: [[image_type:%\w+]] = OpTypeImage
; CHECK: [[ptr_type:%\w+]] = OpTypePointer UniformConstant [[image_type]]
; CHECK: [[var1]] = OpVariable [[ptr_type]] UniformConstant
; CHECK: [[var2]] = OpVariable [[ptr_type]] UniformConstant
; CHECK: [[var3]] = OpVariable [[ptr_type]] UniformConstant
; CHECK: [[var4]] = OpVariable [[ptr_type]] UniformConstant
; CHECK: [[var5]] = OpVariable [[ptr_type]] UniformConstant
; CHECK: OpLoad [[image_type]] [[var1]]
; CHECK: OpLoad [[image_type]] [[var2]]
; CHECK: OpLoad [[image_type]] [[var3]]
; CHECK: OpLoad [[image_type]] [[var4]]
; CHECK: OpLoad [[image_type]] [[var5]]
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main"
               OpExecutionMode %main OriginUpperLeft
               OpSource HLSL 600
               OpDecorate %MyTextures DescriptorSet 0
               OpDecorate %MyTextures Binding 0
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
      %int_1 = OpConstant %int 1
      %int_2 = OpConstant %int 2
      %int_3 = OpConstant %int 3
      %int_4 = OpConstant %int 4
       %uint = OpTypeInt 32 0
     %uint_5 = OpConstant %uint 5
      %float = OpTypeFloat 32
%type_2d_image = OpTypeImage %float 2D 2 0 0 1 Unknown
%_arr_type_2d_image_uint_5 = OpTypeArray %type_2d_image %uint_5
%_ptr_UniformConstant__arr_type_2d_image_uint_5 = OpTypePointer UniformConstant %_arr_type_2d_image_uint_5
    %v2float = OpTypeVector %float 2
       %void = OpTypeVoid
         %26 = OpTypeFunction %void
%_ptr_UniformConstant_type_2d_image = OpTypePointer UniformConstant %type_2d_image
 %MyTextures = OpVariable %_ptr_UniformConstant__arr_type_2d_image_uint_5 UniformConstant
       %main = OpFunction %void None %26
         %28 = OpLabel
         %29 = OpUndef %v2float
         %30 = OpAccessChain %_ptr_UniformConstant_type_2d_image %MyTextures %int_0
         %31 = OpLoad %type_2d_image %30
         %35 = OpAccessChain %_ptr_UniformConstant_type_2d_image %MyTextures %int_1
         %36 = OpLoad %type_2d_image %35
         %40 = OpAccessChain %_ptr_UniformConstant_type_2d_image %MyTextures %int_2
         %41 = OpLoad %type_2d_image %40
         %45 = OpAccessChain %_ptr_UniformConstant_type_2d_image %MyTextures %int_3
         %46 = OpLoad %type_2d_image %45
         %50 = OpAccessChain %_ptr_UniformConstant_type_2d_image %MyTextures %int_4
         %51 = OpLoad %type_2d_image %50
               OpReturn
               OpFunctionEnd

  )";

  SinglePassRunAndMatch<DescriptorScalarReplacement>(
      text, true, /* flatten_composites=*/true, /* flatten_arrays=*/true);
}

TEST_F(DescriptorScalarReplacementTest, ExpandArrayOfSamplers) {
  const std::string text = R"(
; CHECK: OpDecorate [[var1:%\w+]] DescriptorSet 0
; CHECK: OpDecorate [[var1]] Binding 1
; CHECK: OpDecorate [[var2:%\w+]] DescriptorSet 0
; CHECK: OpDecorate [[var2]] Binding 2
; CHECK: OpDecorate [[var3:%\w+]] DescriptorSet 0
; CHECK: OpDecorate [[var3]] Binding 3
; CHECK: [[sampler_type:%\w+]] = OpTypeSampler
; CHECK: [[ptr_type:%\w+]] = OpTypePointer UniformConstant [[sampler_type]]
; CHECK: [[var1]] = OpVariable [[ptr_type]] UniformConstant
; CHECK: [[var2]] = OpVariable [[ptr_type]] UniformConstant
; CHECK: [[var3]] = OpVariable [[ptr_type]] UniformConstant
; CHECK: OpLoad [[sampler_type]] [[var1]]
; CHECK: OpLoad [[sampler_type]] [[var2]]
; CHECK: OpLoad [[sampler_type]] [[var3]]
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main"
               OpExecutionMode %main OriginUpperLeft
               OpSource HLSL 600
               OpDecorate %MySampler DescriptorSet 0
               OpDecorate %MySampler Binding 1
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
      %int_1 = OpConstant %int 1
      %int_2 = OpConstant %int 2
       %uint = OpTypeInt 32 0
     %uint_3 = OpConstant %uint 3
%type_sampler = OpTypeSampler
%_arr_type_sampler_uint_3 = OpTypeArray %type_sampler %uint_3
%_ptr_UniformConstant__arr_type_sampler_uint_3 = OpTypePointer UniformConstant %_arr_type_sampler_uint_3
       %void = OpTypeVoid
         %26 = OpTypeFunction %void
%_ptr_UniformConstant_type_sampler = OpTypePointer UniformConstant %type_sampler
  %MySampler = OpVariable %_ptr_UniformConstant__arr_type_sampler_uint_3 UniformConstant
       %main = OpFunction %void None %26
         %28 = OpLabel
         %31 = OpAccessChain %_ptr_UniformConstant_type_sampler %MySampler %int_0
         %32 = OpLoad %type_sampler %31
         %35 = OpAccessChain %_ptr_UniformConstant_type_sampler %MySampler %int_1
         %36 = OpLoad %type_sampler %35
         %40 = OpAccessChain %_ptr_UniformConstant_type_sampler %MySampler %int_2
         %41 = OpLoad %type_sampler %40
               OpReturn
               OpFunctionEnd
  )";

  SinglePassRunAndMatch<DescriptorScalarReplacement>(
      text, true, /* flatten_composites=*/true, /* flatten_arrays=*/true);
}

TEST_F(DescriptorScalarReplacementTest, ExpandArrayOfSSBOs) {
  // Tests the expansion of an SSBO.  Also check that an access chain with more
  // than 1 index is correctly handled.
  const std::string text = R"(
; CHECK: OpDecorate [[var1:%\w+]] DescriptorSet 0
; CHECK: OpDecorate [[var1]] Binding 0
; CHECK: OpDecorate [[var2:%\w+]] DescriptorSet 0
; CHECK: OpDecorate [[var2]] Binding 1
; CHECK: OpTypeStruct
; CHECK: [[struct_type:%\w+]] = OpTypeStruct
; CHECK: [[ptr_type:%\w+]] = OpTypePointer Uniform [[struct_type]]
; CHECK: [[var1]] = OpVariable [[ptr_type]] Uniform
; CHECK: [[var2]] = OpVariable [[ptr_type]] Uniform
; CHECK: [[ac1:%\w+]] = OpAccessChain %_ptr_Uniform_v4float [[var1]] %uint_0 %uint_0 %uint_0
; CHECK: OpLoad %v4float [[ac1]]
; CHECK: [[ac2:%\w+]] = OpAccessChain %_ptr_Uniform_v4float [[var2]] %uint_0 %uint_0 %uint_0
; CHECK: OpLoad %v4float [[ac2]]
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main"
               OpExecutionMode %main OriginUpperLeft
               OpSource HLSL 600
               OpDecorate %buffers DescriptorSet 0
               OpDecorate %buffers Binding 0
               OpMemberDecorate %S 0 Offset 0
               OpDecorate %_runtimearr_S ArrayStride 16
               OpMemberDecorate %type_StructuredBuffer_S 0 Offset 0
               OpMemberDecorate %type_StructuredBuffer_S 0 NonWritable
               OpDecorate %type_StructuredBuffer_S BufferBlock
       %uint = OpTypeInt 32 0
     %uint_0 = OpConstant %uint 0
      %uint_1 = OpConstant %uint 1
     %uint_2 = OpConstant %uint 2
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
          %S = OpTypeStruct %v4float
%_runtimearr_S = OpTypeRuntimeArray %S
%type_StructuredBuffer_S = OpTypeStruct %_runtimearr_S
%_arr_type_StructuredBuffer_S_uint_2 = OpTypeArray %type_StructuredBuffer_S %uint_2
%_ptr_Uniform__arr_type_StructuredBuffer_S_uint_2 = OpTypePointer Uniform %_arr_type_StructuredBuffer_S_uint_2
%_ptr_Uniform_type_StructuredBuffer_S = OpTypePointer Uniform %type_StructuredBuffer_S
       %void = OpTypeVoid
         %19 = OpTypeFunction %void
%_ptr_Uniform_v4float = OpTypePointer Uniform %v4float
    %buffers = OpVariable %_ptr_Uniform__arr_type_StructuredBuffer_S_uint_2 Uniform
       %main = OpFunction %void None %19
         %21 = OpLabel
         %22 = OpAccessChain %_ptr_Uniform_v4float %buffers %uint_0 %uint_0 %uint_0 %uint_0
         %23 = OpLoad %v4float %22
         %24 = OpAccessChain %_ptr_Uniform_type_StructuredBuffer_S %buffers %uint_1
         %25 = OpAccessChain %_ptr_Uniform_v4float %24 %uint_0 %uint_0 %uint_0
         %26 = OpLoad %v4float %25
               OpReturn
               OpFunctionEnd
  )";

  SinglePassRunAndMatch<DescriptorScalarReplacement>(
      text, true, /* flatten_composites=*/true, /* flatten_arrays=*/true);
}

TEST_F(DescriptorScalarReplacementTest, NameNewVariables) {
  // Checks that if the original variable has a name, then the new variables
  // will have a name derived from that name.
  const std::string text = R"(
; CHECK: OpName [[var1:%\w+]] "SSBO[0]"
; CHECK: OpName [[var2:%\w+]] "SSBO[1]"
; CHECK: OpDecorate [[var1]] DescriptorSet 0
; CHECK: OpDecorate [[var1]] Binding 0
; CHECK: OpDecorate [[var2]] DescriptorSet 0
; CHECK: OpDecorate [[var2]] Binding 1
; CHECK: OpTypeStruct
; CHECK: [[struct_type:%\w+]] = OpTypeStruct
; CHECK: [[ptr_type:%\w+]] = OpTypePointer Uniform [[struct_type]]
; CHECK: [[var1]] = OpVariable [[ptr_type]] Uniform
; CHECK: [[var2]] = OpVariable [[ptr_type]] Uniform
; CHECK: [[ac1:%\w+]] = OpAccessChain %_ptr_Uniform_v4float [[var1]] %uint_0 %uint_0 %uint_0
; CHECK: OpLoad %v4float [[ac1]]
; CHECK: [[ac2:%\w+]] = OpAccessChain %_ptr_Uniform_v4float [[var2]] %uint_0 %uint_0 %uint_0
; CHECK: OpLoad %v4float [[ac2]]
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main"
               OpExecutionMode %main OriginUpperLeft
               OpSource HLSL 600
               OpName %buffers "SSBO"
               OpDecorate %buffers DescriptorSet 0
               OpDecorate %buffers Binding 0
               OpMemberDecorate %S 0 Offset 0
               OpDecorate %_runtimearr_S ArrayStride 16
               OpMemberDecorate %type_StructuredBuffer_S 0 Offset 0
               OpMemberDecorate %type_StructuredBuffer_S 0 NonWritable
               OpDecorate %type_StructuredBuffer_S BufferBlock
       %uint = OpTypeInt 32 0
     %uint_0 = OpConstant %uint 0
      %uint_1 = OpConstant %uint 1
     %uint_2 = OpConstant %uint 2
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
          %S = OpTypeStruct %v4float
%_runtimearr_S = OpTypeRuntimeArray %S
%type_StructuredBuffer_S = OpTypeStruct %_runtimearr_S
%_arr_type_StructuredBuffer_S_uint_2 = OpTypeArray %type_StructuredBuffer_S %uint_2
%_ptr_Uniform__arr_type_StructuredBuffer_S_uint_2 = OpTypePointer Uniform %_arr_type_StructuredBuffer_S_uint_2
%_ptr_Uniform_type_StructuredBuffer_S = OpTypePointer Uniform %type_StructuredBuffer_S
       %void = OpTypeVoid
         %19 = OpTypeFunction %void
%_ptr_Uniform_v4float = OpTypePointer Uniform %v4float
    %buffers = OpVariable %_ptr_Uniform__arr_type_StructuredBuffer_S_uint_2 Uniform
       %main = OpFunction %void None %19
         %21 = OpLabel
         %22 = OpAccessChain %_ptr_Uniform_v4float %buffers %uint_0 %uint_0 %uint_0 %uint_0
         %23 = OpLoad %v4float %22
         %24 = OpAccessChain %_ptr_Uniform_type_StructuredBuffer_S %buffers %uint_1
         %25 = OpAccessChain %_ptr_Uniform_v4float %24 %uint_0 %uint_0 %uint_0
         %26 = OpLoad %v4float %25
               OpReturn
               OpFunctionEnd
  )";

  SinglePassRunAndMatch<DescriptorScalarReplacement>(
      text, true, /* flatten_composites=*/true, /* flatten_arrays=*/true);
}

TEST_F(DescriptorScalarReplacementTest, DontExpandCBuffers) {
  // Checks that constant buffers are not expanded.
  // Constant buffers are represented as global structures, but they should not
  // be replaced with new variables for their elements.
  /*
    cbuffer MyCbuffer : register(b1) {
      float2    a;
      float2   b;
    };
    float main() : A {
      return a.x + b.y;
    }
  */
  const std::string text = R"(
; CHECK: OpAccessChain %_ptr_Uniform_float %MyCbuffer %int_0 %int_0
; CHECK: OpAccessChain %_ptr_Uniform_float %MyCbuffer %int_1 %int_1
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main" %out_var_A
               OpSource HLSL 600
               OpName %type_MyCbuffer "type.MyCbuffer"
               OpMemberName %type_MyCbuffer 0 "a"
               OpMemberName %type_MyCbuffer 1 "b"
               OpName %MyCbuffer "MyCbuffer"
               OpName %out_var_A "out.var.A"
               OpName %main "main"
               OpDecorate %out_var_A Location 0
               OpDecorate %MyCbuffer DescriptorSet 0
               OpDecorate %MyCbuffer Binding 1
               OpMemberDecorate %type_MyCbuffer 0 Offset 0
               OpMemberDecorate %type_MyCbuffer 1 Offset 8
               OpDecorate %type_MyCbuffer Block
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
      %int_1 = OpConstant %int 1
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
%type_MyCbuffer = OpTypeStruct %v2float %v2float
%_ptr_Uniform_type_MyCbuffer = OpTypePointer Uniform %type_MyCbuffer
%_ptr_Output_float = OpTypePointer Output %float
       %void = OpTypeVoid
         %13 = OpTypeFunction %void
%_ptr_Uniform_float = OpTypePointer Uniform %float
  %MyCbuffer = OpVariable %_ptr_Uniform_type_MyCbuffer Uniform
  %out_var_A = OpVariable %_ptr_Output_float Output
       %main = OpFunction %void None %13
         %15 = OpLabel
         %16 = OpAccessChain %_ptr_Uniform_float %MyCbuffer %int_0 %int_0
         %17 = OpLoad %float %16
         %18 = OpAccessChain %_ptr_Uniform_float %MyCbuffer %int_1 %int_1
         %19 = OpLoad %float %18
         %20 = OpFAdd %float %17 %19
               OpStore %out_var_A %20
               OpReturn
               OpFunctionEnd
)";

  SinglePassRunAndMatch<DescriptorScalarReplacement>(
      text, true, /* flatten_composites=*/true, /* flatten_arrays=*/true);
}

TEST_F(DescriptorScalarReplacementTest, DontExpandStructuredBuffers) {
  // Checks that structured buffers are not expanded.
  // Structured buffers are represented as global structures, that have one
  // member which is a runtime array.
  /*
    struct S {
      float2   a;
      float2   b;
    };
    RWStructuredBuffer<S> sb;
    float main() : A {
      return sb[0].a.x + sb[0].b.x;
    }
  */
  const std::string text = R"(
; CHECK: OpAccessChain %_ptr_Uniform_float %sb %int_0 %uint_0 %int_0 %int_0
; CHECK: OpAccessChain %_ptr_Uniform_float %sb %int_0 %uint_0 %int_1 %int_0
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Vertex %main "main" %out_var_A
               OpName %type_RWStructuredBuffer_S "type.RWStructuredBuffer.S"
               OpName %S "S"
               OpMemberName %S 0 "a"
               OpMemberName %S 1 "b"
               OpName %sb "sb"
               OpName %out_var_A "out.var.A"
               OpName %main "main"
               OpDecorate %out_var_A Location 0
               OpDecorate %sb DescriptorSet 0
               OpDecorate %sb Binding 0
               OpMemberDecorate %S 0 Offset 0
               OpMemberDecorate %S 1 Offset 8
               OpDecorate %_runtimearr_S ArrayStride 16
               OpMemberDecorate %type_RWStructuredBuffer_S 0 Offset 0
               OpDecorate %type_RWStructuredBuffer_S BufferBlock
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
       %uint = OpTypeInt 32 0
     %uint_0 = OpConstant %uint 0
      %int_1 = OpConstant %int 1
      %float = OpTypeFloat 32
    %v2float = OpTypeVector %float 2
          %S = OpTypeStruct %v2float %v2float
%_runtimearr_S = OpTypeRuntimeArray %S
%type_RWStructuredBuffer_S = OpTypeStruct %_runtimearr_S
%_ptr_Uniform_type_RWStructuredBuffer_S = OpTypePointer Uniform %type_RWStructuredBuffer_S
%_ptr_Output_float = OpTypePointer Output %float
       %void = OpTypeVoid
         %17 = OpTypeFunction %void
%_ptr_Uniform_float = OpTypePointer Uniform %float
         %sb = OpVariable %_ptr_Uniform_type_RWStructuredBuffer_S Uniform
  %out_var_A = OpVariable %_ptr_Output_float Output
       %main = OpFunction %void None %17
         %19 = OpLabel
         %20 = OpAccessChain %_ptr_Uniform_float %sb %int_0 %uint_0 %int_0 %int_0
         %21 = OpLoad %float %20
         %22 = OpAccessChain %_ptr_Uniform_float %sb %int_0 %uint_0 %int_1 %int_0
         %23 = OpLoad %float %22
         %24 = OpFAdd %float %21 %23
               OpStore %out_var_A %24
               OpReturn
               OpFunctionEnd
)";

  SinglePassRunAndMatch<DescriptorScalarReplacement>(
      text, true, /* flatten_composites=*/true, /* flatten_arrays=*/true);
}

TEST_F(DescriptorScalarReplacementTest, StructureArrayNames) {
  // Checks that names are properly generated for multi-dimension arrays and
  // structure members.
  const std::string checks = R"(
; CHECK: OpName %globalS_0__0__t_0_ "globalS[0][0].t[0]"
; CHECK: OpName %globalS_0__0__s_0_ "globalS[0][0].s[0]"
; CHECK: OpName %globalS_1__1__t_1_ "globalS[1][1].t[1]"
; CHECK: OpName %globalS_1__1__s_1_ "globalS[1][1].s[1]"
  )";

  const std::string text = checks + GetStructureArrayTestSpirv();
  SinglePassRunAndMatch<DescriptorScalarReplacement>(
      text, true, /* flatten_composites=*/true, /* flatten_arrays=*/true);
}

TEST_F(DescriptorScalarReplacementTest,
       FlattensArraysOfStructsButNoResourceArrays) {
  // Check that only the composite array is flattenned, but internal resource
  // arrays are left as-is.
  const std::string checks = R"(
; CHECK:     OpName %globalS_0__0__t "globalS[0][0].t"
; CHECK:     OpName %globalS_0__0__s "globalS[0][0].s"
; CHECK:     OpName %globalS_1__1__t "globalS[1][1].t"
; CHECK:     OpName %globalS_1__1__s "globalS[1][1].s"
; CHECK-NOT: OpName %globalS_1__1__t_0_
; CHECK-NOT: OpName %globalS_1__1__s_0_
  )";

  const std::string text = checks + GetStructureArrayTestSpirv();
  SinglePassRunAndMatch<DescriptorScalarReplacement>(
      text, true, /* flatten_composites=*/true, /* flatten_arrays=*/false);
}

TEST_F(DescriptorScalarReplacementTest, FlattenNothingIfAskedTo) {
  // Not useful, but checks what happens if both are set to false.
  // In such case, nothing happens.
  const std::string checks = R"(
; CHECK:     OpName %globalS
; CHECK-NOT: OpName %globalS_
  )";

  const std::string text = checks + GetStructureArrayTestSpirv();
  SinglePassRunAndMatch<DescriptorScalarReplacement>(
      text, true, /* flatten_composites=*/false, /* flatten_arrays=*/false);
}

TEST_F(DescriptorScalarReplacementTest, StructureArrayBindings) {
  // Checks that flattening structures and arrays results in correct binding
  // numbers.
  const std::string checks = R"(
; CHECK: OpDecorate %globalS_0__0__t_0_ Binding 0
; CHECK: OpDecorate %globalS_0__0__s_0_ Binding 2
; CHECK: OpDecorate %globalS_1__1__t_1_ Binding 13
; CHECK: OpDecorate %globalS_1__1__s_1_ Binding 15
  )";

  const std::string text = checks + GetStructureArrayTestSpirv();
  SinglePassRunAndMatch<DescriptorScalarReplacement>(
      text, true, /* flatten_composites=*/true, /* flatten_arrays=*/true);
}

TEST_F(DescriptorScalarReplacementTest, StructureArrayReplacements) {
  // Checks that all access chains indexing into structures and/or arrays are
  // replaced with direct access to replacement variables.
  const std::string checks = R"(
; CHECK-NOT: OpAccessChain
; CHECK: OpLoad %type_2d_image %globalS_0__0__t_0_
; CHECK: OpLoad %type_sampler %globalS_0__0__s_0_
; CHECK: OpLoad %type_2d_image %globalS_1__1__t_1_
; CHECK: OpLoad %type_sampler %globalS_1__1__s_1_
  )";

  const std::string text = checks + GetStructureArrayTestSpirv();
  SinglePassRunAndMatch<DescriptorScalarReplacement>(
      text, true, /* flatten_composites=*/true, /* flatten_arrays=*/true);
}

TEST_F(DescriptorScalarReplacementTest, ResourceStructAsFunctionParam) {
  // Checks that a mix of OpAccessChain, OpLoad, and OpCompositeExtract patterns
  // can be properly replaced with replacement variables.
  // This pattern can be seen when a global structure of resources is passed to
  // a function.

  /* High-level source:
  // globalS[0].t[0]        binding: 0  (used)
  // globalS[0].t[1]        binding: 1  (used)
  // globalS[0].tt[0].s[0]  binding: 2
  // globalS[0].tt[0].s[1]  binding: 3  (used)
  // globalS[0].tt[0].s[2]  binding: 4
  // globalS[0].tt[1].s[0]  binding: 5
  // globalS[0].tt[1].s[1]  binding: 6
  // globalS[0].tt[1].s[2]  binding: 7  (used)
  // globalS[1].t[0]        binding: 8  (used)
  // globalS[1].t[1]        binding: 9  (used)
  // globalS[1].tt[0].s[0]  binding: 10
  // globalS[1].tt[0].s[1]  binding: 11 (used)
  // globalS[1].tt[0].s[2]  binding: 12
  // globalS[1].tt[1].s[0]  binding: 13
  // globalS[1].tt[1].s[1]  binding: 14
  // globalS[1].tt[1].s[2]  binding: 15 (used)

  struct T {
    SamplerState s[3];
  };

  struct S {
    Texture2D t[2];
    T tt[2];
  };

  float4 tex2D(S x, float2 v) {
    return x.t[0].Sample(x.tt[0].s[1], v) + x.t[1].Sample(x.tt[1].s[2], v);
  }

  S globalS[2];

  float4 main() : SV_Target {
    return tex2D(globalS[0], float2(0,0)) + tex2D(globalS[1], float2(0,0)) ;
  }
  */
  const std::string shader = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %out_var_SV_Target
               OpExecutionMode %main OriginUpperLeft
               OpName %S "S"
               OpMemberName %S 0 "t"
               OpMemberName %S 1 "tt"
               OpName %type_2d_image "type.2d.image"
               OpName %T "T"
               OpMemberName %T 0 "s"
               OpName %type_sampler "type.sampler"
               OpName %globalS "globalS"
               OpName %out_var_SV_Target "out.var.SV_Target"
               OpName %main "main"
               OpName %type_sampled_image "type.sampled.image"
               OpDecorate %out_var_SV_Target Location 0
               OpDecorate %globalS DescriptorSet 0
               OpDecorate %globalS Binding 0
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
      %float = OpTypeFloat 32
    %float_0 = OpConstant %float 0
    %v2float = OpTypeVector %float 2
         %14 = OpConstantComposite %v2float %float_0 %float_0
      %int_1 = OpConstant %int 1
       %uint = OpTypeInt 32 0
     %uint_2 = OpConstant %uint 2
%type_2d_image = OpTypeImage %float 2D 2 0 0 1 Unknown
%_arr_type_2d_image_uint_2 = OpTypeArray %type_2d_image %uint_2
     %uint_3 = OpConstant %uint 3
%type_sampler = OpTypeSampler
%_arr_type_sampler_uint_3 = OpTypeArray %type_sampler %uint_3
          %T = OpTypeStruct %_arr_type_sampler_uint_3
%_arr_T_uint_2 = OpTypeArray %T %uint_2
          %S = OpTypeStruct %_arr_type_2d_image_uint_2 %_arr_T_uint_2
%_arr_S_uint_2 = OpTypeArray %S %uint_2
%_ptr_UniformConstant__arr_S_uint_2 = OpTypePointer UniformConstant %_arr_S_uint_2
    %v4float = OpTypeVector %float 4
%_ptr_Output_v4float = OpTypePointer Output %v4float
       %void = OpTypeVoid
         %27 = OpTypeFunction %void
%_ptr_UniformConstant_S = OpTypePointer UniformConstant %S
%type_sampled_image = OpTypeSampledImage %type_2d_image
    %globalS = OpVariable %_ptr_UniformConstant__arr_S_uint_2 UniformConstant
%out_var_SV_Target = OpVariable %_ptr_Output_v4float Output
       %main = OpFunction %void None %27
         %29 = OpLabel
         %30 = OpAccessChain %_ptr_UniformConstant_S %globalS %int_0
         %31 = OpLoad %S %30
         %32 = OpCompositeExtract %_arr_type_2d_image_uint_2 %31 0
         %33 = OpCompositeExtract %type_2d_image %32 0
         %34 = OpCompositeExtract %type_2d_image %32 1
         %35 = OpCompositeExtract %_arr_T_uint_2 %31 1
         %36 = OpCompositeExtract %T %35 0
         %37 = OpCompositeExtract %_arr_type_sampler_uint_3 %36 0
         %38 = OpCompositeExtract %type_sampler %37 1
         %39 = OpCompositeExtract %T %35 1
         %40 = OpCompositeExtract %_arr_type_sampler_uint_3 %39 0
         %41 = OpCompositeExtract %type_sampler %40 2
         %42 = OpSampledImage %type_sampled_image %33 %38
         %43 = OpImageSampleImplicitLod %v4float %42 %14 None
         %44 = OpSampledImage %type_sampled_image %34 %41
         %45 = OpImageSampleImplicitLod %v4float %44 %14 None
         %46 = OpFAdd %v4float %43 %45
         %47 = OpAccessChain %_ptr_UniformConstant_S %globalS %int_1
         %48 = OpLoad %S %47
         %49 = OpCompositeExtract %_arr_type_2d_image_uint_2 %48 0
         %50 = OpCompositeExtract %type_2d_image %49 0
         %51 = OpCompositeExtract %type_2d_image %49 1
         %52 = OpCompositeExtract %_arr_T_uint_2 %48 1
         %53 = OpCompositeExtract %T %52 0
         %54 = OpCompositeExtract %_arr_type_sampler_uint_3 %53 0
         %55 = OpCompositeExtract %type_sampler %54 1
         %56 = OpCompositeExtract %T %52 1
         %57 = OpCompositeExtract %_arr_type_sampler_uint_3 %56 0
         %58 = OpCompositeExtract %type_sampler %57 2
         %59 = OpSampledImage %type_sampled_image %50 %55
         %60 = OpImageSampleImplicitLod %v4float %59 %14 None
         %61 = OpSampledImage %type_sampled_image %51 %58
         %62 = OpImageSampleImplicitLod %v4float %61 %14 None
         %63 = OpFAdd %v4float %60 %62
         %64 = OpFAdd %v4float %46 %63
               OpStore %out_var_SV_Target %64
               OpReturn
               OpFunctionEnd
)";

  const std::string checks = R"(
; CHECK: OpName %globalS_0__t_0_ "globalS[0].t[0]"
; CHECK: OpName %globalS_0__t_1_ "globalS[0].t[1]"
; CHECK: OpName %globalS_1__t_0_ "globalS[1].t[0]"
; CHECK: OpName %globalS_1__t_1_ "globalS[1].t[1]"
; CHECK: OpName %globalS_0__tt_0__s_1_ "globalS[0].tt[0].s[1]"
; CHECK: OpName %globalS_0__tt_1__s_2_ "globalS[0].tt[1].s[2]"
; CHECK: OpName %globalS_1__tt_0__s_1_ "globalS[1].tt[0].s[1]"
; CHECK: OpName %globalS_1__tt_1__s_2_ "globalS[1].tt[1].s[2]"
; CHECK: OpDecorate %globalS_0__t_0_ Binding 0
; CHECK: OpDecorate %globalS_0__t_1_ Binding 1
; CHECK: OpDecorate %globalS_1__t_0_ Binding 8
; CHECK: OpDecorate %globalS_1__t_1_ Binding 9
; CHECK: OpDecorate %globalS_0__tt_0__s_1_ Binding 3
; CHECK: OpDecorate %globalS_0__tt_1__s_2_ Binding 7
; CHECK: OpDecorate %globalS_1__tt_0__s_1_ Binding 11
; CHECK: OpDecorate %globalS_1__tt_1__s_2_ Binding 15

; CHECK: %globalS_0__t_0_ = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
; CHECK: %globalS_0__t_1_ = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
; CHECK: %globalS_1__t_0_ = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
; CHECK: %globalS_1__t_1_ = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
; CHECK: %globalS_0__tt_0__s_1_ = OpVariable %_ptr_UniformConstant_type_sampler UniformConstant
; CHECK: %globalS_0__tt_1__s_2_ = OpVariable %_ptr_UniformConstant_type_sampler UniformConstant
; CHECK: %globalS_1__tt_0__s_1_ = OpVariable %_ptr_UniformConstant_type_sampler UniformConstant
; CHECK: %globalS_1__tt_1__s_2_ = OpVariable %_ptr_UniformConstant_type_sampler UniformConstant

; CHECK:     [[img_1:%\w+]] = OpLoad %type_2d_image %globalS_0__t_0_
; CHECK:     [[img_2:%\w+]] = OpLoad %type_2d_image %globalS_0__t_1_
; CHECK: [[sampler_1:%\w+]] = OpLoad %type_sampler %globalS_0__tt_0__s_1_
; CHECK: [[sampler_2:%\w+]] = OpLoad %type_sampler %globalS_0__tt_1__s_2_

; CHECK: [[sampled_img_1:%\w+]] = OpSampledImage %type_sampled_image [[img_1]] [[sampler_1]]
; CHECK:      [[sample_1:%\w+]] = OpImageSampleImplicitLod %v4float [[sampled_img_1]]
; CHECK: [[sampled_img_2:%\w+]] = OpSampledImage %type_sampled_image [[img_2]] [[sampler_2]]
; CHECK:      [[sample_2:%\w+]] = OpImageSampleImplicitLod %v4float [[sampled_img_2]]
; CHECK:                          OpFAdd %v4float [[sample_1]] [[sample_2]]

; CHECK:     [[img_3:%\w+]] = OpLoad %type_2d_image %globalS_1__t_0_
; CHECK:     [[img_4:%\w+]] = OpLoad %type_2d_image %globalS_1__t_1_
; CHECK: [[sampler_3:%\w+]] = OpLoad %type_sampler %globalS_1__tt_0__s_1_
; CHECK: [[sampler_4:%\w+]] = OpLoad %type_sampler %globalS_1__tt_1__s_2_

; CHECK: [[sampled_img_3:%\w+]] = OpSampledImage %type_sampled_image [[img_3]] [[sampler_3]]
; CHECK:      [[sample_3:%\w+]] = OpImageSampleImplicitLod %v4float [[sampled_img_3]]
; CHECK: [[sampled_img_4:%\w+]] = OpSampledImage %type_sampled_image [[img_4]] [[sampler_4]]
; CHECK:      [[sample_4:%\w+]] = OpImageSampleImplicitLod %v4float [[sampled_img_4]]
; CHECK:                          OpFAdd %v4float [[sample_3]] [[sample_4]]
)";

  SinglePassRunAndMatch<DescriptorScalarReplacement>(
      checks + shader, true, /* flatten_composites=*/true,
      /* flatten_arrays=*/true);
}

TEST_F(DescriptorScalarReplacementTest, BindingForResourceArrayOfStructs) {
  // Check that correct binding numbers are given to an array of descriptors
  // to structs.

  const std::string shader = R"(
; CHECK: OpDecorate {{%\w+}} Binding 0
; CHECK: OpDecorate {{%\w+}} Binding 1
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %2 "psmain"
               OpExecutionMode %2 OriginUpperLeft
               OpDecorate %5 DescriptorSet 0
               OpDecorate %5 Binding 0
               OpMemberDecorate %_struct_4 0 Offset 0
               OpMemberDecorate %_struct_4 1 Offset 4
               OpDecorate %_struct_4 Block
      %float = OpTypeFloat 32
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
      %int_1 = OpConstant %int 1
       %uint = OpTypeInt 32 0
     %uint_2 = OpConstant %uint 2
  %_struct_4 = OpTypeStruct %float %int
%_arr__struct_4_uint_2 = OpTypeArray %_struct_4 %uint_2
%_ptr_Uniform__arr__struct_4_uint_2 = OpTypePointer Uniform %_arr__struct_4_uint_2
       %void = OpTypeVoid
         %25 = OpTypeFunction %void
%_ptr_Uniform_int = OpTypePointer Uniform %int
          %5 = OpVariable %_ptr_Uniform__arr__struct_4_uint_2 Uniform
          %2 = OpFunction %void None %25
         %29 = OpLabel
         %40 = OpAccessChain %_ptr_Uniform_int %5 %int_0 %int_1
         %41 = OpAccessChain %_ptr_Uniform_int %5 %int_1 %int_1
               OpReturn
               OpFunctionEnd
)";

  SinglePassRunAndMatch<DescriptorScalarReplacement>(
      shader, true, /* flatten_composites=*/true, /* flatten_arrays=*/true);
}

TEST_F(DescriptorScalarReplacementTest, MemberDecorationForResourceStruct) {
  // Check that an OpMemberDecorate instruction is correctly converted to a
  // OpDecorate instruction.

  const std::string shader = R"(
; CHECK: OpDecorate [[t:%\w+]] DescriptorSet 0
; CHECK: OpDecorate [[t]] Binding 0
; CHECK: OpDecorate [[t]] RelaxedPrecision
; CHECK: OpDecorate [[s:%\w+]] DescriptorSet 0
; CHECK: OpDecorate [[s]] Binding 1
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %PSMain "PSMain" %in_var_TEXCOORD %out_var_SV_Target
               OpExecutionMode %PSMain OriginUpperLeft
               OpSource HLSL 600
               OpName %sampler2D_h "sampler2D_h"
               OpMemberName %sampler2D_h 0 "t"
               OpMemberName %sampler2D_h 1 "s"
               OpName %type_2d_image "type.2d.image"
               OpName %type_sampler "type.sampler"
               OpName %_MainTex "_MainTex"
               OpName %in_var_TEXCOORD "in.var.TEXCOORD"
               OpName %out_var_SV_Target "out.var.SV_Target"
               OpName %PSMain "PSMain"
               OpName %type_sampled_image "type.sampled.image"
               OpDecorate %in_var_TEXCOORD Location 0
               OpDecorate %out_var_SV_Target Location 0
               OpDecorate %_MainTex DescriptorSet 0
               OpDecorate %_MainTex Binding 0
               OpMemberDecorate %sampler2D_h 0 RelaxedPrecision
               OpDecorate %out_var_SV_Target RelaxedPrecision
               OpDecorate %69 RelaxedPrecision
      %float = OpTypeFloat 32
%type_2d_image = OpTypeImage %float 2D 2 0 0 1 Unknown
%type_sampler = OpTypeSampler
%sampler2D_h = OpTypeStruct %type_2d_image %type_sampler
%_ptr_UniformConstant_sampler2D_h = OpTypePointer UniformConstant %sampler2D_h
    %v2float = OpTypeVector %float 2
%_ptr_Input_v2float = OpTypePointer Input %v2float
    %v4float = OpTypeVector %float 4
%_ptr_Output_v4float = OpTypePointer Output %v4float
       %void = OpTypeVoid
         %35 = OpTypeFunction %void
%type_sampled_image = OpTypeSampledImage %type_2d_image
   %_MainTex = OpVariable %_ptr_UniformConstant_sampler2D_h UniformConstant
%in_var_TEXCOORD = OpVariable %_ptr_Input_v2float Input
%out_var_SV_Target = OpVariable %_ptr_Output_v4float Output
     %PSMain = OpFunction %void None %35
         %43 = OpLabel
         %44 = OpLoad %v2float %in_var_TEXCOORD
         %57 = OpLoad %sampler2D_h %_MainTex
         %72 = OpCompositeExtract %type_2d_image %57 0
         %73 = OpCompositeExtract %type_sampler %57 1
         %68 = OpSampledImage %type_sampled_image %72 %73
         %69 = OpImageSampleImplicitLod %v4float %68 %44 None
               OpStore %out_var_SV_Target %69
               OpReturn
               OpFunctionEnd
)";

  SinglePassRunAndMatch<DescriptorScalarReplacement>(
      shader, true, /* flatten_composites=*/true, /* flatten_arrays=*/true);
}

TEST_F(DescriptorScalarReplacementTest, DecorateStringForReflect) {
  // Check that an OpDecorateString instruction is correctly cloned to new
  // variable.

  const std::string shader = R"(
; CHECK: OpName %g_testTextures_0_ "g_testTextures[0]"
; CHECK: OpDecorate %g_testTextures_0_ DescriptorSet 0
; CHECK: OpDecorate %g_testTextures_0_ Binding 0
; CHECK: OpDecorateString %g_testTextures_0_ UserTypeGOOGLE "texture2d"
               OpCapability Shader
               OpExtension "SPV_GOOGLE_hlsl_functionality1"
               OpExtension "SPV_GOOGLE_user_type"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %gl_FragCoord %out_var_SV_Target
               OpExecutionMode %main OriginUpperLeft
               OpSource HLSL 600
               OpName %type_2d_image "type.2d.image"
               OpName %g_testTextures "g_testTextures"
               OpName %out_var_SV_Target "out.var.SV_Target"
               OpName %main "main"
               OpName %param_var_vPixelPos "param.var.vPixelPos"
               OpName %src_main "src.main"
               OpName %vPixelPos "vPixelPos"
               OpName %bb_entry "bb.entry"
               OpDecorate %gl_FragCoord BuiltIn FragCoord
               OpDecorateString %gl_FragCoord UserSemantic "SV_Position"
               OpDecorateString %out_var_SV_Target UserSemantic "SV_Target"
               OpDecorate %out_var_SV_Target Location 0
               OpDecorate %g_testTextures DescriptorSet 0
               OpDecorate %g_testTextures Binding 0
               OpDecorateString %g_testTextures UserTypeGOOGLE "texture2d"
       %uint = OpTypeInt 32 0
     %uint_0 = OpConstant %uint 0
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
     %uint_2 = OpConstant %uint 2
      %float = OpTypeFloat 32
%type_2d_image = OpTypeImage %float 2D 2 0 0 1 Unknown
%_arr_type_2d_image_uint_2 = OpTypeArray %type_2d_image %uint_2
%_ptr_UniformConstant__arr_type_2d_image_uint_2 = OpTypePointer UniformConstant %_arr_type_2d_image_uint_2
    %v4float = OpTypeVector %float 4
%_ptr_Input_v4float = OpTypePointer Input %v4float
%_ptr_Output_v4float = OpTypePointer Output %v4float
       %void = OpTypeVoid
         %18 = OpTypeFunction %void
%_ptr_Function_v4float = OpTypePointer Function %v4float
         %25 = OpTypeFunction %v4float %_ptr_Function_v4float
    %v2float = OpTypeVector %float 2
     %v3uint = OpTypeVector %uint 3
      %v3int = OpTypeVector %int 3
      %v2int = OpTypeVector %int 2
%_ptr_UniformConstant_type_2d_image = OpTypePointer UniformConstant %type_2d_image
%g_testTextures = OpVariable %_ptr_UniformConstant__arr_type_2d_image_uint_2 UniformConstant
%gl_FragCoord = OpVariable %_ptr_Input_v4float Input
%out_var_SV_Target = OpVariable %_ptr_Output_v4float Output
       %main = OpFunction %void None %18
         %19 = OpLabel
%param_var_vPixelPos = OpVariable %_ptr_Function_v4float Function
         %22 = OpLoad %v4float %gl_FragCoord
               OpStore %param_var_vPixelPos %22
         %23 = OpFunctionCall %v4float %src_main %param_var_vPixelPos
               OpStore %out_var_SV_Target %23
               OpReturn
               OpFunctionEnd
   %src_main = OpFunction %v4float None %25
  %vPixelPos = OpFunctionParameter %_ptr_Function_v4float
   %bb_entry = OpLabel
         %28 = OpLoad %v4float %vPixelPos
         %30 = OpVectorShuffle %v2float %28 %28 0 1
         %31 = OpCompositeExtract %float %30 0
         %32 = OpCompositeExtract %float %30 1
         %33 = OpConvertFToU %uint %31
         %34 = OpConvertFToU %uint %32
         %36 = OpCompositeConstruct %v3uint %33 %34 %uint_0
         %38 = OpBitcast %v3int %36
         %40 = OpVectorShuffle %v2int %38 %38 0 1
         %41 = OpCompositeExtract %int %38 2
         %43 = OpAccessChain %_ptr_UniformConstant_type_2d_image %g_testTextures %int_0
         %44 = OpLoad %type_2d_image %43
         %45 = OpImageFetch %v4float %44 %40 Lod %41
               OpReturnValue %45
               OpFunctionEnd
)";

  SinglePassRunAndMatch<DescriptorScalarReplacement>(
      shader, true, /* flatten_composites=*/true, /* flatten_arrays=*/true);
}

TEST_F(DescriptorScalarReplacementTest, ExpandArrayInOpEntryPoint) {
  const std::string text = R"(; SPIR-V
; Version: 1.6
; Bound: 31
; Schema: 0
               OpCapability Shader
               OpMemoryModel Logical GLSL450

; CHECK:       OpEntryPoint GLCompute %main "main" %output_0_ %output_1_

               OpEntryPoint GLCompute %main "main" %output
               OpExecutionMode %main LocalSize 1 1 1
               OpSource HLSL 670
               OpName %type_RWByteAddressBuffer "type.RWByteAddressBuffer"
               OpName %output "output"
               OpName %main "main"
               OpName %src_main "src.main"
               OpName %bb_entry "bb.entry"

; CHECK:       OpDecorate %output_1_ DescriptorSet 0
; CHECK:       OpDecorate %output_1_ Binding 1
; CHECK:       OpDecorate %output_0_ DescriptorSet 0
; CHECK:       OpDecorate %output_0_ Binding 0

               OpDecorate %output DescriptorSet 0
               OpDecorate %output Binding 0

               OpDecorate %_runtimearr_uint ArrayStride 4
               OpMemberDecorate %type_RWByteAddressBuffer 0 Offset 0
               OpDecorate %type_RWByteAddressBuffer Block
        %int = OpTypeInt 32 1
      %int_1 = OpConstant %int 1
       %uint = OpTypeInt 32 0
     %uint_0 = OpConstant %uint 0
     %uint_2 = OpConstant %uint 2
    %uint_32 = OpConstant %uint 32
%_runtimearr_uint = OpTypeRuntimeArray %uint
%type_RWByteAddressBuffer = OpTypeStruct %_runtimearr_uint
%_arr_type_RWByteAddressBuffer_uint_2 = OpTypeArray %type_RWByteAddressBuffer %uint_2
%_ptr_StorageBuffer__arr_type_RWByteAddressBuffer_uint_2 = OpTypePointer StorageBuffer %_arr_type_RWByteAddressBuffer_uint_2
       %void = OpTypeVoid
         %23 = OpTypeFunction %void
%_ptr_StorageBuffer_type_RWByteAddressBuffer = OpTypePointer StorageBuffer %type_RWByteAddressBuffer
%_ptr_StorageBuffer_uint = OpTypePointer StorageBuffer %uint

; CHECK: %output_1_ = OpVariable %_ptr_StorageBuffer_type_RWByteAddressBuffer StorageBuffer
; CHECK: %output_0_ = OpVariable %_ptr_StorageBuffer_type_RWByteAddressBuffer StorageBuffer

     %output = OpVariable %_ptr_StorageBuffer__arr_type_RWByteAddressBuffer_uint_2 StorageBuffer

       %main = OpFunction %void None %23
         %26 = OpLabel
         %27 = OpFunctionCall %void %src_main
               OpReturn
               OpFunctionEnd
   %src_main = OpFunction %void None %23
   %bb_entry = OpLabel
         %28 = OpAccessChain %_ptr_StorageBuffer_type_RWByteAddressBuffer %output %int_1
         %29 = OpShiftRightLogical %uint %uint_0 %uint_2
         %30 = OpAccessChain %_ptr_StorageBuffer_uint %28 %uint_0 %29
               OpStore %30 %uint_32
               OpReturn
               OpFunctionEnd
  )";

  SinglePassRunAndMatch<DescriptorScalarReplacement>(
      text, false, /* flatten_composites=*/true, /* flatten_arrays=*/true);
}

TEST_F(DescriptorScalarReplacementTest,
       ExpandArrayWhenCompositeExpensionIsOff) {
  const std::string text = R"(; SPIR-V
; Version: 1.6
; Bound: 31
; Schema: 0
               OpCapability Shader
               OpMemoryModel Logical GLSL450

; CHECK:       OpEntryPoint GLCompute %main "main" %output_0_ %output_1_

               OpEntryPoint GLCompute %main "main" %output
               OpExecutionMode %main LocalSize 1 1 1
               OpSource HLSL 670
               OpName %type_RWByteAddressBuffer "type.RWByteAddressBuffer"
               OpName %output "output"
               OpName %main "main"
               OpName %src_main "src.main"
               OpName %bb_entry "bb.entry"

; CHECK:       OpDecorate %output_1_ DescriptorSet 0
; CHECK:       OpDecorate %output_1_ Binding 1
; CHECK:       OpDecorate %output_0_ DescriptorSet 0
; CHECK:       OpDecorate %output_0_ Binding 0

               OpDecorate %output DescriptorSet 0
               OpDecorate %output Binding 0

               OpDecorate %_runtimearr_uint ArrayStride 4
               OpMemberDecorate %type_RWByteAddressBuffer 0 Offset 0
               OpDecorate %type_RWByteAddressBuffer Block
        %int = OpTypeInt 32 1
      %int_1 = OpConstant %int 1
       %uint = OpTypeInt 32 0
     %uint_0 = OpConstant %uint 0
     %uint_2 = OpConstant %uint 2
    %uint_32 = OpConstant %uint 32
%_runtimearr_uint = OpTypeRuntimeArray %uint
%type_RWByteAddressBuffer = OpTypeStruct %_runtimearr_uint
%_arr_type_RWByteAddressBuffer_uint_2 = OpTypeArray %type_RWByteAddressBuffer %uint_2
%_ptr_StorageBuffer__arr_type_RWByteAddressBuffer_uint_2 = OpTypePointer StorageBuffer %_arr_type_RWByteAddressBuffer_uint_2
       %void = OpTypeVoid
         %23 = OpTypeFunction %void
%_ptr_StorageBuffer_type_RWByteAddressBuffer = OpTypePointer StorageBuffer %type_RWByteAddressBuffer
%_ptr_StorageBuffer_uint = OpTypePointer StorageBuffer %uint

; CHECK: %output_1_ = OpVariable %_ptr_StorageBuffer_type_RWByteAddressBuffer StorageBuffer
; CHECK: %output_0_ = OpVariable %_ptr_StorageBuffer_type_RWByteAddressBuffer StorageBuffer

     %output = OpVariable %_ptr_StorageBuffer__arr_type_RWByteAddressBuffer_uint_2 StorageBuffer

       %main = OpFunction %void None %23
         %26 = OpLabel
         %27 = OpFunctionCall %void %src_main
               OpReturn
               OpFunctionEnd
   %src_main = OpFunction %void None %23
   %bb_entry = OpLabel
         %28 = OpAccessChain %_ptr_StorageBuffer_type_RWByteAddressBuffer %output %int_1
         %29 = OpShiftRightLogical %uint %uint_0 %uint_2
         %30 = OpAccessChain %_ptr_StorageBuffer_uint %28 %uint_0 %29
               OpStore %30 %uint_32
               OpReturn
               OpFunctionEnd
  )";

  SinglePassRunAndMatch<DescriptorScalarReplacement>(
      text, false, /* flatten_composites=*/false, /* flatten_arrays=*/true);
}

TEST_F(DescriptorScalarReplacementTest, ExpandStructButNotArray) {
  const std::string text = R"(; SPIR-V
; Version: 1.6
; Generator: Khronos SPIR-V Tools Assembler; 0
; Bound: 41
; Schema: 0
                                   OpCapability Shader
                                   OpMemoryModel Logical GLSL450
                                   OpEntryPoint Fragment %main "main" %out_var_SV_Target
                                   OpExecutionMode %main OriginUpperLeft
                                   OpSource HLSL 660
                                   OpName %type_2d_image "type.2d.image"
                                   OpName %Textures "Textures"
                                   OpName %type_sampler "type.sampler"
                                   OpName %out_var_SV_Target "out.var.SV_Target"
                                   OpName %main "main"
                                   OpName %type_sampled_image "type.sampled.image"
                                   OpName %TheStruct "TheStruct"
                                   OpMemberName %StructOfResources 0 "Texture"
                                   OpMemberName %StructOfResources 1 "Sampler"
; CHECK:                           OpName %TheStruct_Sampler "TheStruct.Sampler"
; CHECK:                           OpName %TheStruct_Texture "TheStruct.Texture"
                                   OpDecorate %out_var_SV_Target Location 0
                                   OpDecorate %Textures DescriptorSet 0
                                   OpDecorate %Textures Binding 0
                                   OpDecorate %TheStruct DescriptorSet 0
                                   OpDecorate %TheStruct Binding 10
; CHECK:                           OpDecorate %TheStruct_Sampler DescriptorSet 0
; CHECK:                           OpDecorate %TheStruct_Sampler Binding 11
; CHECK:                           OpDecorate %TheStruct_Texture DescriptorSet 0
; CHECK:                           OpDecorate %TheStruct_Texture Binding 10
                            %int = OpTypeInt 32 1
                          %int_0 = OpConstant %int 0
                          %int_1 = OpConstant %int 1
                          %float = OpTypeFloat 32
                        %float_0 = OpConstant %float 0
                        %v2float = OpTypeVector %float 2
                             %13 = OpConstantComposite %v2float %float_0 %float_0
                           %uint = OpTypeInt 32 0
                        %uint_10 = OpConstant %uint 10
                  %type_2d_image = OpTypeImage %float 2D 2 0 0 1 Unknown
     %_arr_type_2d_image_uint_10 = OpTypeArray %type_2d_image %uint_10
%_ptr_UniformConstant__arr_type_2d_image_uint_10 = OpTypePointer UniformConstant %_arr_type_2d_image_uint_10
                   %type_sampler = OpTypeSampler
              %StructOfResources = OpTypeStruct %type_2d_image %type_sampler
%_ptr_UniformConstant__struct_18 = OpTypePointer UniformConstant %StructOfResources
                        %v4float = OpTypeVector %float 4
            %_ptr_Output_v4float = OpTypePointer Output %v4float
                           %void = OpTypeVoid
                             %23 = OpTypeFunction %void
%_ptr_UniformConstant_type_2d_image = OpTypePointer UniformConstant %type_2d_image
 %_ptr_UniformConstant_type_sampler = OpTypePointer UniformConstant %type_sampler
             %type_sampled_image = OpTypeSampledImage %type_2d_image
                       %Textures = OpVariable %_ptr_UniformConstant__arr_type_2d_image_uint_10 UniformConstant
              %out_var_SV_Target = OpVariable %_ptr_Output_v4float Output
                      %TheStruct = OpVariable %_ptr_UniformConstant__struct_18 UniformConstant
                           %main = OpFunction %void None %23
                             %26 = OpLabel
                             %27 = OpAccessChain %_ptr_UniformConstant_type_2d_image %Textures %int_0
                             %28 = OpLoad %type_2d_image %27
                             %29 = OpAccessChain %_ptr_UniformConstant_type_sampler %TheStruct %int_1
                             %31 = OpLoad %type_sampler %29
; CHECK:                     %31 = OpLoad %type_sampler %TheStruct_Sampler
                             %32 = OpSampledImage %type_sampled_image %28 %31
                             %33 = OpImageSampleImplicitLod %v4float %32 %13 None
                             %34 = OpAccessChain %_ptr_UniformConstant_type_2d_image %TheStruct %int_0
                             %35 = OpLoad %type_2d_image %34
; CHECK:                     %35 = OpLoad %type_2d_image %TheStruct_Texture
                             %36 = OpAccessChain %_ptr_UniformConstant_type_sampler %TheStruct %int_1
                             %37 = OpLoad %type_sampler %36
; CHECK:                     %37 = OpLoad %type_sampler %TheStruct_Sampler
                             %38 = OpSampledImage %type_sampled_image %35 %37
                             %39 = OpImageSampleImplicitLod %v4float %38 %13 None
                             %40 = OpFAdd %v4float %33 %39
                                   OpStore %out_var_SV_Target %40
                                   OpReturn
                                   OpFunctionEnd
  )";

  SinglePassRunAndMatch<DescriptorScalarReplacement>(
      text, false, /* flatten_composites=*/true, /* flatten_arrays=*/false);
}

}  // namespace
}  // namespace opt
}  // namespace spvtools
