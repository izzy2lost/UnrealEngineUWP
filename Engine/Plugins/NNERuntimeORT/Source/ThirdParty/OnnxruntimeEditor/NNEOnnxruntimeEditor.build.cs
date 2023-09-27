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
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemIncludePaths.AddRange(
				new string[] {
					Path.Combine(ModuleDirectory, "include/"),
					Path.Combine(ModuleDirectory, "include/onnxruntime"),
					Path.Combine(ModuleDirectory, "include/onnxruntime/core/session")
				}
			);

			string PlatformDir = Target.Platform.ToString();
			string OrtPlatformRelativePath = Path.Combine("Binaries", "ThirdParty", "OnnxruntimeEditor", PlatformDir);
			string OrtPlatformPath = Path.Combine(PluginDirectory, OrtPlatformRelativePath);
			string[] OrtLibNames = new string[] {
				"onnxruntime",
				"onnxruntime_providers_shared"
			};

			foreach (string LibFileName in OrtLibNames)
			{
				string DLLFileName = LibFileName + ".dll";
				string DLLFullPath = Path.Combine(OrtPlatformPath, DLLFileName);

				PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "lib", PlatformDir, LibFileName + ".lib"));
				PublicDelayLoadDLLs.Add(DLLFileName);
				RuntimeDependencies.Add(DLLFullPath);
			}

			// PublicDefinitions
			PublicDefinitions.Add("ONNXRUNTIMEEDITOR_VERSION=1.13.1");

			PublicDefinitions.Add("ONNXRUNTIME_USE_DLLS");
			PublicDefinitions.Add("WITH_ONNXRUNTIME");
			PublicDefinitions.Add("ONNXRUNTIME_PLATFORM_PATH=" + OrtPlatformRelativePath.Replace('\\', '/'));
			PublicDefinitions.Add("ORT_API_MANUAL_INIT");
		}
	}
}
