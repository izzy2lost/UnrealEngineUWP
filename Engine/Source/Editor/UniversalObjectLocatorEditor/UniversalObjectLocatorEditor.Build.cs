// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UniversalObjectLocatorEditor : ModuleRules
{
	public UniversalObjectLocatorEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		UnsafeTypeCastWarningLevel = WarningLevel.Error;

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"PropertyEditor",
				"SlateCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"EditorWidgets",
				"Engine",
				"PropertyEditor",
				"SceneOutliner",
				"Slate",
				"SlateCore",
				"Sequencer",
				"UniversalObjectLocator",
			}
		);
	}
}
