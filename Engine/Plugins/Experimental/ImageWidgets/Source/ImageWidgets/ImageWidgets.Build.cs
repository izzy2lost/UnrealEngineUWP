// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ImageWidgets : ModuleRules
{
	// Set this flag to true to force building the ColorViewer sample. Otherwise, it will only be built in Debug builds. 
	private const bool ForceBuildColorViewerSample = false;

	// Todo Remove build flags as soon as all the respective prototype code is migrated to this module.
	// These flags are currently used to migrate already existing prototype functionality into this module without having to remove/change the functionality of
	// the prototype applications.
	private const bool EnableCatalog = true;
	private const bool EnableAbComparison = false;

	public ImageWidgets(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"Slate",
				"UnrealEd"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"ApplicationCore",
				"CoreUObject",
				"Engine",
				"InputCore",
				"SlateCore"
			}
		);

		bool bBuildColorViewerSample = ForceBuildColorViewerSample || Target.Configuration == UnrealTargetConfiguration.Debug;
		if (bBuildColorViewerSample)
		{
			PrivateDependencyModuleNames.AddRange(
				new[]
				{
					"Projects",
					"WorkspaceMenuStructure"
				}
			);
		}
		PublicDefinitions.Add(bBuildColorViewerSample ? "IMAGE_WIDGETS_BUILD_COLOR_VIEWER_SAMPLE=1" : "IMAGE_WIDGETS_BUILD_COLOR_VIEWER_SAMPLE=0");

		PublicDefinitions.Add(EnableCatalog ? "IMAGE_WIDGETS_WITH_CATALOG=1" : "IMAGE_WIDGETS_WITH_CATALOG=0");
		PublicDefinitions.Add(EnableAbComparison ? "IMAGE_WIDGETS_WITH_AB_COMPARISON=1" : "IMAGE_WIDGETS_WITH_AB_COMPARISON=0");
	}
}