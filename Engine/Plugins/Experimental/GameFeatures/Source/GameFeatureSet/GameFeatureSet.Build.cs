// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameFeatureSet : ModuleRules
	{
        public GameFeatureSet(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
                {
                    "Core",
                    "CoreUObject",
					"DeveloperSettings",
					"Engine",
                    "ModularGameplay",
					"GameFeatures"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Projects"
				}
			);

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"PluginUtils",
					}
				);
			}
		}
	}
}
