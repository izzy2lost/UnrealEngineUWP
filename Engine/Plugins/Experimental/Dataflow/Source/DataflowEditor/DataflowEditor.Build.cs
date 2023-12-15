// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DataflowEditor : ModuleRules
	{
        public DataflowEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePathModuleNames.AddRange(
				new string[]
				{
					"SkeletonEditor"
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"AdvancedPreviewScene",
					"ApplicationCore",
					"AssetDefinition",
					"AssetTools",
					"AssetRegistry",
					"BaseCharacterFXEditor",
					"Chaos",
					"Core",
					"CoreUObject",
					"DataflowCore",
					"DataflowEngine",
					"DataflowEnginePlugin",
					"DataflowNodes",
					"Engine",
					"EditorFramework",
					"EditorInteractiveToolsFramework",
					"EditorStyle",
					"GraphEditor",
					"InputCore",
					"InteractiveToolsFramework",
					"MeshModelingToolsEditorOnlyExp",
					"ModelingComponentsEditorOnly",
					"ModelingComponents",
					"GeometryCore",
					"GeometryFramework",
					"MeshDescription",
					"StaticMeshDescription",
					"MeshConversion",
					"LevelEditor",
					"Slate",
				    "SlateCore",
					"UnrealEd",
					"Projects",
					"PropertyEditor",
				    "RenderCore",
				    "RHI",
				    "SceneOutliner",
					"ToolMenus",
					"ToolWidgets",
					"Slate",
					"XmlParser",
					"TypedElementRuntime",
					"MeshModelingToolsExp"
				}
			);
		}
	}
}
