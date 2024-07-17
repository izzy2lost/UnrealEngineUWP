// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class uLangLocalization : ModuleRules
{
	public uLangLocalization(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bRequiresImplementModule = false;
		BinariesSubFolder = "NotForLicensees";

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"uLangCore",
				"uLangToolchainDependencies"
			}
		);

		bDisableAutoRTFMInstrumentation = true;
	}
}
