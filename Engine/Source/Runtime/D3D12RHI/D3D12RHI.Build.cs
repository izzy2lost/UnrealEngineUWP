// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class D3D12RHI : ModuleRules
{
	public D3D12RHI(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Runtime/D3D12RHI/Private");

        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine",
				"RHI",
				"RenderCore",
				"ShaderCore",
				"UtilityShaders",
			    }
			);

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "TaskGraph" });
		}

		///////////////////////////////////////////////////////////////
        // Platform specific defines
        ///////////////////////////////////////////////////////////////

        if (Target.Platform != UnrealTargetPlatform.Win64 && Target.Platform != UnrealTargetPlatform.XboxOne)
        {
            PrecompileForTargets = PrecompileTargetsType.None;
        }

        // @ATG_CHANGE : BEGIN UWP support
        if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 ||
            Target.Platform == UnrealTargetPlatform.UWP64 || Target.Platform == UnrealTargetPlatform.UWP32)
        // @ATG_CHANGE : END
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
            // @ATG_CHANGE : BEGIN UWP support
            if (Target.Platform != UnrealTargetPlatform.UWP64 && Target.Platform != UnrealTargetPlatform.UWP32)
            {
                AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");
            }
            // @ATG_CHANGE : END

			// @ATG_CHANGE : BEGIN UWP support
			if ((Target.Platform == UnrealTargetPlatform.UWP64 || Target.Platform == UnrealTargetPlatform.UWP32) &&
				(Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Configuration != UnrealTargetConfiguration.Test))
			{
				Definitions.Add("D3D12_PROFILING_ENABLED=1");
				Definitions.Add("PROFILE");
			}
			// @ATG_CHANGE : END
		}
	}
}
