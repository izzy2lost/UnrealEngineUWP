// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class D3D12RHI : ModuleRules
{
	public D3D12RHI(ReadOnlyTargetRules Target) : base(Target)
	{
		if (Target.Platform == UnrealTargetPlatform.HoloLens)
		{
			PrivateIncludePaths.Add("Runtime/D3D12RHI/Private/HoloLens");
		}
		// @EMMETTJNR_CHANGE : BEGIN UWP support
		else if (Target.Platform == UnrealTargetPlatform.UWP32 || Target.Platform == UnrealTargetPlatform.UWP64)
        {
			PrivateIncludePaths.Add("Runtime/D3D12RHI/Private/UWP");
		}
		// @EMMETTJNR_CHANGE : END
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

        if (!Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) && Target.Platform != UnrealTargetPlatform.XboxOne)
        {
            PrecompileForTargets = PrecompileTargetsType.None;
        }

// @EMMETTJNR_CHANGE : BEGIN UWP support
        if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) ||
            Target.Platform == UnrealTargetPlatform.HoloLens ||
            Target.Platform == UnrealTargetPlatform.UWP64 ||
            Target.Platform == UnrealTargetPlatform.UWP32)
// @EMMETTJNR_CHANGE : END
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
// @EMMETTJNR_CHANGE : BEGIN UWP support
            if (Target.Platform != UnrealTargetPlatform.HoloLens && Target.Platform != UnrealTargetPlatform.UWP64 && Target.Platform != UnrealTargetPlatform.UWP32)
// @EMMETTJNR_CHANGE : END
            {
				PrivateDependencyModuleNames.Add("GeForceNOWWrapper");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "AMD_AGS");
            	AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAftermath");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelMetricsDiscovery");
            }
        }
    }
}
