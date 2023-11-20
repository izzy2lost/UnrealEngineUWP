// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class CaptureSourceFramework : ModuleRules
{
	public CaptureSourceFramework(ReadOnlyTargetRules Target) : base(Target)
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
			"AudioEditor",
			"UnrealEd",
			"DesktopWidgets",
			"Slate",
			"SlateCore",
			"RenderCore",
		});
	}
}
