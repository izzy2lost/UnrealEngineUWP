// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class nanoflann : ModuleRules
{
	protected readonly string Version = "1.5.5";

	public nanoflann(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string VersionPath = Path.Combine(ModuleDirectory, Version);

		PublicSystemIncludePaths.Add(Path.Combine(VersionPath, "include"));
	}
}
