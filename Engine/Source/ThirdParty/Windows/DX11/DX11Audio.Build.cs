// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX11Audio : ModuleRules
{
	public DX11Audio(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

        // @ATG_CHANGE : BEGIN UWP Support
		string DirectXSDKDir = Target.WindowsPlatform.bUseWindowsSDK10 ?
			UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DirectXLegacy" :
			UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DirectX";        
        // @ATG_CHANGE : END

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
		if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
		{
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
}

