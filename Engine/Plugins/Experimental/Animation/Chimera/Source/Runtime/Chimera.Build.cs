// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Chimera : ModuleRules
{
	public Chimera(ReadOnlyTargetRules Target) : base(Target)
	{
		//OptimizeCode = CodeOptimization.Never;
		//bUseUnity = false;
		//PCHUsage = PCHUsageMode.NoPCHs;

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
