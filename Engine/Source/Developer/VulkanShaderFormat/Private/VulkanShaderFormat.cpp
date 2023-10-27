// Copyright Epic Games, Inc. All Rights Reserved.

#include "VulkanShaderFormat.h"
#include "VulkanCommon.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/IShaderFormatModule.h"
#include "hlslcc.h"
#include "ShaderCore.h"
#include "ShaderCompilerCore.h"
#include "ShaderPreprocessTypes.h"
#include "DXCWrapper.h"
#include "ShaderConductorContext.h"
#include "RHIShaderFormatDefinitions.inl"

extern bool PreprocessVulkanShader(
	const FShaderCompilerInput& Input,
	const FShaderCompilerEnvironment& Environment,
	FShaderPreprocessOutput& PreprocessOutput);

extern void CompileVulkanShader(
	const FShaderCompilerInput& Input,
	const FString& InPreprocessedSource,
	FShaderCompilerOutput& Output,
	const FString& WorkingDirectory);

extern void OutputVulkanDebugData(
	const FShaderCompilerInput& Input, 
	const FShaderPreprocessOutput& PreprocessOutput, 
	const FShaderCompilerOutput& Output);

static const FGuid UE_SHADER_VULKAN_ES3_1_VER = FGuid("6D333063-D2F7-4AA5-A79D-BA73F32C898E");
static const FGuid UE_SHADER_VULKAN_SM5_VER = FGuid("6EC81E81-BDE8-4F09-8A70-AD09A817B32A");
static const FGuid UE_SHADER_VULKAN_SM6_VER = FGuid("C732FBB7-4CAD-4249-A760-912777047233");

class FShaderFormatVulkan : public IShaderFormat
{
	FGuid InternalGetVersion(FName Format) const
	{
		if (Format == NAME_VULKAN_SM6)
		{
			return UE_SHADER_VULKAN_SM6_VER;
		}

		if (Format == NAME_VULKAN_SM5 || Format == NAME_VULKAN_SM5_ANDROID)
		{
			return UE_SHADER_VULKAN_SM5_VER;
		}

		if (Format == NAME_VULKAN_ES3_1_ANDROID || Format == NAME_VULKAN_ES3_1)
		{
			return UE_SHADER_VULKAN_ES3_1_VER;
		}

		FString FormatStr = Format.ToString();
		checkf(0, TEXT("Invalid shader format passed to Vulkan shader compiler: %s"), *FormatStr);
		return FGuid();
	}

	uint32 ShaderConductorVersionHash;

public:

	FShaderFormatVulkan(uint32 InShaderConductorVersionHash)
		: ShaderConductorVersionHash(InShaderConductorVersionHash)
	{
	}

	virtual uint32 GetVersion(FName Format) const override
	{
		uint32 Version = HashCombine(GetTypeHash(HLSLCC_VersionMajor), GetTypeHash(HLSLCC_VersionMinor));
		Version = HashCombine(Version, GetTypeHash(InternalGetVersion(Format)));
		Version = HashCombine(Version, GetTypeHash(ShaderConductorVersionHash));

	#if VULKAN_ENABLE_BINDING_DEBUG_NAMES
		Version = HashCombine(Version, 0xFC0848E2);
	#endif

	#if UE_VULKAN_SHADER_COMPILER_ALLOW_DEAD_CODE_REMOVAL
		Version = HashCombine(Version, 0x75E2FE85);
	#endif // UE_VULKAN_SHADER_COMPILER_ALLOW_DEAD_CODE_REMOVAL

		return Version;
	}
	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const
	{
		OutFormats.Add(NAME_VULKAN_SM5);
		OutFormats.Add(NAME_VULKAN_ES3_1_ANDROID);
		OutFormats.Add(NAME_VULKAN_ES3_1);
		OutFormats.Add(NAME_VULKAN_SM5_ANDROID);
		OutFormats.Add(NAME_VULKAN_SM6);
	}

	virtual bool PreprocessShader(const FShaderCompilerInput& Input, const FShaderCompilerEnvironment& Environment, FShaderPreprocessOutput& PreprocessOutput) const
	{
		return PreprocessVulkanShader(Input, Environment, PreprocessOutput);
	}

	virtual void CompilePreprocessedShader(const FShaderCompilerInput& Input, const FShaderPreprocessOutput& PreprocessOutput, FShaderCompilerOutput& Output,const FString& WorkingDirectory) const override
	{
		CompileVulkanShader(Input, PreprocessOutput.GetSource(), Output, WorkingDirectory);
	}

	virtual void OutputDebugData(const FShaderCompilerInput& Input, const FShaderPreprocessOutput& PreprocessOutput, const FShaderCompilerOutput& Output) const override
	{
		OutputVulkanDebugData(Input, PreprocessOutput, Output);
	}

	virtual const TCHAR* GetPlatformIncludeDirectory() const
	{
		return TEXT("Vulkan");
	}
};

/**
 * Module for Vulkan shaders
 */

static IShaderFormat* Singleton = nullptr;

class FVulkanShaderFormatModule : public IShaderFormatModule, public FShaderConductorModuleWrapper
{
public:
	virtual ~FVulkanShaderFormatModule()
	{
		delete Singleton;
		Singleton = nullptr;
	}

	virtual IShaderFormat* GetShaderFormat()
	{
		if (!Singleton)
		{
			Singleton = new FShaderFormatVulkan(FShaderConductorModuleWrapper::GetModuleVersionHash());
		}

		return Singleton;
	}

	virtual void ShutdownModule() override
	{
		CrossCompiler::FShaderConductorContext::Shutdown();
	}
};

IMPLEMENT_MODULE( FVulkanShaderFormatModule, VulkanShaderFormat);
