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
#include "DXCWrapper.h"
#include "ShaderConductorContext.h"
#include "RHIShaderFormatDefinitions.inl"

extern bool PreprocessVulkanShader(
	const struct  FShaderCompilerInput& Input,
	const struct FShaderCompilerEnvironment& Environment,
	class FShaderPreprocessOutput& PreprocessOutput);

extern void CompileVulkanShader(
	const struct FShaderCompilerInput& Input,
	const class FShaderPreprocessOutput& PreprocessOutput,
	struct FShaderCompilerOutput& Output,
	const class FString& WorkingDirectory);

extern void OutputVulkanDebugData(
	const FShaderCompilerInput& Input, 
	const FShaderPreprocessOutput& PreprocessOutput, 
	const FShaderCompilerOutput& Output);

class FShaderFormatVulkan : public IShaderFormat
{
	enum 
	{
		UE_SHADER_VULKAN_ES3_1_VER	= 37,
		UE_SHADER_VULKAN_SM5_VER 	= 37,
		UE_SHADER_VULKAN_SM6_VER	= 37,
	};

	int32 InternalGetVersion(FName Format) const
	{
		if (Format == NAME_VULKAN_SM6)
		{
			return UE_SHADER_VULKAN_SM6_VER;
		}
		else if (Format == NAME_VULKAN_SM5 || Format == NAME_VULKAN_SM5_ANDROID)
		{
			return UE_SHADER_VULKAN_SM5_VER;
		}
		else if (Format == NAME_VULKAN_ES3_1_ANDROID || Format == NAME_VULKAN_ES3_1)
		{
			return UE_SHADER_VULKAN_ES3_1_VER;
		}

		FString FormatStr = Format.ToString();
		checkf(0, TEXT("Invalid shader format passed to Vulkan shader compiler: %s"), *FormatStr);
		return -1;
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

	virtual bool SupportsIndependentPreprocessing() const override
	{
		return true;
	}

	virtual bool PreprocessShader(const FShaderCompilerInput& Input, const FShaderCompilerEnvironment& Environment, FShaderPreprocessOutput& PreprocessOutput) const
	{
		check(InternalGetVersion(Input.ShaderFormat) >= 0);

		return PreprocessVulkanShader(Input, Environment, PreprocessOutput);
	}

	virtual void CompilePreprocessedShader(const FShaderCompilerInput& Input, const FShaderPreprocessOutput& PreprocessOutput, FShaderCompilerOutput& Output,const FString& WorkingDirectory) const override
	{
		check(InternalGetVersion(Input.ShaderFormat) >= 0);
		CompileVulkanShader(Input, PreprocessOutput, Output, WorkingDirectory);
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
