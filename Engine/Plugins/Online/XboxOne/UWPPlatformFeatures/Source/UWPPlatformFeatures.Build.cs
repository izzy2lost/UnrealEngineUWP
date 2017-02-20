// Copyright Microsoft Inc. All Rights Reserved.

using UnrealBuildTool;

public class UWPPlatformFeatures : ModuleRules
{
	public UWPPlatformFeatures(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core", 
				"Engine", 
				"OnlineSubsystem"
				});

		// This module requires installation of the Xbox Platform Extensions SDK for UWP
		PrivateWinMDReferences.Add(VCEnvironment.GetLatestMetadataPathForApiContract("Windows.Gaming.XboxLive.StorageApiContract"));
	}
}
