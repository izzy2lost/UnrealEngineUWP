// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheTest : ModuleRules
{
    public AvalancheTest(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "FunctionalTesting",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
	            "Avalanche",
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
            }
        );
        
        if (Target.Type == TargetType.Editor)
        {
	        PrivateDependencyModuleNames.AddRange(
		        new string[]
		        {
			        "AvalancheEditor",
			        "LevelEditor",
			        "UnrealEd",
		        });
        }
    }
}
