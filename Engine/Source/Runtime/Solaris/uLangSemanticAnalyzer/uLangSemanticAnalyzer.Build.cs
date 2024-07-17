// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class uLangSemanticAnalyzer : ModuleRules
{
	public uLangSemanticAnalyzer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bRequiresImplementModule = false;
		BinariesSubFolder = "NotForLicensees";

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"uLangCore",
				"uLangToolchainDependencies",
				"uLangParser"
			}
		);

		bDisableAutoRTFMInstrumentation = true;
	}
}
