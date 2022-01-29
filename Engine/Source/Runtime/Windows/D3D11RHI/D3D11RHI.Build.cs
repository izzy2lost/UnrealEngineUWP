// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win32", "Win64", "HoloLens")]
public class D3D11RHI : ModuleRules
{
	public D3D11RHI(ReadOnlyTargetRules Target) : base(Target)
	{
		if (Target.Platform == UnrealTargetPlatform.HoloLens)
		{
			PrivateIncludePaths.Add("Runtime/Windows/D3D11RHI/Private/HoloLens");
		}
// @ATG_CHANGE : BEGIN UWP support
		if (Target.Platform == UnrealTargetPlatform.UWP64 || Target.Platform == UnrealTargetPlatform.UWP32)
		{
			PrivateIncludePaths.Add("Runtime/Windows/D3D11RHI/Private/UWP");
		}
// @ATG_CHANGE : END
		PrivateIncludePaths.Add("Runtime/Windows/D3D11RHI/Private");
		PrivateIncludePaths.Add("../Shaders/Shared");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine",
				"RHI",
				"RenderCore"
			}
			);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
// @EMMETTJNR_CHANGE : BEGIN UWP support
		if (Target.Platform != UnrealTargetPlatform.HoloLens && Target.Platform != UnrealTargetPlatform.UWP64 && Target.Platform != UnrealTargetPlatform.UWP32)
// @EMMETTJNR_CHANGE : END
		{ 
        	AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "AMD_AGS");
        	AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAftermath");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelMetricsDiscovery");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelExtensionsFramework");
		}


        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "TaskGraph" });
		}
	}
}
