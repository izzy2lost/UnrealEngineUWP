// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class RapidJSON : ModuleRules
{
	public RapidJSON(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string LibRapidJSONPath = Target.UEThirdPartySourceDirectory + "RapidJSON/1.1.0/";
		PublicSystemIncludePaths.Add(Path.Combine(LibRapidJSONPath, "include"));
	}
}
