// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX11 : ModuleRules
{
	public DX11(TargetInfo Target)
	{
		Type = ModuleType.External;

		Definitions.Add("WITH_D3DX_LIBS=1");

// @ATG_CHANGE : BEGIN UWP support

        string DirectXSDKDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DirectXLegacy";
 
		PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemIncludePaths.Add(DirectXSDKDir + "/include/Win7");
			PublicSystemIncludePaths.Add(DirectXSDKDir + "/include/xp"); 
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x64");
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x64/Win7");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicSystemIncludePaths.Add(DirectXSDKDir + "/include/Win7");
			PublicSystemIncludePaths.Add(DirectXSDKDir + "/include/xp");
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x86");
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x86/Win7");
		}
// @ATG_CHANGE : END

		PublicAdditionalLibraries.AddRange(
			new string[] {
				"dxgi.lib",
				"d3d9.lib",
				"d3d11.lib",
				"dxguid.lib",
				"d3dcompiler.lib",
                (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ? "d3dx11d.lib" : "d3dx11.lib",				
				"dinput8.lib",
				"X3DAudio.lib",
				"xapobase.lib",
				"XAPOFX.lib"
			}
			);
	}
}

