// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class XAudio2 : ModuleRules
{
	public XAudio2(TargetInfo Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
			}
			);

		// @ATG_CHANGE : BEGIN UWP support
		if (Target.Platform == UnrealTargetPlatform.Win64 ||
            Target.Platform == UnrealTargetPlatform.Win32 ||
            Target.Platform == UnrealTargetPlatform.UWP32 ||
            Target.Platform == UnrealTargetPlatform.UWP64)
		// @ATG_CHANGE : END
		{
			// VS2015 updated some of the CRT definitions but not all of the Windows SDK has been updated to match.
			// Microsoft provides this shim library to enable building with VS2015 until they fix everything up.
			//@todo: remove when no longer neeeded (no other code changes should be necessary).
			if (WindowsPlatform.bNeedsLegacyStdioDefinitionsLib)
			{
				PublicAdditionalLibraries.Add("legacy_stdio_definitions.lib");
			}
		}

		AddEngineThirdPartyPrivateStaticDependencies(Target, 
			"DX11Audio",
			"UEOgg",
			"Vorbis",
			"VorbisFile"
			);
	}
}
