// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO; // for Path

public class uLangToolchainDependencies : ModuleRules
{
	public uLangToolchainDependencies(ReadOnlyTargetRules Target) : base(Target)
	{
		IWYUSupport = IWYUSupport.KeepAsIsForNow;

		PCHUsage = PCHUsageMode.NoPCHs;
		bRequiresImplementModule = false;
		BinariesSubFolder = "NotForLicensees";
		ShortName = "ulangTCDeps";

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"uLangCore"
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"uLangJSON"
			}
		);

		bDisableAutoRTFMInstrumentation = true;
	}
}
