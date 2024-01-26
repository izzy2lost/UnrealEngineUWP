// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealEditorVMTarget : UnrealEditorTarget
{
	public UnrealEditorVMTarget( TargetInfo Target ) : base(Target)
	{
		// Use the new VM (= don't use the BPVM)
		bUseVerseBPVM = false;
		bUseAutoRTFMCompiler = true;
		BuildEnvironment = TargetBuildEnvironment.Unique;
		bLegalToDistributeBinary = true;
		Type = TargetType.Program;

		DisablePlugins.AddRange(
			new string[] {
				"ConcertSyncEntity",
				"EntityActor",
				"EntityCore",
				"EntityFramework",
				"EpicGamesEngine",
				"EpicGamesEngineEditor",
				"EpicGamesEngineRestricted",
				"EpicGamesTemporary",
				"UnrealEngineExperimental",
				"VerseColors",
				"VerseEngine",
				"VerseEngineRestricted",
				"VerseExperimental",
				"VerseGameplay",
				"VerseGameplayDebug",
				"VerseGameplayTags",
				"VerseGeoScript",
				"VerseInput",
				"VerseMovement",
				"VersePrint",
				"VerseRestricted",
				"VerseSimulation",
				"VerseSimulationMetadata",
				"VerseUEStandalone",
				"VerseUI",
				"VerseWorldPartition",
				"VGameplayRst",
			}
		);
	}
}
