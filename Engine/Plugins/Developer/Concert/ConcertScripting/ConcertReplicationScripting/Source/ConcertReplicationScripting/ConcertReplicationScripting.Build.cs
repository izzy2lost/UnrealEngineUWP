// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ConcertReplicationScripting : ModuleRules
	{
		public ConcertReplicationScripting(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					// Engine
					"Core",
					"CoreUObject",
					"Engine",
					
					// Concert
					"ConcertSyncCore"
				}
			);
			
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// Concert
					"ConcertTransport", // For LogConcert
				}
			);

			// TODO UE-202216: Do proper fix
			ShortName = "CS";
		}
	}
}