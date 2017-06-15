// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX11 : ModuleRules
{
	public DX11(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

        // @ATG_CHANGE : BEGIN UWP support
        string DirectXSDKDir = Target.WindowsPlatform.bUseWindowsSDK10 ?
            UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DirectXLegacy" :
			UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DirectX";
		// @ATG_CHANGE : END 
        PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			Definitions.Add("WITH_D3DX_LIBS=1");

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x64");
			}
			else if (Target.Platform == UnrealTargetPlatform.Win32)
			{
				PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x86");
			}

			PublicAdditionalLibraries.AddRange(
				new string[] {
				"dxgi.lib",
				"d3d9.lib",
				"d3d11.lib",
				"dxguid.lib",
				"d3dcompiler.lib",
				(Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ? "d3dx11d.lib" : "d3dx11.lib",
				"dinput8.lib",
				}
				);
	        // @ATG_CHANGE : BEGIN DX SDK lib isolation clean up
			// Preserved for consistency with original version, but definitely not needed when using Win10 SDK
			if (!Target.WindowsPlatform.bUseWindowsSDK10)
			{
				PublicAdditionalLibraries.AddRange(
					new string[]
					{
						"X3DAudio.lib",
						"xapobase.lib",
						"XAPOFX.lib"
					}
					);
			}
			// @ATG_CHANGE : END
		}
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			Definitions.Add("WITH_D3DX_LIBS=0");
		}
		// @ATG_CHANGE : BEGIN UWP support
		else if (Target.Platform == UnrealTargetPlatform.UWP64 || Target.Platform == UnrealTargetPlatform.UWP32)
		{
			Definitions.Add("WITH_D3DX_LIBS=0");
			PublicAdditionalLibraries.AddRange(
				new string[] {
				"dxguid.lib",
				}
				);
		}
		// @ATG_CHANGE : END 

		}
}

