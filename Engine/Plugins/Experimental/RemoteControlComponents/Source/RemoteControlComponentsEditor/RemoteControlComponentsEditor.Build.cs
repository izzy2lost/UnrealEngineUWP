// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RemoteControlComponentsEditor : ModuleRules
{
    public RemoteControlComponentsEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "EditorSubsystem",
                "Projects",
                "RemoteControl",
				"RemoteControlComponents",
                "Slate",
                "SlateCore",
                "UnrealEd"
            }
        );
    }
}