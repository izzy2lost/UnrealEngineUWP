// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IREE : ModuleRules
{
	public IREE(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "Include"));

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string Version = "14.34";
			if(Target.WindowsPlatform.ToolchainVerison.StartsWith("14.37"))
			{
				Version = "14.37";
			}
			else if (Target.WindowsPlatform.ToolchainVerison.StartsWith("14.36"))
			{
				Version = "14.36";
			}
			else if (Target.WindowsPlatform.ToolchainVerison.StartsWith("14.35"))
			{
				Version = "14.35";
			}
			PublicAdditionalLibraries.Add(System.IO.Path.Combine(ModuleDirectory, "Lib", "Win64", Version, "flatcc_parsing.lib"));
			PublicAdditionalLibraries.Add(System.IO.Path.Combine(ModuleDirectory, "Lib", "Win64", Version, "ireert.lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicAdditionalLibraries.Add(System.IO.Path.Combine(ModuleDirectory, "Lib", "Linux", "x86_64", "libflatcc_parsing.a"));
			PublicAdditionalLibraries.Add(System.IO.Path.Combine(ModuleDirectory, "Lib", "Linux", "x86_64", "libireert.a"));
		}
	}
}
