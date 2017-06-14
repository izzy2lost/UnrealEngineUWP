// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UWPTargetPlatform : ModuleRules
{
	public UWPTargetPlatform(ReadOnlyTargetRules Target) : base(Target)
    {
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Settings",
				"TargetPlatform",
				"DesktopPlatform",
				"UWPDeviceDetector",
				"HTTP",
			}
		);

		PrivateIncludePathModuleNames.Add("Settings");

		// compile withEngine
		if (UEBuildConfiguration.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
			PrivateIncludePathModuleNames.Add("TextureCompressor");
		}

		PublicAdditionalLibraries.Add("shlwapi.lib");
	}
}
