// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PlainProps : ModuleRules
{
	public PlainProps(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
		});
	}
}
