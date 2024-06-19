// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ColorGradingEditor : ModuleRules
{
	public ColorGradingEditor(ReadOnlyTargetRules ROTargetRules) : base(ROTargetRules)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ApplicationCore",
				"AppFramework",
				"ColorCorrectRegions",
				"Core",
				"CoreUObject",
				"DetailCustomizations",
				"EditorStyle",
				"Engine",
				"LevelEditor",
				"InputCore",
				"PropertyEditor",
				"PropertyPath",
				"Slate",
				"SlateCore",
				"ToolWidgets",
				"UnrealEd",
				"WorkspaceMenuStructure",
			});
	}
}
