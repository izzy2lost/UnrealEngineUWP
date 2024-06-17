// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class StudioTelemetry : ModuleRules
{
	public StudioTelemetry(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"HTTP",
				"Horde"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreUObject",
                "EngineSettings",
                "BuildSettings",
                "Analytics",
                "AnalyticsET",
				"TelemetryUtils",
				"RHI",
			}
		);
	}
}
