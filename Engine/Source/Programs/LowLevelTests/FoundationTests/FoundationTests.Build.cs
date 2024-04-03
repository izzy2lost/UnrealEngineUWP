// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class FoundationTests : TestModuleRules
{
	protected Metadata FoundationTestsMetadata = new Metadata() {
		TestName = "Foundation",
		TestShortName = "Foundation",
		ReportType = "xml",
		SupportedPlatforms = {
			UnrealTargetPlatform.Win64,
			UnrealTargetPlatform.Linux,
			UnrealTargetPlatform.Mac,
			UnrealTargetPlatform.Android,
			UnrealTargetPlatform.IOS } };

	/// <summary>
	/// Test metadata to be used with BuildGraph
	/// </summary>
	public Metadata TestMetadata
	{ 
		get { return FoundationTestsMetadata; }
	}

	public FoundationTests(ReadOnlyTargetRules Target) : base(Target, true)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Cbor",
				"CoreUObject",
				"TelemetryUtils",
				"AssetRegistry",
				"Serialization",
			});

		if (Target.bBuildWithEditorOnlyData)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"DesktopPlatform"
				});
		}

		string PlatformCompilationArgs;
		foreach (var Platform in UnrealTargetPlatform.GetValidPlatforms())
		{
			if (Platform == UnrealTargetPlatform.Android)
			{
				PlatformCompilationArgs = "-allmodules -architectures=arm64";
			}
			else
			{
				PlatformCompilationArgs = "-allmodules";
			}
			TestMetadata.PlatformCompilationExtraArgs.Add(Platform, PlatformCompilationArgs);
		}

		// Platform-specific tags
		TestMetadata.PlatformTags.Add(UnrealTargetPlatform.Linux, "~[.]~[Slow]");
		TestMetadata.PlatformTags.Add(UnrealTargetPlatform.Android, "~[Perf]~[Slow]~[AndroidSkip]");

		// Allow Android run for this test
		// Will remove Android from PlatformsRunUnsupported as more diverse types of tests can run on this platform
		TestMetadata.PlatformsRunUnsupported.Remove(UnrealTargetPlatform.Android);

		UpdateBuildGraphPropertiesFile(TestMetadata);
	}
}