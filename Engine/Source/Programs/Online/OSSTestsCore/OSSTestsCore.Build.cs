// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OSSTestsCore : ModuleRules
{
	public OSSTestsCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Programs/Online/OSSTestsCore");

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Projects",
				"EngineSettings",
				"OnlineSubsystem",
				"OnlineSubsystemNull",
				"ApplicationCore"
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(Target.RelativeEnginePath, "Plugins", "Online"),
				Path.Combine(Target.RelativeEnginePath, "Restricted", "NotForLicensees", "Plugins", "Online")
            }
        );
	}
}
