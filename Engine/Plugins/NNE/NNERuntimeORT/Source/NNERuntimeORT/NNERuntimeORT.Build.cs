// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NNERuntimeORT : ModuleRules
{
	public NNERuntimeORT( ReadOnlyTargetRules Target ) : base( Target )
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange
			(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"NNE",
				"NNEOnnxruntimeEditor",
				"Projects",
				"RenderCore"
			}
		);

		if (Target.Type == TargetType.Editor || Target.Type == TargetType.Program)
		{
			PrivateDefinitions.Add("NNE_UTILITIES_AVAILABLE");
			PrivateDependencyModuleNames.Add("NNEUtilities");
		}

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"D3D12RHI",
				"DirectML",
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
