// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System;

public class VTCodecsRHI : ModuleRules
{
    public VTCodecsRHI(ReadOnlyTargetRules Target) : base(Target)
    {
        bLegacyPublicIncludePaths = false;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Engine",
            "AVCodecsCore",
            "VTCodecs",
            "RHI"
        });

        PublicDependencyModuleNames.AddRange(new string[] {
            "RenderCore",
            "Core",
            "CoreUObject",
        });
        
        PublicFrameworks.AddRange(new string[]{
            "AVFoundation",
            "VideoToolbox"
        });
    }
}
