// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LinuxTargetPlatform : ModuleRules
{
    public LinuxTargetPlatform(ReadOnlyTargetRules Target) : base(Target)
	{
        BinariesSubFolder = "Linux";

		PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"TargetPlatform",
				"DesktopPlatform",
			}
        );
	}
}
