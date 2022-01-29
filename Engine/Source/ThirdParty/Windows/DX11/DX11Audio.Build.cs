// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX11Audio : ModuleRules
{
	public DX11Audio(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string DirectXSDKDir = "";
// @EMMETTJNR_CHANGE : BEGIN
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

		string LibDir = null;
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			LibDir = DirectXSDKDir + "/Lib/x64/";
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			LibDir = DirectXSDKDir + "/Lib/x86/";
		}

		if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

			PublicAdditionalLibraries.AddRange(
			new string[] 
			{
				LibDir + "dxguid.lib",
				LibDir + "X3DAudio.lib",
				LibDir + "xapobase.lib",
				LibDir + "XAPOFX.lib"
			}
			);
		}
// @EMMETTJNR_CHANGE : BEGIN UWP support
		else if (Target.Platform == UnrealTargetPlatform.HoloLens ||
			Target.Platform == UnrealTargetPlatform.UWP32 ||
			Target.Platform == UnrealTargetPlatform.UWP64)
// @EMMETTJNR_CHANGE : END
		{
			PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

            PublicSystemLibraries.AddRange(
			new string[]
			{
				LibDir + "dxguid.lib",
				LibDir + "xapobase.lib"
			}
			);
		}
	}
}

