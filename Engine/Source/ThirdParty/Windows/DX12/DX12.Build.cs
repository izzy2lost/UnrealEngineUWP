// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class DX12 : ModuleRules
{
	public DX12(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// @ATG_CHANGE : BEGIN UWP support - using the flattened "DirectX" folder that has some conflicting legacy items is an issue when consuming W10 SDK
		string DirectXSDKDir = UEBuildConfiguration.UEThirdPartySourceDirectory + (WindowsPlatform.bUseWindowsSDK10 ? "Windows/DX12" : "Windows/DirectX");
		if (WindowsPlatform.bUseWindowsSDK10 && (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.UWP64))
		{
			PublicSystemIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "/Windows/Pix/Include");
			PublicLibraryPaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "/Windows/Pix/Lib/x64");
		}
		// @ATG_CHANGE : END
		PublicSystemIncludePaths.Add(DirectXSDKDir + "/include");

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
		// @ATG_CHANGE : BEGIN UWP support
		else if (Target.Platform == UnrealTargetPlatform.UWP64 && Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PublicDelayLoadDLLs.Add("WinPixEventRuntime.dll");
			PublicAdditionalLibraries.Add("WinPixEventRuntime.lib");
			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Windows/DirectX/x64/WinPixEventRuntime.dll"));
		}
		// @ATG_CHANGE : END

		// Always delay-load D3D12
		PublicDelayLoadDLLs.AddRange(new string[] {
			"d3d12.dll"
			});

		PublicAdditionalLibraries.AddRange(
			new string[] {
				"d3d12.lib"
			}
			);
	}
}

