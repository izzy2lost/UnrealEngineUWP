// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineSubsystemUtilsTest : ModuleRules
{
	public OnlineSubsystemUtilsTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreOnline",
				"CoreUObject",
				"Engine",
				"NetCore",
				"UnrealEd",
				"OnlineSubsystem",
				"OnlineSubsystemUtils"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Sockets"
			}
		);
	}
}
