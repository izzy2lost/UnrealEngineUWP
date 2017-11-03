// Copyright Microsoft Inc. All Rights Reserved.

using UnrealBuildTool;

public class UWPPlatformFeatures : ModuleRules
{
	public UWPPlatformFeatures(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core", 
				"Engine", 
				"OnlineSubsystem"
				});

		// This module requires installation of the Xbox Platform Extensions SDK for UWP
		PrivateWinMDReferences.Add("Windows.Gaming.XboxLive.StorageApiContract");
	}
}
