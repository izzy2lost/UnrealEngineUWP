// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using UnrealBuildTool;


[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class LiveLinkHubEditorTarget : TargetRules
{
	// Whether to enable building Capture Manager plugin.
	[CommandLine("-EnableCaptureManagerPlugin=")]
	public bool bEnableCaptureManagerPlugin = false;

	public LiveLinkHubEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		bExplicitTargetForType = true;
		bGenerateProgramProject = true;

		SolutionDirectory = "Programs/LiveLink";
		LaunchModuleName = "LiveLinkHubLauncher";
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		// These plugins are required for running LiveLinkHub. 
		// They may be a direct dependency or a dependency of another plugin.
		EnablePlugins.AddRange(new string[]
		{
			"LiveLink",
			"LiveLinkHub",
			"LiveLinkCamera",
			"LiveLinkLens",
			"LensComponent",
			"LiveLinkInputDevice",
			"ContentBrowserAssetDataSource",
			"ProceduralMeshComponent",
			"PropertyAccessEditor",
			"PythonScriptPlugin",
			"StructUtils",
			"UdpMessaging",
			"CameraCalibrationCore",
			"AppleARKitFaceSupport",
		});

		if (bEnableCaptureManagerPlugin)
		{
			OptionalPlugins.AddRange(new string[]
			{
				"CaptureManager"
			});
		}

		// FIXME?: Without this, staging fails on several files in TargetPlatform-related
		// restricted subdirectories (Engine/Binaries/Win64/{Android,IOS,Linux,LinuxArm64...}).
		OptedInModulePlatforms = new UnrealTargetPlatform[] { Target.Platform };

		bAllowEnginePluginsEnabledByDefault = false;
		bBuildAdditionalConsoleApp = false;

		// Based loosely on the VCProject.cs logic to construct NMakePath.
		string BaseExeName = "LiveLinkHubEditor";
		OutputFile = "Binaries/" + Platform.ToString() + "/" + BaseExeName;
		if (Configuration != UndecoratedConfiguration)
		{
			OutputFile += "-" + Platform.ToString() + "-" + Configuration.ToString();
		}
		if (Platform == UnrealTargetPlatform.Win64)
		{
			OutputFile += ".exe";
		}
	}
}
