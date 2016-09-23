// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX11Audio : ModuleRules
{
	public DX11Audio(TargetInfo Target)
	{
		Type = ModuleType.External;

// @ATG_CHANGE : BEGIN UWP support
		string DirectXSDKDir = WindowsPlatform.bUseWindowsSDK10 ?
			UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DirectXLegacy" :
			UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DirectX";        

		PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemIncludePaths.Add(DirectXSDKDir + "/include/Win7");
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x64");
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x64/Win7");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicSystemIncludePaths.Add(DirectXSDKDir + "/include/Win7");
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x86");
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x86/Win7");
		}
// @ATG_CHANGE : END
		PublicAdditionalLibraries.AddRange(
			new string[] {
				"dxguid.lib",
				"X3DAudio.lib",
				"xapobase.lib",
				"XAPOFX.lib"
			}
			);
	}
}

