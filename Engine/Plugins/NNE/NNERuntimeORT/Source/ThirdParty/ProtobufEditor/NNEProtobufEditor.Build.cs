// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class NNEProtobufEditor : ModuleRules
{
	public NNEProtobufEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Type != TargetType.Editor && Target.Type != TargetType.Program)
			return;

		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));

		string LibraryName = "libprotobuf-lite";
		string PlatformDir = Target.Platform.ToString();
		string LibraryPath = Path.Combine(ModuleDirectory, "lib", PlatformDir);
		string LibPlatformExtension = ".lib";//Win64

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			LibPlatformExtension = ".a";
		}

		PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, LibraryName + LibPlatformExtension));
	}
}
