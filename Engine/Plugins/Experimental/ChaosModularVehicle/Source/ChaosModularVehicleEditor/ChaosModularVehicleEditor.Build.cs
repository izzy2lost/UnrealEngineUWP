// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ChaosModularVehicleEditor : ModuleRules
	{
		public ChaosModularVehicleEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			SetupModulePhysicsSupport(Target);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{

				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"EditorFramework",
				"UnrealEd",
				"MainFrame",
				"DesktopPlatform",
				"PropertyEditor",
				"RHI",
				"RawMesh",
				"AssetTools",
				"AssetRegistry",
				"SceneOutliner",
				"ToolMenus",
				"EditorSubsystem",
				"PhysicsUtilities",
				"Chaos",
				"ChaosVehiclesCore",
				"ChaosModularVehicleEngine",
				"GeometryCollectionEngine",
				"GeometryCollectionEditor",
				"ProceduralMeshComponent",			
				"SubobjectEditor",
				"SubobjectDataInterface",
				}
			);

			PrivateDefinitions.Add("CHAOS_INCLUDE_LEVEL_1=1");
		}
	}
}
