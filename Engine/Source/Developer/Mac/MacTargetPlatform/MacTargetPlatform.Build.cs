// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MacTargetPlatform : ModuleRules
{
	public MacTargetPlatform(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"TargetPlatform",
				"DesktopPlatform",
				"MacTargetPlatformSettings",
				"MacTargetPlatformControls",
			}
		);
	}
}
