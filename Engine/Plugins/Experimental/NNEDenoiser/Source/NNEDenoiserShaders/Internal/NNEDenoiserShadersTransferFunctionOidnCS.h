// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterUtils.h"

namespace UE::NNEDenoiserShaders::Internal
{

	enum class ETransferFunctionOidnMode : uint8
	{
		Forwward = 0,
		Inverse,
		MAX
	};

	class FTransferFunctionOidnConstants
	{
	public:
		static constexpr int32 THREAD_GROUP_SIZE{16};
	};
	
	class NNEDENOISERSHADERS_API FTransferFunctionOidnCS : public FGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FTransferFunctionOidnCS);
		SHADER_USE_PARAMETER_STRUCT(FTransferFunctionOidnCS, FGlobalShader)

		class FTransferFunctionOidnMode : SHADER_PERMUTATION_ENUM_CLASS("MODE", ETransferFunctionOidnMode);
		using FPermutationDomain = TShaderPermutationDomain<FTransferFunctionOidnMode>;

	public:

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(int32, InputTextureWidth)
			SHADER_PARAMETER(int32, InputTextureHeight)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
			SHADER_PARAMETER(int32, OutputTextureWidth)
			SHADER_PARAMETER(int32, OutputTextureHeight)
			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutputTexture)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, InputScaleBuffer)
			SHADER_PARAMETER(float, NormScale)
			SHADER_PARAMETER(float, InvNormScale)
		END_SHADER_PARAMETER_STRUCT()

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);
	};

} // namespace UE::NNEDenoiser::Private