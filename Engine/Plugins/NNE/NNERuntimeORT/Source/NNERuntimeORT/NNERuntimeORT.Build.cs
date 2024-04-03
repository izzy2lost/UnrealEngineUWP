// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NNERuntimeORT : ModuleRules
{
	public NNERuntimeORT( ReadOnlyTargetRules Target ) : base( Target )
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"NNE",
			"NNEOnnxruntime",
			"Projects",
			"RenderCore",
			"DeveloperSettings"
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"D3D12RHI",
				"RHI"
			});

			AddEngineThirdPartyPrivateStaticDependencies(Target, new string[]
			{
				"DirectML",
				"DX12"
			});
		}
	}
}
