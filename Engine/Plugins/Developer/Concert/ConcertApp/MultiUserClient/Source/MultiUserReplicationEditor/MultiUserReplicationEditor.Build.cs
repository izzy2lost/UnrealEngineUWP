// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MultiUserReplicationEditor : ModuleRules
	{
		public MultiUserReplicationEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					// Engine
					"Core",
					"CoreUObject",
					"Engine",
					
					// Concert
					"ConcertSyncCore",
					"MultiUserReplication"
				}
			);
			
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// Engine
					"AssetDefinition",
					"AssetRegistry",
					"InputCore",
					"Projects",
					"Slate",
					"SlateCore",
					"ToolWidgets",
					"UnrealEd", 
					
					// Concert
					"Concert",
					"ConcertClient",
					"ConcertSharedSlate",
					"MultiUserClient"
				}
			);
		}
	}
}
