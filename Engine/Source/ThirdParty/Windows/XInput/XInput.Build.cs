// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class XInput : ModuleRules
{
	public XInput(TargetInfo Target)
	{
		Type = ModuleType.External;

// @ATG_CHANGE : BEGIN UWP support
        string DirectXSDKDir = WindowsPlatform.ShouldUseWindowsSDK10(Target.Platform) ?
            UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DirectXLegacy" :
			UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DirectX";

		PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");
		if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
		{ 
			// Ensure correct include and link paths for xinput so the correct dll is loaded (xinput1_3.dll)
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x64");
				PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x64/Win7");
			}
			else if (Target.Platform == UnrealTargetPlatform.Win32)
			{
				PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x86");
				PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x86/Win7");
			}
			PublicAdditionalLibraries.Add("XInput.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.UWP64 || Target.Platform == UnrealTargetPlatform.UWP32)
		{
			PublicAdditionalLibraries.Add("xinputuap.lib");
		}
// @ATG_CHANGE : END
	}
}

