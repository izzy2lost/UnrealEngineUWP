// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheMRQEditor : ModuleRules
{
    public AvalancheMRQEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "AvalancheMedia",
                "AvalancheMediaEditor",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Avalanche",
                "AvalancheMRQ",
                "AvalancheSequence",
                "AvalancheSequencer",
                "Engine",
                "MovieRenderPipelineCore",
                "MovieRenderPipelineEditor",
                "MovieRenderPipelineRenderPasses",
                "RemoteControl",
                "Slate",
                "SlateCore",
                "UnrealEd", 
            }
        );
    }
}