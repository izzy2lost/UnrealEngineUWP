// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using UnrealBuildTool;

public class NNEOnnxruntimeEditor : ModuleRules
{
    public NNEOnnxruntimeEditor(ReadOnlyTargetRules Target) : base(Target)
    {
		Type = ModuleType.External;

		string PlatformDir = Target.Platform.ToString();
		string IncDirPath = Path.Combine(ModuleDirectory, "include");
		string LibDirPath = Path.Combine(ModuleDirectory, "lib", PlatformDir);
		string OrtPlatformRelativePath = Path.Combine("Binaries", "ThirdParty", "OnnxruntimeEditor", PlatformDir);
		string OrtPlatformPath = Path.Combine(PluginDirectory, OrtPlatformRelativePath);
		string SharedLibName = "onnxruntime";

		PublicIncludePaths.Add(IncDirPath);
		PublicDefinitions.Add("ORT_API_MANUAL_INIT");
		PublicDefinitions.Add("ONNXRUNTIME_PLATFORM_PATH=" + OrtPlatformRelativePath.Replace('\\', '/'));

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string DllFileName = SharedLibName + ".dll";
			string DllFilePath = Path.Combine(OrtPlatformPath, DllFileName);

			PublicAdditionalLibraries.Add(Path.Combine(LibDirPath, SharedLibName + ".lib"));
			PublicDelayLoadDLLs.Add(DllFileName);
			RuntimeDependencies.Add(DllFilePath);
		}
	}
}