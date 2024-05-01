// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;


public class SwitchboardListenerTests : TestModuleRules
{
	static SwitchboardListenerTests()
	{
		TestMetadata = new Metadata();
		TestMetadata.TestName = "SwitchboardListener";
		TestMetadata.TestShortName = "SwitchboardListener";
	}

	public SwitchboardListenerTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] {
			"SblCore",

			"JWT",
		});

		PrivateIncludePathModuleNames.Add("LowLevelTestsRunner");
	}
}
