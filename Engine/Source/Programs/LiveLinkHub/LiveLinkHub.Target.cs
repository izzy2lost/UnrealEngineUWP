// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class LiveLinkHubTarget : TargetRules
{
	public LiveLinkHubTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Modular;
		LaunchModuleName = "LiveLinkHubLauncher";

		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		SolutionDirectory = "Programs/LiveLink";

		AdditionalPlugins.Add("LiveLink");
		AdditionalPlugins.Add("ContentBrowserAssetDataSource");
		AdditionalPlugins.Add("StructUtils");
		AdditionalPlugins.Add("UdpMessaging");
		AdditionalPlugins.Add("QuicMessaging");
		AdditionalPlugins.Add("PropertyAccessEditor");
		AdditionalPlugins.Add("PythonScriptPlugin");

		OptionalPlugins.AddRange(new string[]
		{
			"AppleARKitFaceSupport",
			//"LiveLinkViconDataStream",
			"MocopiLiveLink",
			"OptitrackLiveLink"
		});

		bCompileAgainstCoreUObject = true;
		bCompileAgainstEngine = true;
		bCompileAgainstEditor = true;
		bBuildWithEditorOnlyData = true;
		bIncludePluginsForTargetPlatforms = false;
		bLegalToDistributeBinary = true;

		bUsesSlate = true;

		bCompileWithPluginSupport = true;
		bBuildDeveloperTools = true;
		bIsBuildingConsoleApplication = false;

		GlobalDefinitions.Add("WITH_LIVELINK_HUB=1");

		bEnableTrace = false;

		OptedInModulePlatforms = new UnrealTargetPlatform[] { UnrealTargetPlatform.Win64, UnrealTargetPlatform.Mac,
															  UnrealTargetPlatform.Linux, UnrealTargetPlatform.LinuxArm64 };

		// todo: Look into using ExeBinariesSubFolder
	}
}
