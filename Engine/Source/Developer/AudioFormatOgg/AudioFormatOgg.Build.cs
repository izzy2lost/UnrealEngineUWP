// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioFormatOgg : ModuleRules
{
	public AudioFormatOgg(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine"
			}
		);

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32) ||
			(Target.Platform == UnrealTargetPlatform.HoloLens) ||
// @ATG_CHANGE : BEGIN UWP support
			(Target.Platform == UnrealTargetPlatform.UWP64) ||
			(Target.Platform == UnrealTargetPlatform.UWP32) ||
// @ATG_CHANGE : END
			(Target.Platform == UnrealTargetPlatform.Mac) ||
			Target.IsInPlatformGroup(UnrealPlatformGroup.Linux)
		)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"UEOgg",
				"Vorbis",
				"VorbisFile"
			);
		}
	}
}
