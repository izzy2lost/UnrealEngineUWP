// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class AudioModulationTest : ModuleRules
	{
		public AudioModulationTest(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange
			(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"AudioExtensions",
					"AudioMixer",
					"Projects"
				}
			);

			PublicDependencyModuleNames.AddRange
			(
				new string[]
				{
					"AudioModulation",
				}
			);
		}
	}
}
