// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class LiveLinkHubCaptureSource : ModuleRules
{
	public LiveLinkHubCaptureSource(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CaptureUtils"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
			"Slate",
			"Json",
			"SlateCore",
			"AudioEditor",
			"UnrealEd",
			"RenderCore",
			"CaptureSourceFramework",
			"LiveLinkHubCaptureMessaging",
			"ContentBrowser",
			"DesktopPlatform",
			"MessagingCommon"
		});
	}
}
