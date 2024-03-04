// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
public class TextureGraphEditor : ModuleRules
{
	// Flag that enables the new node preview.
	private const bool EnableNewNodePreview = true;
	// Flag that disables the old selection preview. This is only used if the EnableNewNodePreview is set to true.
	private const bool DisableOldSelectionPreview = false;

	public TextureGraphEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
            }
			);
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"TextureGraph",
				"TextureGraphEngine",
				"Renderer",
				"RenderCore",
				"RHI",
				"Projects",
				"ToolWidgets",
				"KismetWidgets",
				// ... add other public dependencies that you statically link with here ...
			}
			);
		if (Target.bBuildEditor == true)
		{
			//reference the module "MyModule"
			PublicDependencyModuleNames.AddRange(new string[]
			{
				"AssetTools",
				"UnrealEd",
				"EditorFramework",
				"ToolMenus",
				"AdvancedPreviewScene",
				"PropertyEditor",
				"ContentBrowser",
				"ContentBrowserData"
			});
		}
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"ApplicationCore",
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
				"GraphEditor", 
				"MessageLog",
				"EditorWidgets",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		if (EnableNewNodePreview)
		{
			PrivateDependencyModuleNames.Add("ImageWidgets");
		}

		PublicDefinitions.Add(EnableNewNodePreview
			? "TEXTUREGRAPHEDITOR_ENABLE_NEW_NODE_PREVIEW=1"
			: "TEXTUREGRAPHEDITOR_ENABLE_NEW_NODE_PREVIEW=0");
		PublicDefinitions.Add(EnableNewNodePreview && DisableOldSelectionPreview
			? "TEXTUREGRAPHEDITOR_ENABLE_OLD_SELECTION_PREVIEW=0"
			: "TEXTUREGRAPHEDITOR_ENABLE_OLD_SELECTION_PREVIEW=1");
	}
}