// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SmartObjectsEditorModule : ModuleRules
	{
		public SmartObjectsEditorModule(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
			new string[] {
			}
			);

			PublicDependencyModuleNames.AddRange(
			new string[] {
				"AdvancedPreviewScene",
				"Core",
				"CoreUObject",
				"ComponentVisualizers",
				"EditorInteractiveToolsFramework",
				"Engine",
				"GameplayTags",
				"InputCore",
				"InteractiveToolsFramework",
				"PropertyEditor",
				"SlateCore",
				"Slate",
				"SmartObjectsModule",
				"SourceControl",
				"UnrealEd",
				"StructUtils",
				"WorldConditions",
				"ToolWidgets",
			}
			);

			PrivateDependencyModuleNames.AddRange(
			new string[] {
				"RenderCore",
				"ApplicationCore",
				"AssetDefinition",
				"StructUtilsEditor",
			}
			);
		}

	}
}
