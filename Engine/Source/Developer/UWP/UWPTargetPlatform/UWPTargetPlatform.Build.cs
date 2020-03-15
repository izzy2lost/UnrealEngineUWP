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
        bEnableWinRTComponentExtensions = true;
        bEnableExceptions = true;
        PCHUsage = PCHUsageMode.NoSharedPCHs;
        PrivatePCHHeaderFile = "Private/UWPTargetPlatformPCH.h";

        // compile withEngine
        if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
			PrivateIncludePathModuleNames.Add("TextureCompressor");
		}

		PublicAdditionalLibraries.Add("shlwapi.lib");
	}
}
