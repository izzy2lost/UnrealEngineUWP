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
				"DynamicMaterial"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AppFramework",
				"ApplicationCore",
				"AssetDefinition",
				"ContentBrowser",
				"CustomDetailsView",
				"DeveloperSettings",
				"EditorWidgets",
				"Engine",
				"InputCore",
				"Json",
				"JsonUtilities",
				"MaterialEditor",
				"Projects",
				"PropertyEditor",
				"RenderCore",
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
