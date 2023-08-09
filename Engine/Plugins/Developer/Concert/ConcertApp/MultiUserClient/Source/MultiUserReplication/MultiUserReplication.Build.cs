// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MultiUserReplication : ModuleRules
	{
		public MultiUserReplication(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"ConcertSyncCore", 
					"Engine",
				}
			);
			
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Concert",
					"ConcertClient"
				}
			);
		}
	}
}
