// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class D3D12RHI : ModuleRules
{
	public D3D12RHI(ReadOnlyTargetRules Target) : base(Target)
	{
		if (Target.Platform == UnrealTargetPlatform.HoloLens)
		{
			PrivateIncludePaths.Add("Runtime/D3D12RHI/Private/HoloLens");
		}
		// @UWP_CHANGE : BEGIN UWP support
		else if (Target.Platform == UnrealTargetPlatform.UWP32 || Target.Platform == UnrealTargetPlatform.UWP64)
        {
			PrivateIncludePaths.Add("Runtime/D3D12RHI/Private/UWP");
		}
		// @UWP_CHANGE : END
		PrivateIncludePaths.Add("Runtime/D3D12RHI/Private");
		PrivateIncludePaths.Add("../Shaders/Shared");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine",
				"RHI",
				"RenderCore",
				}
			);

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "TaskGraph" });
		}

		///////////////////////////////////////////////////////////////
		// Platform specific defines
		///////////////////////////////////////////////////////////////

		if (Target.Platform != UnrealTargetPlatform.Win32 && Target.Platform != UnrealTargetPlatform.Win64 && Target.Platform != UnrealTargetPlatform.XboxOne)
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}

		// @UWP_CHANGE : BEGIN UWP support
        if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 ||
            Target.Platform == UnrealTargetPlatform.HoloLens ||
            Target.Platform == UnrealTargetPlatform.UWP64 || Target.Platform == UnrealTargetPlatform.UWP32)
        // @UWP_CHANGE : END
        {
            AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
            AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");

            // @UWP_CHANGE : BEGIN UWP support
            if (Target.Platform != UnrealTargetPlatform.HoloLens && Target.Platform != UnrealTargetPlatform.UWP64 && Target.Platform != UnrealTargetPlatform.UWP32)
			// @UWP_CHANGE : END
            {
                AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "AMD_AGS");
            	AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAftermath");
            	AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelMetricsDiscovery");
            }
        }
    }
}
