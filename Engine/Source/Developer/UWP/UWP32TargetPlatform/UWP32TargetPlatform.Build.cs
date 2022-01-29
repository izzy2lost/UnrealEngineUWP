// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UWP32TargetPlatform : ModuleRules
{
	public UWP32TargetPlatform(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"TargetPlatform",
				"UWPTargetPlatform",
				"UWPDeviceDetector",
				"DesktopPlatform",
			}
		);

		// compile withEngine
		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
			PrivateIncludePathModuleNames.Add("TextureCompressor");
		}

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/UWP/UWPTargetPlatform/Private"
			}
		);
	}
}
