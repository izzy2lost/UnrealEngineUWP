// Copyright Epic Games, Inc. All Rights Reserved.


using UnrealBuildTool;
using System.IO;

public class NNEUtils : ModuleRules
{
	public NNEUtils(ReadOnlyTargetRules Target) : base(Target)
	{
		ShortName = "NNEUtils"; // Shorten to avoid path-too-long errors
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"NNE"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"NNEOnnxruntime",
				"NNEOnnx",
				"ORTHelper",
				}
			);


		if (Target.Platform == UnrealTargetPlatform.Win64 || 
			Target.Platform == UnrealTargetPlatform.Linux ||
			Target.Platform == UnrealTargetPlatform.Mac)
		{
			PrivateDependencyModuleNames.AddRange
				(
				new string[] {
					"NNEProtobuf",
					"Re2" // ONNXRuntimeRE2
				}
			);
		}
	}
}

