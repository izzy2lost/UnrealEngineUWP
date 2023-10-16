// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class OodleWAV : ModuleRules
{
	public OodleWAV(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bool bWithLib = false;
		if (Target.Platform == UnrealTargetPlatform.Win64 && Target.WindowsPlatform.Architecture != UnrealArch.Arm64)
		{
			string LibPath = Path.Combine(ModuleDirectory, "Lib");
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "oowav_w64.lib"));
			bWithLib = true;
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibPath = Path.Combine(ModuleDirectory, "Lib");
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "oowav_mac.a"));
			bWithLib = true;
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string LibPath = Path.Combine(ModuleDirectory, "Lib/Unix", Target.Architecture.LinuxName);
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "oowav_linux.a"));
			bWithLib = true;
		}

		if (bWithLib)
		{
			PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "Source"));
		}

		PublicDefinitions.Add("WITH_OOWAV=" + (bWithLib ? '1' : '0'));
	}
}
