// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderFormatD3D.h"
#include "ShaderCompilerCommon.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/IShaderFormatModule.h"
#include "DXCWrapper.h"

static FName NAME_PCD3D_SM6(TEXT("PCD3D_SM6"));
static FName NAME_PCD3D_SM5(TEXT("PCD3D_SM5"));
static FName NAME_PCD3D_ES3_1(TEXT("PCD3D_ES31"));

static const FGuid UE_SHADER_PCD3D_SHARED_VER = FGuid("dd4e6e76-4b48-4097-9ece-0f21118b7177");
static const FGuid UE_SHADER_PCD3D_SM6_VER    = FGuid("96a53626-2b11-4663-8b85-01e30625b76e");
static const FGuid UE_SHADER_PCD3D_SM5_VER    = FGuid("07cd0fa4-6d18-43e0-915d-e6c125effafd");
static const FGuid UE_SHADER_PCD3D_ES3_1_VER  = FGuid("75466d2b-e169-40d8-bac5-1e2f9d43e0bb");

class FShaderFormatD3D : public UE::ShaderCompilerCommon::FBaseShaderFormat 
{
	uint32 DxcVersionHash = 0;

public:

	FShaderFormatD3D(uint32 InDxcVersionHash)
		: DxcVersionHash(InDxcVersionHash)
	{
	}

	inline uint32 GetVersionHash(const FGuid& InVersion) const
	{
		const uint32 BaseHash = GetTypeHash(UE_SHADER_PCD3D_SHARED_VER);
		uint32 VersionHash = GetTypeHash(InVersion);

	#if UE_D3D_SHADER_COMPILER_ALLOW_DEAD_CODE_REMOVAL
		VersionHash = HashCombine(VersionHash, 0x75E2FE85);
	#endif // UE_D3D_SHADER_COMPILER_ALLOW_DEAD_CODE_REMOVAL

		return HashCombine(BaseHash, VersionHash);
	}

	virtual uint32 GetVersion(FName Format) const override
	{
		if (Format == NAME_PCD3D_SM6)
		{
			uint32 ShaderModelHash = GetVersionHash(UE_SHADER_PCD3D_SM6_VER);

			// Make sure we recompile if EShaderCodeFeatures gets bigger
			ShaderModelHash = HashCombine(ShaderModelHash, GetTypeHash(sizeof(EShaderCodeFeatures)));

			return HashCombine(DxcVersionHash, ShaderModelHash);
		}
		else if (Format == NAME_PCD3D_SM5)
		{
			uint32 ShaderModelHash = GetVersionHash(UE_SHADER_PCD3D_SM6_VER);

			// Make sure we recompile if EShaderCodeFeatures gets bigger
			ShaderModelHash = HashCombine(ShaderModelHash, GetTypeHash(sizeof(EShaderCodeFeatures)));

			// Technically not needed for regular SM5 compiled with legacy compiler,
			// but PCD3D_SM5 currently includes ray tracing shaders that are compiled with new compiler stack.
			return HashCombine(DxcVersionHash, ShaderModelHash);
		}
		else if (Format == NAME_PCD3D_ES3_1) 
		{
			// Shader DXC signature is intentionally not included, as ES3_1 target always uses legacy compiler.
			return GetVersionHash(UE_SHADER_PCD3D_ES3_1_VER);
		}
		checkf(0, TEXT("Unknown Format %s"), *Format.ToString());
		return 0;
	}

	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const
	{
		OutFormats.Add(NAME_PCD3D_SM6);
		OutFormats.Add(NAME_PCD3D_SM5);
		OutFormats.Add(NAME_PCD3D_ES3_1);
	}

	ELanguage FormatToLanguage(FName Format) const
	{
		if (Format == NAME_PCD3D_SM6)
		{
			return ELanguage::SM6;
		}
		else if (Format == NAME_PCD3D_SM5)
		{
			return ELanguage::SM5;
		}
		else if (Format == NAME_PCD3D_ES3_1)
		{
			return ELanguage::ES3_1;
		}
		else
		{
			checkf(0, TEXT("Unknown format %s"), *Format.ToString());
			return ELanguage::Invalid;
		}
	}

	virtual bool PreprocessShader(const struct FShaderCompilerInput& Input, const struct FShaderCompilerEnvironment& MergedEnvironment, class FShaderPreprocessOutput& PreprocessOutput) const
	{
		return PreprocessD3DShader(Input, MergedEnvironment, PreprocessOutput, FormatToLanguage(Input.ShaderFormat));
	}

	virtual void CompilePreprocessedShader(
		const struct FShaderCompilerInput& Input, 
		const class FShaderPreprocessOutput& PreprocessOutput, 
		struct FShaderCompilerOutput& Output,
		const FString& WorkingDirectory) const
	{
		CompileD3DShader(Input, PreprocessOutput.GetSource(), Output, WorkingDirectory, FormatToLanguage(Input.ShaderFormat));

		Output.ShaderDiagnosticDatas = PreprocessOutput.GetDiagnosticDatas();
	}

	virtual bool SupportsIndependentPreprocessing() const
	{
		return true;
	}

	void AddShaderTargetDefines(FShaderCompilerInput& Input, uint32 ShaderTargetMajor, uint32 ShaderTargetMinor) const
	{
		// Inserting our own versions of these defines since we preprocess our shader source before we actually use something that defines them.
		Input.Environment.SetDefine(TEXT("__SHADER_TARGET_MAJOR"), ShaderTargetMajor);
		Input.Environment.SetDefine(TEXT("__SHADER_TARGET_MINOR"), ShaderTargetMinor);
	}

	void ModifyShaderCompilerInput(FShaderCompilerInput& Input) const final
	{
		const ELanguage Language = FormatToLanguage(Input.ShaderFormat);

		if (IsUsingSM66(Input, Language))
		{
			Input.Environment.SetDefine(TEXT("SM6_PROFILE"), 1);
			Input.Environment.SetDefine(TEXT("COMPILER_DXC"), 1);
			Input.Environment.SetDefine(TEXT("PLATFORM_SUPPORTS_CONSTANTBUFFER_OBJECT"), 1);

			AddShaderTargetDefines(Input, 6, 6);
		}
		else if (Language == ELanguage::SM5)
		{
			Input.Environment.SetDefine(TEXT("SM5_PROFILE"), 1);
			const bool bUseDXC =
				Input.Environment.CompilerFlags.Contains(CFLAG_WaveOperations)
				|| Input.Environment.CompilerFlags.Contains(CFLAG_ForceDXC);
			Input.Environment.SetDefine(TEXT("COMPILER_DXC"), bUseDXC);
			Input.Environment.SetDefine(TEXT("PLATFORM_SUPPORTS_CONSTANTBUFFER_OBJECT"), bUseDXC);

			if (bUseDXC)
			{
				AddShaderTargetDefines(Input, 6, 0);
			}
			else
			{
				AddShaderTargetDefines(Input, 5, 0);
			}
		}
		else if (Language == ELanguage::ES3_1)
		{
			Input.Environment.SetDefine(TEXT("ES3_1_PROFILE"), 1);
			Input.Environment.SetDefine(TEXT("COMPILER_DXC"), 0);
			Input.Environment.SetDefine(TEXT("__SHADER_TARGET_MAJOR"), 5);
			Input.Environment.SetDefine(TEXT("__SHADER_TARGET_MINOR"), 0);
		}
		else
		{
			checkf(0, TEXT("Unknown format %s"), *Input.ShaderFormat.ToString());
		}
	}

	virtual const TCHAR* GetPlatformIncludeDirectory() const
	{
		return TEXT("D3D");
	}
};


/**
 * Module for D3D shaders
 */

static IShaderFormat* Singleton = nullptr;

class FShaderFormatD3DModule : public IShaderFormatModule, public FDxcModuleWrapper
{
public:
	virtual ~FShaderFormatD3DModule()
	{
		delete Singleton;
		Singleton = nullptr;
	}

	virtual IShaderFormat* GetShaderFormat()
	{
		if (!Singleton)
		{
			Singleton = new FShaderFormatD3D(FDxcModuleWrapper::GetModuleVersionHash());
		}
		return Singleton;
	}
};

IMPLEMENT_MODULE( FShaderFormatD3DModule, ShaderFormatD3D);
