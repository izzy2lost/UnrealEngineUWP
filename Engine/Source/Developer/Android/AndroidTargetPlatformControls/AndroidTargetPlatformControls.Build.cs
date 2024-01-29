// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AndroidTargetPlatformControls : ModuleRules
{
	public AndroidTargetPlatformControls(ReadOnlyTargetRules Target) : base(Target)
	{
		UnsafeTypeCastWarningLevel = WarningLevel.Error;
		BinariesSubFolder = "Android";

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"TargetPlatform",
				"DesktopPlatform",
				"AndroidDeviceDetection",
				"AudioPlatformConfiguration",
				"AndroidTargetPlatformSettings"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"AndroidTargetPlatformSettings"
			}
			);

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
			PrivateIncludePathModuleNames.Add("TextureCompressor");     //@todo android: AndroidTargetPlatform.Build
		}

		PublicDefinitions.Add("WITH_OGGVORBIS=1");
	}
}
