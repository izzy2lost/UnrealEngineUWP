// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Draco : ModuleRules
{
	public Draco(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string DracoLibsDir = Path.Combine(ModuleDirectory, "lib");
		string DracoIncDir = Path.Combine(ModuleDirectory, "include");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemIncludePaths.Add(DracoIncDir);
			PublicSystemLibraryPaths.Add(DracoLibsDir);

			foreach (string DracoLib in Directory.EnumerateFiles(DracoLibsDir, "*.lib", SearchOption.AllDirectories))
			{
				PublicAdditionalLibraries.Add(DracoLib);
			}

			PublicDefinitions.Add("USE_DRACO_LIBRARY=1");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicDefinitions.Add("USE_DRACO_LIBRARY=0"); //update to =1 once the support for the platform is added
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicDefinitions.Add("USE_DRACO_LIBRARY=0"); //update to =1 once the support for the platform is added
		}
		else
		{
			PublicDefinitions.Add("USE_DRACO_LIBRARY=0");
		}
	}
}
