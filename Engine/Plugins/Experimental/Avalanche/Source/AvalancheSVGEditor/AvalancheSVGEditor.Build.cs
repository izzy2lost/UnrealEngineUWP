// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheSVGEditor : ModuleRules
{
    public AvalancheSVGEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
	            "ActorModifierCore",
	            "AvalancheInteractiveTools",
	            "AvalancheModifiers",
	            "CoreUObject",
                "Engine",
                "InteractiveToolsFramework",
                "Slate",
                "SlateCore",
                "SVGImporter",
                "SVGImporterEditor",
                "ToolMenus",
            }
        );
    }
}
