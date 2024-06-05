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
			PublicAdditionalLibraries.Add(System.IO.Path.Combine(ModuleDirectory, "Lib", "Windows", "flatcc_parsing.lib"));
			PublicAdditionalLibraries.Add(System.IO.Path.Combine(ModuleDirectory, "Lib", "Windows", "ireert.lib"));

			if (Target.WindowsPlatform.Compiler.IsClang())
			{
				PublicSystemIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "Include", "Clang"));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicAdditionalLibraries.Add(System.IO.Path.Combine(ModuleDirectory, "Lib", "Linux", "libflatcc_parsing.a"));
			PublicAdditionalLibraries.Add(System.IO.Path.Combine(ModuleDirectory, "Lib", "Linux", "libireert.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(System.IO.Path.Combine(ModuleDirectory, "Lib", "Mac", "libflatcc_parsing.a"));
			PublicAdditionalLibraries.Add(System.IO.Path.Combine(ModuleDirectory, "Lib", "Mac", "libireert.a"));
		}
	}
}
