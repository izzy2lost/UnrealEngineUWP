// Copyright Epic Games, Inc. All Rights Reserved.
//

#include "ShaderFormatOpenGL.h"

#include "ShaderCompilerCommon.h"
#include "ShaderPreprocessTypes.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/IShaderFormatModule.h"
#include "ShaderCore.h"
#include "ShaderCompilerCore.h"

static FName NAME_GLSL_150_ES3_1(TEXT("GLSL_150_ES31"));
static FName NAME_GLSL_ES3_1_ANDROID(TEXT("GLSL_ES3_1_ANDROID"));

extern bool PreprocessOpenGLShader(
	const FShaderCompilerInput& Input,
	const FShaderCompilerEnvironment& Environment,
	FShaderPreprocessOutput& Output,
	GLSLVersion Version);

extern void CompileOpenGLShader(
	const FShaderCompilerInput& Input,
	const FString& InPreprocessedSource,
	FShaderCompilerOutput& Output,
	const FString& WorkingDirectory,
	GLSLVersion Version);

class FShaderFormatGLSL : public UE::ShaderCompilerCommon::FBaseShaderFormat 
{
	enum
	{
		/** Version for shader format, this becomes part of the DDC key. */
		UE_SHADER_GLSL_VER = 107,
	};

	static void CheckFormat(FName Format)
	{
		check(Format == NAME_GLSL_150_ES3_1 || Format == NAME_GLSL_ES3_1_ANDROID);
	}

public:
	virtual uint32 GetVersion(FName Format) const override
	{
		CheckFormat(Format);
		uint32 GLSLVersion = 0;
		if (Format == NAME_GLSL_150_ES3_1 || Format == NAME_GLSL_ES3_1_ANDROID)
		{
			GLSLVersion = UE_SHADER_GLSL_VER;
		}
		else
		{
			check(0);
		}

		uint32 Version = ((HLSLCC_VersionMinor & 0xff) << 8) | (GLSLVersion & 0xff);

	#if UE_OPENGL_SHADER_COMPILER_ALLOW_DEAD_CODE_REMOVAL
		Version = HashCombine(Version, 0x75E2FE85);
	#endif // UE_OPENGL_SHADER_COMPILER_ALLOW_DEAD_CODE_REMOVAL

		return Version;
	}
	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const override
	{
		OutFormats.Add(NAME_GLSL_150_ES3_1);
		OutFormats.Add(NAME_GLSL_ES3_1_ANDROID);
	}

	static GLSLVersion TranslateFormatNameToEnum(FName Format)
	{
		if (Format == NAME_GLSL_150_ES3_1)
		{
			return GLSL_150_ES3_1;
		}
		else if (Format == NAME_GLSL_ES3_1_ANDROID)
		{
			return GLSL_ES3_1_ANDROID;
		}
		else
		{
			check(0);
			return GLSL_MAX;
		}
	}

	virtual bool PreprocessShader(const FShaderCompilerInput& Input, const FShaderCompilerEnvironment& Environment, FShaderPreprocessOutput& PreprocessOutput) const override
	{
		CheckFormat(Input.ShaderFormat);
		return PreprocessOpenGLShader(Input, Environment, PreprocessOutput, TranslateFormatNameToEnum(Input.ShaderFormat));
	}

	virtual void CompilePreprocessedShader(const FShaderCompilerInput& Input, const FShaderPreprocessOutput& PreprocessOutput, FShaderCompilerOutput& Output, const FString& WorkingDirectory) const override
	{
		CheckFormat(Input.ShaderFormat);
		CompileOpenGLShader(Input, PreprocessOutput.GetSource(), Output, WorkingDirectory, TranslateFormatNameToEnum(Input.ShaderFormat));		
	}

	virtual const TCHAR* GetPlatformIncludeDirectory() const override
	{
		return TEXT("GL");
	}
};

/**
 * Module for OpenGL shaders
 */

static IShaderFormat* Singleton = nullptr;

class FShaderFormatOpenGLModule : public IShaderFormatModule
{
public:
	virtual ~FShaderFormatOpenGLModule() override
	{
		delete Singleton;
		Singleton = nullptr;
	}
	virtual IShaderFormat* GetShaderFormat() override
	{
		if (!Singleton)
		{
			Singleton = new FShaderFormatGLSL();
		}
		return Singleton;
	}
};

IMPLEMENT_MODULE( FShaderFormatOpenGLModule, ShaderFormatOpenGL);
