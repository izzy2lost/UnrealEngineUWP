// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ConcertReplicationScriptingEditor : ModuleRules
	{
		public ConcertReplicationScriptingEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					// Engine
					"Core",
					"CoreUObject",
					"Engine",
					"Slate",
					"SlateCore",
					
					// Concert
					"ConcertReplicationScripting",
					"ConcertSyncCore"
				}
			);
			
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// Engine
					"Slate",
					"SlateCore",
					"UnrealEd",
					
					// Concert
					"ConcertTransport", // For LogConcert
				}
			);
            
			// TODO UE-202216: Do proper fix
			ShortName = "CSE";
		}
	}
}
