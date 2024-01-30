// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheRemoteControlEditor : ModuleRules
{
    public AvalancheRemoteControlEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
	            "AvalancheEditorCore",
	            "AvalancheOutliner",
                "Core",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "AvalancheRemoteControl",
                "CoreUObject",
                "Engine",
                "RemoteControlUI",
                "RemoteControlComponents",
                "RemoteControlComponentsEditor",
                "Slate",
                "SlateCore",
                "ToolMenus",
            }
        );

        ShortName = "AvRCEd";
    }
}