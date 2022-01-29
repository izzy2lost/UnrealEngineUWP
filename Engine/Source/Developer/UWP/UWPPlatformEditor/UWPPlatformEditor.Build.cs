// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UWPPlatformEditor : ModuleRules
{
	public UWPPlatformEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Settings",
				"TargetPlatform",
				"DesktopPlatform",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"PropertyEditor",
				"SharedSettingsWidgets",
				"AppFramework",
				"DesktopWidgets",
				"UnrealEd",
				"SourceControl",
				"WindowsTargetPlatform", // For ECompilerVersion
				"EngineSettings",
				"Projects",
			}
		);

		PublicSystemLibraries.Add("crypt32.lib");
	}
}
