// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

// Experimental test module. please refrain from depending on it until this warning is removed
public class TypedElementsDataStorageAssetData : ModuleRules
{
	public TypedElementsDataStorageAssetData(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		if (Target.bBuildEditor)
		{
			PublicIncludePaths.AddRange(new string[] { });
			PrivateIncludePaths.AddRange(new string[] { });

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetRegistry",
					"Core",
					"CoreUObject",
					"Engine",
					"TypedElementFramework",
				});

		}
	}
}
