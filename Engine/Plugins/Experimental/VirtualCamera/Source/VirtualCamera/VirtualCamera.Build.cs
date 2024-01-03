// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VirtualCamera : ModuleRules
{
	public VirtualCamera(ReadOnlyTargetRules Target) : base(Target)
	{
		DefaultBuildSettings = BuildSettingsVersion.V2;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AugmentedReality",
				"CinematicCamera",
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"LevelSequence",
				"LiveLinkInterface",
				"MovieScene",
				"RemoteSession",
				"TimeManagement",
				"UMG",
				"VCamCore",
				"VPUtilities",
				"AssetRegistry",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"MediaIOCore",
				"Slate",
				"AdvancedWidgets"
			}
		);
		
		if (Target.bBuildEditor == true)
		{
			PublicDependencyModuleNames.Add("LevelSequenceEditor");
			PublicDependencyModuleNames.Add("Sequencer");
			PublicDependencyModuleNames.Add("SlateCore");
			PublicDependencyModuleNames.Add("TakeRecorder");
			PrivateDependencyModuleNames.Add("LevelEditor");
			PrivateDependencyModuleNames.Add("EditorFramework");
			PrivateDependencyModuleNames.Add("UnrealEd");
			PrivateDependencyModuleNames.Add("EditorScriptingUtilities");
			PrivateDependencyModuleNames.Add("VPUtilitiesEditor");
			PrivateDependencyModuleNames.Add("TakesCore");
		}
	}
}
