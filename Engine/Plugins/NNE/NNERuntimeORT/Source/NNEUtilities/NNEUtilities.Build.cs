// Copyright Epic Games, Inc. All Rights Reserved.


using UnrealBuildTool;
using System.IO;

public class NNEUtilities : ModuleRules
{
	public NNEUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.Add("NNE");

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Projects",
				"NNEProtobufEditor",
				"NNEOnnxruntime",
				"NNEOnnxEditor"
			}
		);
	}
}

