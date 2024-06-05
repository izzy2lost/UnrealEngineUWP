// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HoldoutComposite : ModuleRules
{
	public HoldoutComposite(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(GetModuleDirectory("Renderer"), "Private"),
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"DeveloperSettings",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				"RenderCore",
				"Renderer",
				"RHI",
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"Slate",
				}
			);
		}
	}
}
