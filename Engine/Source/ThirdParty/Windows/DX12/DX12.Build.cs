// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX12 : ModuleRules
{
	public DX12(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// @ATG_CHANGE : BEGIN UWP support
		string DirectXSDKDir = Target.WindowsPlatform.bUseWindowsSDK10 ?
			UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DirectXLegacy" :
			UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DirectX";
		// @ATG_CHANGE : END

		// For d3dx12, which is not part of the SDK distribution
		PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");


// @ATG_CHANGE : BEGIN UWP support
		if (Target.WindowsPlatform.bUseWindowsSDK10)
		{
			PublicSystemIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "Windows/DX12/Include");
		}

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x64");

            PublicDelayLoadDLLs.Add("WinPixEventRuntime.dll");
            PublicAdditionalLibraries.Add("WinPixEventRuntime.lib");
            RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Windows/DirectX/x64/WinPixEventRuntime.dll"));
        }
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicLibraryPaths.Add(DirectXSDKDir + "/Lib/x86");
		}
// @ATG_CHANGE : END

		// Always delay-load D3D12
		PublicDelayLoadDLLs.AddRange( new string[] {
			"d3d12.dll"
			} );

		PublicAdditionalLibraries.AddRange(
			new string[] {
                "d3d12.lib"
			}
			);
	}
}

