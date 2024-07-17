// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO; // for Path

public class uLangToolchain : ModuleRules
{
	public uLangToolchain(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bRequiresImplementModule = false;
		BinariesSubFolder = "NotForLicensees";

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"uLangCore",
				"uLangToolchainDependencies",
				"uLangLocalization",
			}
		);

		string ModuleName = this.GetType().Name.ToUpper();
		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PublicDefinitions.Add(ModuleName + "_TESTAPI=" + ModuleName + "_API");
		}
		else
		{
			PublicDefinitions.Add(ModuleName + "_TESTAPI=");
		}

		bDisableAutoRTFMInstrumentation = true;
	}
}
