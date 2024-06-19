// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class StateTreeEditorModule : ModuleRules
	{
		public StateTreeEditorModule(ReadOnlyTargetRules Target) : base(Target)
		{
			UnsafeTypeCastWarningLevel = WarningLevel.Warning;

			PublicIncludePaths.AddRange(
			new string[] {
			}
			);

			PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"AssetTools",
				"EditorFramework",
				"UnrealEd",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"PropertyEditor",
				"StateTreeModule",
				"SourceControl",
				"Projects",
				"BlueprintGraph",
				"PropertyAccessEditor",
				"StructUtilsEditor",
				"GameplayTags",
			}
			);

			PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AssetDefinition",
				"RenderCore",
				"GraphEditor",
				"KismetWidgets",
				"PropertyPath",
				"ToolMenus",
				"ToolWidgets",
				"ApplicationCore",
				"DeveloperSettings",
				"RewindDebuggerInterface",
				"DetailCustomizations",
				"AppFramework",
				"KismetCompiler"
			}
			);

			PrivateIncludePathModuleNames.AddRange(new string[] {
				"MessageLog",
			});

			PublicDefinitions.Add("WITH_STATETREE_TRACE=1");
			PublicDefinitions.Add("WITH_STATETREE_TRACE_DEBUGGER=1");
		}
	}
}
