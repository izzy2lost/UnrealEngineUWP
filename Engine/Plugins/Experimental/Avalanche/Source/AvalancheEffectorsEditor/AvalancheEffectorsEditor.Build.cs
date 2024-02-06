// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheEffectorsEditor : ModuleRules
{
	public AvalancheEffectorsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AvalancheComponentVisualizers",
				"AvalancheCore",
				"AvalancheEffectors",
				"AvalancheInteractiveTools",
				"AvalancheShapes",
				"AvalancheShapesEditor",
				"ClonerEffector",
				"ComponentVisualizers",
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"InteractiveToolsFramework",
				"Projects",
				"Slate",
				"SlateCore",
				"UnrealEd",
			}
		);
	}
}
