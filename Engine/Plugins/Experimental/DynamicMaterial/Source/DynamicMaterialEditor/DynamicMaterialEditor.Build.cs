// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DynamicMaterialEditor : ModuleRules
{
	public DynamicMaterialEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"DynamicMaterial",
				"Engine"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AppFramework",
				"ApplicationCore",
				"AssetDefinition",
				"ContentBrowser",
				"ContentBrowserData",
				"CustomDetailsView",
				"DeveloperSettings",
				"DynamicMaterialShadersEditor",
				"EditorSubsystem",
				"EditorWidgets",
				"InputCore",
				"Json",
				"JsonUtilities",
				"MaterialEditor",
				"Projects",
				"PropertyEditor",
				"RenderCore",
				"Renderer",
				"RHI",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"TypedElementRuntime",
				"UMG",
				"UnrealEd",
				"WorkspaceMenuStructure"
			}
		);

		ShortName = "DynMatEd";
	}
}
