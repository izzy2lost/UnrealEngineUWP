// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX12 : ModuleRules
{
	public DX12(TargetInfo Target)
	{
		Type = ModuleType.External;

// @ATG_CHANGE : BEGIN UWP support
		string DirectXSDKDir = WindowsPlatform.bUseWindowsSDK10 ?
			UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DX12" :
			UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DirectX";
// @ATG_CHANGE : END
		PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x64");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x86");
		}
// @ATG_CHANGE : BEGIN UWP support
		if (WindowsPlatform.bUseWindowsSDK10)
		{
			PublicSystemIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DirectXLegacy/Include");
		}
// @ATG_CHANGE : END

		// Always delay-load D3D12
		PublicDelayLoadDLLs.AddRange( new string[] {
			"d3d12.dll"
			} );

		PublicAdditionalLibraries.AddRange(
			new string[] {
                "d3d12.lib"
			}
			);
	}
}

