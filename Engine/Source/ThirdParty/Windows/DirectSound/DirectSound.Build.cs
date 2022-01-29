// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DirectSound : ModuleRules
{
	public DirectSound(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		
		string DirectXSDKDir = "";
// @EMMETTJNR_CHANGE  : BEGIN
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

		if (LibDir != null)
		{
			PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

			PublicAdditionalLibraries.AddRange(
				new string[] {
					 LibDir + "dxguid.lib",
					 LibDir + "dsound.lib"
				}
			);
		}
	}
}
