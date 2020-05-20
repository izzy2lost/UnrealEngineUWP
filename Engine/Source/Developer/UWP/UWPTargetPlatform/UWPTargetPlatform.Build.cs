// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class UWPTargetPlatform : ModuleRules
{
	// Hack to reference HoloLens module path to use HoloLensBuildLib
	// This replaces the original UWPTargetDevice code which fails to build in 4.23
	private string HoloLensModulePath
	{
		get { return Path.Combine(ModuleDirectory, "../../../../Plugins/Runtime/AR/Microsoft/HoloLensAR/Source/HoloLensTargetPlatform"); }
	}

	private string ThirdPartyPath
	{
		get { return Path.GetFullPath(Path.Combine(HoloLensModulePath, "../ThirdParty")); }
	}

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

		if (Target.WindowsPlatform.bUseWindowsSDK10)
		{
			bEnableWinRTComponentExtensions = true;
			bEnableExceptions = true;
			PCHUsage = PCHUsageMode.NoSharedPCHs;
			PrivatePCHHeaderFile = "Private/UWPTargetPlatformPCH.h";
		}

        // compile withEngine
        if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
			PrivateIncludePathModuleNames.Add("TextureCompressor");
		}

		PublicAdditionalLibraries.Add("shlwapi.lib");

		string LibrariesPath = Path.Combine(ThirdPartyPath, "Lib", Target.WindowsPlatform.GetArchitectureSubpath());

		PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "HoloLensBuildLib.lib"));

		PrivateIncludePaths.Add(Path.Combine(ThirdPartyPath, "Include/"));
	}
}
