// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RemoteWorkerCpp : ModuleRules
{
	public RemoteWorkerCpp(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Horde"
			}
		);
	}
}
