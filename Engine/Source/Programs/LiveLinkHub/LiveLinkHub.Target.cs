// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;
using EpicGames.Core;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class LiveLinkHubTarget : TargetRules
{
	[CommandLine("-Monolithic")]
	public bool bMonolithic = false;
	public LiveLinkHubTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = bMonolithic ? TargetLinkType.Monolithic : TargetLinkType.Modular;
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
		GlobalDefinitions.Add("AUTOSDKS_ENABLED=0");
		GlobalDefinitions.Add("NO_LOGGING=0");

		bEnableTrace = true;

		OptedInModulePlatforms = new UnrealTargetPlatform[] { UnrealTargetPlatform.Win64, UnrealTargetPlatform.Mac,
															  UnrealTargetPlatform.Linux, UnrealTargetPlatform.LinuxArm64 };

		// todo: Look into using ExeBinariesSubFolder
	}
}
