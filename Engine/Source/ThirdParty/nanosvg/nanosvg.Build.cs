// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Nanosvg : ModuleRules
{
	public Nanosvg(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDefinitions.Add("NSVG_USE_BGRA=1");

		Type = ModuleType.CPlusPlus;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
		);
	}
}
