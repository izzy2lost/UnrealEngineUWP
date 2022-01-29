// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX11 : ModuleRules
{
	public DX11(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string DirectXSDKDir = "";
// @EMMETTJNR_CHANGE : BEGIN UWP support
		if (Target.Platform == UnrealTargetPlatform.HoloLens ||
			Target.Platform == UnrealTargetPlatform.UWP32 ||
			Target.Platform == UnrealTargetPlatform.UWP64)
// @EMMETTJNR_CHANGE : END
		{
			DirectXSDKDir = Target.WindowsPlatform.bUseWindowsSDK10 ?
			Target.UEThirdPartySourceDirectory + "Windows/DirectXLegacy" :
			Target.UEThirdPartySourceDirectory + "Windows/DirectX";
		}
		else
		{
			DirectXSDKDir = Target.UEThirdPartySourceDirectory + "Windows/DirectX";
		}

		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicSystemIncludePaths.Add(DirectXSDKDir + "/Include");

			PublicDefinitions.Add("WITH_D3DX_LIBS=1");

			string LibDir = null;
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				LibDir = DirectXSDKDir + "/Lib/x64/";
			}
			else if (Target.Platform == UnrealTargetPlatform.Win32)
			{
				LibDir = DirectXSDKDir + "/Lib/x86/";
			}

			PublicAdditionalLibraries.AddRange(
				new string[] {
					LibDir + "dxgi.lib",
					LibDir + "d3d9.lib",
					LibDir + "d3d11.lib",
					LibDir + "dxguid.lib",
					LibDir + "d3dcompiler.lib",
					(Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? LibDir + "d3dx11d.lib" : LibDir + "d3dx11.lib",
					LibDir + "dinput8.lib",
				}
				);
	        	// @ATG_CHANGE : BEGIN DX SDK lib isolation clean up
			// Preserved for consistency with original version, but definitely not needed when using Win10 SDK
			if (!Target.WindowsPlatform.bUseWindowsSDK10)
			{
				PublicAdditionalLibraries.AddRange(
					new string[]
					{
				LibDir + "X3DAudio.lib",
				LibDir + "xapobase.lib",
				LibDir + "XAPOFX.lib"
				}
				);
			}
			// @ATG_CHANGE : END
		}
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			PublicDefinitions.Add("WITH_D3DX_LIBS=0");
		}

// UWP_CHANGE : BEGIN UWP support
		else if (Target.Platform == UnrealTargetPlatform.HoloLens ||
			Target.Platform == UnrealTargetPlatform.UWP32 ||
			Target.Platform == UnrealTargetPlatform.UWP64)
// UWP_CHANGE : END
		{
			PublicSystemIncludePaths.Add(DirectXSDKDir + "/Include");

			PublicDefinitions.Add("WITH_D3DX_LIBS=0");
			PublicSystemLibraries.AddRange(
				new string[] {
				"dxguid.lib",
				}
				);
		}

	}
}

