// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ChaosClothAssetDataflowNodes : ModuleRules
{
	public ChaosClothAssetDataflowNodes(ReadOnlyTargetRules Target) : base(Target)
	{	
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"DatasmithCore",  // Deprecated in 5.5 (remove in 5.7), for DatasmithCloth.h
		});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Chaos",
				"ChaosCloth",
				"ChaosClothAsset",
				"ChaosClothAssetEngine",
				"ChaosClothAssetTools",
				"ClothingSystemRuntimeCommon",
				"CoreUObject",
				"DataflowCore",
				"DataflowEditor",
				"DataflowEngine",
				"DesktopWidgets",  // For SFilePathPicker
				"DetailCustomizations",
				"DynamicMesh",
				"Engine",
				"GeometryCore",
				"InputCore",
				"MeshConversion",
				"MeshDescription",
				"MeshUtilitiesCommon",
				"ModelingOperatorsEditorOnly",	// TODO: Someday remove editor dependencies, see UE-206172
				"ModelingOperators",
				"RenderCore",
				"SkeletalMeshDescription",
				"Slate",
				"SlateCore",
				"StaticMeshDescription",
				"UnrealEd",
				"UnrealUSDWrapper",
				"USDClasses",
				"USDSchemas",
				"USDStage",
				"USDStageImporter",
				"USDUtilities",
				// ... add private dependencies that you statically link with here ...
			}
		);
	}
}
