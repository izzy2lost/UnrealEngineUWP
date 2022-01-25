// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class XInput : ModuleRules
{
	public XInput(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string DirectXSDKDir = "";
// @UWP_CHANGE : BEGIN UWP support
		if (Target.Platform == UnrealTargetPlatform.HoloLens ||
			Target.Platform == UnrealTargetPlatform.UWP32 ||
			Target.Platform == UnrealTargetPlatform.UWP64)
// @UWP_CHANGE : END
		{
			DirectXSDKDir = Target.WindowsPlatform.bUseWindowsSDK10 ?
			Target.UEThirdPartySourceDirectory + "Windows/DirectXLegacy" :
			Target.UEThirdPartySourceDirectory + "Windows/DirectX";
		}
		else
		{
			DirectXSDKDir = Target.UEThirdPartySourceDirectory + "Windows/DirectX";
		}

		// Ensure correct include and link paths for xinput so the correct dll is loaded (xinput1_3.dll)
		PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");
		
		// @UWP_CHANGE : BEGIN UWP support
		if (Target.Platform == UnrealTargetPlatform.HoloLens || Target.Platform == UnrealTargetPlatform.UWP64 || Target.Platform == UnrealTargetPlatform.UWP32)
		// @UWP_CHANGE : END
		{
			PublicSystemLibraries.Add("xinputuap.lib");
		}
		else
		{
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicAdditionalLibraries.Add(DirectXSDKDir + "/Lib/x64/XInput.lib");
			}
			else if (Target.Platform == UnrealTargetPlatform.Win32)
			{
				PublicAdditionalLibraries.Add(DirectXSDKDir + "/Lib/x86/XInput.lib");
			}
		}
	}
}

