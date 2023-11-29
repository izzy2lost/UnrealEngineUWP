// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNEHlslShaderBase.h"
#include "RenderGraphFwd.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterUtils.h"

namespace UE::NNE::Internal { class FTensor; }

namespace UE::NNEHlslShaders::Internal
{
	class FReduceConstants
	{
	public:
		static const int32 NUM_GROUP_THREADS{ 768 };
	};

	class NNEHLSLSHADERS_API TReduceCS : public FHlslShaderBase
	{
		DECLARE_GLOBAL_SHADER(TReduceCS);
		SHADER_USE_PARAMETER_STRUCT(TReduceCS, FHlslShaderBase)

	public:

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(int32, NumElemBeforeAxis)
			SHADER_PARAMETER(int32, AxisSize)
			SHADER_PARAMETER(int32, NumElemAfterAxis)
			SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, Input)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, Output)
		END_SHADER_PARAMETER_STRUCT()

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);
		static void FillInParameters(TConstArrayView<uint32> Shape, int32 Axis, FParameters* Parameters);
		static void EnqueueRDG(FRDGBuilder& GraphBuilder, FParameters* Parameters, FRDGBufferRef Input, FRDGBufferRef Output);
	};
} // UE::NNEHlslShaders::Internal