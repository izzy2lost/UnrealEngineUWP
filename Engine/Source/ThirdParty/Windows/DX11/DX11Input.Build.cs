// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX11Input : ModuleRules
{
	public DX11Input(TargetInfo Target)
	{
		Type = ModuleType.External;

// @ATG_CHANGE : BEGIN UWP support
        string DirectXSDKDir = WindowsPlatform.bUseWindowsSDK10 ?
            UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DirectXLegacy" :
			UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DirectX";

		PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x64");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x86");
		}
// @ATG_CHANGE : END
		PublicAdditionalLibraries.AddRange(
			new string[] {
				"dxguid.lib",
				"dinput8.lib"
			}
			);
	}
}

