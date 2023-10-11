// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class NNERuntimeRDG : ModuleRules
{
	public NNERuntimeRDG(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp17;

		// Replace with PCHUsageMode.UseExplicitOrSharedPCHs when this plugin can compile with cpp20
		PCHUsage = PCHUsageMode.NoPCHs;

		PublicDependencyModuleNames.AddRange(new string[] 
		{ 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore",
			"RenderCore"
		});

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "NNE",
			"NNEUtils",
			"NNEHlslShaders",
            "RHI",
			"Projects"
        });

        if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.Add("D3D12RHI");
			PrivateDependencyModuleNames.Add("DirectML");

			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "DirectML");

			PublicDefinitions.Add("NNE_USE_DIRECTML");
		}

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{	
			PrivateDependencyModuleNames.Add("MetalRHI");
		}

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{	
			PrivateDependencyModuleNames.Add("VulkanRHI");
		}
	}
}
