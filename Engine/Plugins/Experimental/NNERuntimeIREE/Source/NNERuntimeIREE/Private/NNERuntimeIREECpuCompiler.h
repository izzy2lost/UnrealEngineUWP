// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef WITH_NNE_RUNTIME_IREE
#if WITH_EDITOR

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Serialization/JsonSerializerMacros.h"
#include "Templates/UniquePtr.h"

namespace UE::NNERuntimeIREECpu::Private
{
	struct FIREEBuildTarget : public FJsonSerializable
	{
		FString Architecture;
		FString CompilerArguments;
		FString LinkerArguments;

		BEGIN_JSON_SERIALIZER
			JSON_SERIALIZE("Architecture", Architecture);
			JSON_SERIALIZE("CompilerArguments", CompilerArguments);
			JSON_SERIALIZE("LinkerArguments", LinkerArguments);
		END_JSON_SERIALIZER
	};

	struct FIREEBuildConfig : public FJsonSerializable
	{
		TArray<FString> CompilerCommand;
		TArray<FString> LinkerCommand;
		FString SharedLibExt;
		TArray<FIREEBuildTarget> Targets;

		BEGIN_JSON_SERIALIZER
			JSON_SERIALIZE_ARRAY("CompilerCommand", CompilerCommand);
			JSON_SERIALIZE_ARRAY("LinkerCommand", LinkerCommand);
			JSON_SERIALIZE("SharedLibExt", SharedLibExt);
			JSON_SERIALIZE_ARRAY_SERIALIZABLE("Targets", Targets, FIREEBuildTarget);
		END_JSON_SERIALIZER
	};

	struct FIREECompilerResult
	{
		FString Architecture;
		FString RelativeDirPath;
		FString SharedLibraryFileName;
		FString VmfbFileName;
		FString SharedLibraryEntryPointName;
	};

	class FNNERuntimeIREECpuCompiler
	{
	public:
		FNNERuntimeIREECpuCompiler(const FString& InCompilerCommand, const FString& InLinkerCommand, const FString& InSharedLibExt, TConstArrayView<FIREEBuildTarget> InTargets);
		~FNNERuntimeIREECpuCompiler() {};
		static TUniquePtr<FNNERuntimeIREECpuCompiler> Make(const FString& InTargetPlatformName);

	public:
		bool CompileMlir(TConstArrayView<uint8> InFileData, const FString& InModelName, const FString& InIntermediateDir, const FString& InStagingDir, TArray<FIREECompilerResult>& OutCompilerResults);

		// Needs to stay in header to prevent system macros to overwrite GetEnvironmentVariable
		static FString GetEnvVar(const FString& EnvironmentVariableName) { return FPlatformMisc::GetEnvironmentVariable(*EnvironmentVariableName); }

	private:
		FString CompilerCommand;
		FString LinkerCommand;
		FString SharedLibExt;
		TArray<FIREEBuildTarget> Targets;
	};
} // UE::NNERuntimeIREECpu::Private

#endif // WITH_EDITOR
#endif // WITH_NNE_RUNTIME_IREE