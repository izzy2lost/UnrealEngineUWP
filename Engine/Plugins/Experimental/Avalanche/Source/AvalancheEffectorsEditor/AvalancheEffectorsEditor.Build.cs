// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheEffectorsEditor : ModuleRules
{
	public AvalancheEffectorsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AvalancheComponentVisualizers",
				"AvalancheCore",
				"AvalancheEffectors",
				"AvalancheInteractiveTools",
				"AvalancheShapes",
				"AvalancheShapesEditor",
				"ComponentVisualizers",
				"InputCore",
				"InteractiveToolsFramework",
				"Projects",
				"UnrealEd"
			}
		);
	}
}
