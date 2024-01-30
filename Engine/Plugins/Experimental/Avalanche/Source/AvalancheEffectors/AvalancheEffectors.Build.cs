// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheEffectors : ModuleRules
{
	public AvalancheEffectors(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AvalancheCore",
				"Core",
				"CoreUObject",
				"Engine",
				"Niagara",
				"NiagaraCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Avalanche",
				"AvalancheSceneTree",
				"GeometryCore",
				"GeometryFramework",
				"GeometryScriptingCore",
				"DynamicMesh",
				"ProceduralMeshComponent",
				"StaticMeshDescription"
			}
		);

		if (Target.Type == TargetRules.TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"AvalancheOutliner",
				"UnrealEd",
				"Slate",
				"SlateCore"
			});
		}
	}
}
