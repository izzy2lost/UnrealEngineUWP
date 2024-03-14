// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class NDISDK : ModuleRules
{
    public NDISDK(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

		// The NDI SDK is available for Win64 + Linux, but this plugin only supports Win64
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string IncludePath = Path.Combine(ModuleDirectory, "Include");

            string LibraryName = "Processing.NDI.Lib.x64";
            string ThirdPartyBinaryPath = Path.Combine(ModuleDirectory, "../../../Binaries/ThirdParty");

	        PublicIncludePaths.Add(IncludePath);

            string DllName = Path.Combine(LibraryName, ".dll");
            PublicDelayLoadDLLs.Add(DllName);

            string DllPath = Path.Combine(ThirdPartyBinaryPath, DllName);
            RuntimeDependencies.Add(DllPath);

            // Ensure that we define our c++ define
            PublicDefinitions.Add("NDI_SDK_ENABLED");
        }
    }
}
