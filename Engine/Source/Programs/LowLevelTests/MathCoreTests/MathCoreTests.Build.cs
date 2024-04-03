// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class MathCoreTests : TestModuleRules
{
	protected Metadata MathCoreTestsMetadata = new Metadata() {
		TestName = "MathCore",
		TestShortName = "MathCore",
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
		get { return MathCoreTestsMetadata; }
	}

	public MathCoreTests(ReadOnlyTargetRules Target) : base(Target, InUsesCatch2:true)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"MathCore",
			});

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
		TestMetadata.PlatformsRunUnsupported.Remove(UnrealTargetPlatform.Android);
		UpdateBuildGraphPropertiesFile(TestMetadata);
	}
}