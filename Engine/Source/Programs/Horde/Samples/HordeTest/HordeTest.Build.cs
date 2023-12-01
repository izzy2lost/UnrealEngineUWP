// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HordeTest : ModuleRules
{
	public HordeTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Horde"
			}
		);
	}
}
