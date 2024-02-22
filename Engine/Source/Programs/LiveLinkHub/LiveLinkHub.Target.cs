// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;
using EpicGames.Core;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class LiveLinkHubTarget : TargetRules
{
	// Whether this should be built as a monolithic executable.
	[CommandLine("-Monolithic")]
	public bool bMonolithic = false;

	// Whether the hub is being built for distribution alongside a cooked editor.
	// Will dictate whether to mount LLH's remapped engine folder to "/Engine/"
	[CommandLine("-CookedEditorDistribution")]
	public bool bCookedEditorDistribution = false;

	public LiveLinkHubTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = bMonolithic ? TargetLinkType.Monolithic : TargetLinkType.Modular;
		LaunchModuleName = "LiveLinkHubLauncher";

		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		SolutionDirectory = "Programs/LiveLink";

		AdditionalPlugins.Add("LiveLink");
		AdditionalPlugins.Add("LiveLinkHub");
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
			"LiveLinkInputDevice",
			"OptitrackLiveLink"
		});

		bCompileAgainstCoreUObject = true;
		bCompileAgainstEngine = true;
		bCompileAgainstEditor = true;
		bBuildWithEditorOnlyData = true;
		bIncludePluginsForTargetPlatforms = false;
		bLegalToDistributeBinary = true;

		bUsesSlate = true;

		bCompileICU = false;
		bCompilePython = false; 
		bCompileWithPluginSupport = true;
		bBuildDeveloperTools = true;
		bIsBuildingConsoleApplication = false;

		GlobalDefinitions.Add("WITH_LIVELINK_HUB=1");
		GlobalDefinitions.Add("AUTOSDKS_ENABLED=0");
		GlobalDefinitions.Add("NO_LOGGING=0");
		GlobalDefinitions.Add("PLATFORM_SUPPORTS_MESSAGEBUS=1");

		if (bCookedEditorDistribution)
		{
			GlobalDefinitions.Add("COOKED_EDITOR_DISTRIBUTION=1");
		}

		bEnableTrace = true;

		

		OptedInModulePlatforms = new UnrealTargetPlatform[] { UnrealTargetPlatform.Win64, UnrealTargetPlatform.Mac,
															  UnrealTargetPlatform.Linux, UnrealTargetPlatform.LinuxArm64 };

		// todo: Look into using ExeBinariesSubFolder
	}
}
