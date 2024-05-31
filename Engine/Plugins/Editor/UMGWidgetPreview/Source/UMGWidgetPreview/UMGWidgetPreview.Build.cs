// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UMGWidgetPreview : ModuleRules
{
	public UMGWidgetPreview(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AdvancedPreviewScene",
				"AssetDefinition",
				"ContentBrowser",
				"Core",
				"CoreUObject",
                "DataValidation",
				"EditorSubsystem",
				"Engine",
				"InputCore",
				"MessageLog",
				"Projects",
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UMG",
				"UMGEditor",
				"UnrealEd",
			});
	}
}
