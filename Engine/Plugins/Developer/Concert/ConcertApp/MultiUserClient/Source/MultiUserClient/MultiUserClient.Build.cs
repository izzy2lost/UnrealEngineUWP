// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MultiUserClient : ModuleRules
	{
		public MultiUserClient(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"ApplicationCore",
					"Concert",
					"ConcertClient",
					"ConcertClientSharedSlate",
					"ConcertSharedSlate",
					"ConcertSyncClient",
					"ConcertSyncCore",
					"ConcertTransport",
					"ContentBrowser",
					"DesktopPlatform",
					"EditorStyle",
					"InputCore",
					"MessageLog",
					"MultiUserReplication",
					"Projects",
					"Slate",
					"SlateCore",
					"SourceControl",
					"ToolMenus",
					"ToolWidgets"
				}
			);

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange( 
					new string[]
					{ 
						"EditorFramework",
						"MultiUserReplicationEditor",
						"PropertyEditor",
						"Settings",
						"Sequencer",
						"ToolMenus",
						"UnrealEd",
						"WorkspaceMenuStructure",
					}
				);
			}
		}
	}
}
