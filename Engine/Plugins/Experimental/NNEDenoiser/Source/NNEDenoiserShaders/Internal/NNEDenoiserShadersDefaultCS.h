// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterUtils.h"

namespace UE::NNEDenoiserShaders::Internal
{

	enum class ENNEDenoiserDataType : uint8
	{
		None = 0,
		// Char,
		// Boolean,
		Half = 3,
		Float,
		MAX
	};

	class FNNEDenoiserConstants
	{
	public:
		static constexpr int32 THREAD_GROUP_SIZE{ 32 };
		static constexpr int32 MAX_NUM_MAPPED_CHANNELS{ 4 };
	};
	
	class NNEDENOISERSHADERS_API FNNEDenoiserTextureBufferMappedCopyCS : public FGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FNNEDenoiserTextureBufferMappedCopyCS);
		SHADER_USE_PARAMETER_STRUCT(FNNEDenoiserTextureBufferMappedCopyCS, FGlobalShader)

		class FNNEDenoiserDataType : SHADER_PERMUTATION_ENUM_CLASS("BUFFER_TYPE_INDEX", ENNEDenoiserDataType);
		class FNNEDenoiserNumMappedChannels : SHADER_PERMUTATION_RANGE_INT("NUM_MAPPED_CHANNELS", 0, FNNEDenoiserConstants::MAX_NUM_MAPPED_CHANNELS + 1);
		using FPermutationDomain = TShaderPermutationDomain<FNNEDenoiserDataType, FNNEDenoiserNumMappedChannels>;

	public:

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(int32, Width)
			SHADER_PARAMETER(int32, Height)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, OutputBuffer)
			SHADER_PARAMETER_ARRAY(FIntVector4, OutputChannel_InputChannel_Unused_Unused, [FNNEDenoiserConstants::MAX_NUM_MAPPED_CHANNELS])
		END_SHADER_PARAMETER_STRUCT()

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);
	};

	class NNEDENOISERSHADERS_API FNNEDenoiserBufferMappedCopyCS : public FGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FNNEDenoiserBufferMappedCopyCS);
		SHADER_USE_PARAMETER_STRUCT(FNNEDenoiserBufferMappedCopyCS, FGlobalShader)

		class FNNEDenoiserDataType : SHADER_PERMUTATION_ENUM_CLASS("BUFFER_TYPE_INDEX", ENNEDenoiserDataType);
		class FNNEDenoiserNumMappedChannels : SHADER_PERMUTATION_RANGE_INT("NUM_MAPPED_CHANNELS", 0, FNNEDenoiserConstants::MAX_NUM_MAPPED_CHANNELS + 1);
		using FPermutationDomain = TShaderPermutationDomain<FNNEDenoiserDataType, FNNEDenoiserNumMappedChannels>;

	public:

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(int32, Width)
			SHADER_PARAMETER(int32, Height)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, InputBuffer)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, OutputBuffer)
			SHADER_PARAMETER_ARRAY(FIntVector4, OutputChannel_InputChannel_Unused_Unused, [FNNEDenoiserConstants::MAX_NUM_MAPPED_CHANNELS])
		END_SHADER_PARAMETER_STRUCT()

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);
	};

	class NNEDENOISERSHADERS_API FNNEDenoiserBufferTextureMappedCopyCS : public FGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FNNEDenoiserBufferTextureMappedCopyCS);
		SHADER_USE_PARAMETER_STRUCT(FNNEDenoiserBufferTextureMappedCopyCS, FGlobalShader)

		class FNNEDenoiserDataType : SHADER_PERMUTATION_ENUM_CLASS("BUFFER_TYPE_INDEX", ENNEDenoiserDataType);
		class FNNEDenoiserNumMappedChannels : SHADER_PERMUTATION_RANGE_INT("NUM_MAPPED_CHANNELS", 0, FNNEDenoiserConstants::MAX_NUM_MAPPED_CHANNELS + 1);
		using FPermutationDomain = TShaderPermutationDomain<FNNEDenoiserDataType, FNNEDenoiserNumMappedChannels>;

	public:

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(int32, Width)
			SHADER_PARAMETER(int32, Height)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, InputBuffer)
			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutputTexture)
			SHADER_PARAMETER_ARRAY(FIntVector4, OutputChannel_InputChannel_Unused_Unused, [FNNEDenoiserConstants::MAX_NUM_MAPPED_CHANNELS])
		END_SHADER_PARAMETER_STRUCT()

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);
	};

	class NNEDENOISERSHADERS_API FNNEDenoiserTextureMappedCopyCS : public FGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FNNEDenoiserTextureMappedCopyCS);
		SHADER_USE_PARAMETER_STRUCT(FNNEDenoiserTextureMappedCopyCS, FGlobalShader)

		class FNNEDenoiserDataType : SHADER_PERMUTATION_ENUM_CLASS("BUFFER_TYPE_INDEX", ENNEDenoiserDataType);
		class FNNEDenoiserNumMappedChannels : SHADER_PERMUTATION_RANGE_INT("NUM_MAPPED_CHANNELS", 0, FNNEDenoiserConstants::MAX_NUM_MAPPED_CHANNELS + 1);
		using FPermutationDomain = TShaderPermutationDomain<FNNEDenoiserDataType, FNNEDenoiserNumMappedChannels>;

	public:

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(int32, Width)
			SHADER_PARAMETER(int32, Height)
			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, InputTexture)
			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutputTexture)
			SHADER_PARAMETER_ARRAY(FIntVector4, OutputChannel_InputChannel_Unused_Unused, [FNNEDenoiserConstants::MAX_NUM_MAPPED_CHANNELS])
		END_SHADER_PARAMETER_STRUCT()

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);
	};

} // namespace UE::NNEDenoiser::Private