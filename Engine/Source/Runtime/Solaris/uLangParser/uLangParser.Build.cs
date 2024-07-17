// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class uLangParser : ModuleRules
{
	public uLangParser(ReadOnlyTargetRules Target) : base(Target)
	{
		IWYUSupport = IWYUSupport.None;

		PCHUsage = PCHUsageMode.NoPCHs;
		bRequiresImplementModule = false;
		BinariesSubFolder = "NotForLicensees";

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"uLangCore",
				"uLangToolchainDependencies",
			}
		);

		bDisableAutoRTFMInstrumentation = true;
	}
}
