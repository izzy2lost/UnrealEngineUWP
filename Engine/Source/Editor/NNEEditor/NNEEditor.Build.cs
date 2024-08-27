// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class NNEEditor : ModuleRules
{
	public NNEEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange
			(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"DeveloperSettings",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"AssetTools",
				"NNE"
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string SharedLibPath = Path.Combine(ModuleDirectory, "Bin", Target.Platform.ToString());
			string SharedLibFileName = "NNEEditorOnnxTools.dll";

			PublicDelayLoadDLLs.Add(SharedLibFileName);

			RuntimeDependencies.Add("$(TargetOutputDir)/"+ SharedLibFileName, Path.Combine(SharedLibPath, SharedLibFileName));

			PublicDefinitions.Add("NNEEDITORONNXTOOLS_SUPPORTED");
			PublicDefinitions.Add("NNEEDITORONNXTOOLS_SHAREDLIB_FILENAME=" + SharedLibFileName);
		}

		PublicDefinitions.Add("UE_NNEEDITORONNXTOOLS");
	}
}
