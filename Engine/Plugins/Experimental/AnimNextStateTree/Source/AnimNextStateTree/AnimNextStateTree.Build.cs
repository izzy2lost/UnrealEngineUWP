// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimNextStateTree : ModuleRules
	{
		public AnimNextStateTree(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AnimNext", 
					"AnimNextAnimGraph",
					"RigVM"
				});
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"StateTreeModule",
					"Engine"
				}
			);
		}
	}
}