// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Xml.Linq;
using UnrealBuildTool;

public class NNEOnnxEditor : ModuleRules
{
    public NNEOnnxEditor(ReadOnlyTargetRules Target) : base(Target)
    {
		Type = ModuleType.External;

		string IncPath = Path.Combine(ModuleDirectory, "include");
		PublicSystemIncludePaths.Add(IncPath);

		string PlatformDir = Target.Platform.ToString();
		string LibDirPath = Path.Combine(ModuleDirectory, "lib", PlatformDir);
		string[] LibFileNames = new string[] {
			"onnx",
			"onnx_proto"
		};

		foreach(string LibFileName in LibFileNames)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibDirPath, LibFileName + ".lib"));
		}

		PublicDefinitions.Add("ONNX_ML");
		PublicDefinitions.Add("ONNX_NAMESPACE = onnx");
		PublicDefinitions.Add("__ONNX_NO_DOC_STRINGS");
	}
}
