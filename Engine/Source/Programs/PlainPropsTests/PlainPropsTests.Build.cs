// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class PlainPropsTests : TestModuleRules
{
	protected Metadata PlainPropsMetadata = new Metadata() {
		TestName = "PlainProps",
		TestShortName = "PlainProps",
		ReportType = "xml",
		SupportedPlatforms = {
			UnrealTargetPlatform.Win64,
			UnrealTargetPlatform.Linux,
			UnrealTargetPlatform.Mac} };

	public Metadata TestMetadata
	{ 
		get { return PlainPropsMetadata; }
	}

	public PlainPropsTests(ReadOnlyTargetRules Target) : base(Target, true)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject"
			});

		if (Target.bBuildWithEditorOnlyData)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "DesktopPlatform" });
		}

		string PlatformCompilationArgs;
		foreach (var Platform in UnrealTargetPlatform.GetValidPlatforms())
		{
			PlatformCompilationArgs = "-allmodules";
			TestMetadata.PlatformCompilationExtraArgs.Add(Platform, PlatformCompilationArgs);
		}

		// Platform-specific tags
		TestMetadata.PlatformTags.Add(UnrealTargetPlatform.Linux, "~[.]~[Slow]");

		UpdateBuildGraphPropertiesFile(TestMetadata);
	}
}