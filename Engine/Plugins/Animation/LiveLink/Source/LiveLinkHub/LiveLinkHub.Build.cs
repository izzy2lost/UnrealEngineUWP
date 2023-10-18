// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LiveLinkHub : ModuleRules
	{
		public LiveLinkHub(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
			});

			PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"AssetTools",
				"ContentBrowser",
				"ContentBrowserAssetDataSource",
				"ContentBrowserData",
				"Engine",
				"InputCore",
				"LiveLink",
				"LiveLinkEditor",
				"LiveLinkInterface",
				"LiveLinkMessageBusFramework",
				"Slate",
				"SlateCore",
				"StandaloneRenderer",
				"StructUtils",
				"TimeManagement",
				"ToolWidgets",
				"UnrealEd",
			});
		}
	}
}
