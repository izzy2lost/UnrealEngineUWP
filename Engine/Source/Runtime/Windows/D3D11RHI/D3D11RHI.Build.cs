// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class D3D11RHI : ModuleRules
{
	public D3D11RHI(TargetInfo Target)
	{
// @ATG_CHANGE : BEGIN UWP support
		if (Target.Platform == UnrealTargetPlatform.UWP64 || Target.Platform == UnrealTargetPlatform.UWP32)
		{
			PrivateIncludePaths.Add("Runtime/Windows/D3D11RHI/Private/UWP");
		}
// @ATG_CHANGE : END
		PrivateIncludePaths.Add("Runtime/Windows/D3D11RHI/Private");

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

		AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
// @ATG_CHANGE : BEGIN UWP support
		if (Target.Platform != UnrealTargetPlatform.UWP64 && Target.Platform != UnrealTargetPlatform.UWP32)
		{ 
        	AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");
		}
// @ATG_CHANGE : END

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "TaskGraph" });
		}
	}
}
