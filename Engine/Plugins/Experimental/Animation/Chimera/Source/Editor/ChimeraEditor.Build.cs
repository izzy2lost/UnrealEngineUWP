// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ChimeraEditor : ModuleRules
	{
		public ChimeraEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
			new string[]
			{
                "AnimGraph",
				"AnimGraphRuntime",
				"BlendStack",
				"Core",
                "CoreUObject",
				"Chimera",
                "Engine",
				"PoseSearchEditor"
			});

            PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AdvancedPreviewScene",
				"AssetDefinition",
				"BlendStackEditor",
				"BlueprintGraph",
				"EditorFramework",
				"PoseSearch",
				"Slate",
				"SlateCore",
				"ToolWidgets",						
				"UnrealEd",
			});
        }
	}
}
