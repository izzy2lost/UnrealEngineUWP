// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GeometryCollectionDeprecatedNodes : ModuleRules
	{
        public GeometryCollectionDeprecatedNodes(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"ChaosCore",
					"Chaos",
					"DataflowCore",
					"DataflowEngine",
					"GeometryCollectionEngine",
					"Engine"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] 
				{
				}
			);
		}
	}
}
