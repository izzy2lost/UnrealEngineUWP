// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Chimera : ModuleRules
{
	public Chimera(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AnimationCore",
                "AnimGraphRuntime",
                "Core",
                "CoreUObject",
                "Engine",
				"PoseSearch",
            });

			PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"BlendStack"
			});
	}
}
