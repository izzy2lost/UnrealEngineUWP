// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class NNERuntimeRDG : ModuleRules
{
	public NNERuntimeRDG(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp17;

		PublicDependencyModuleNames.AddRange(new string[] 
		{ 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore",
			"RenderCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"NNE",
			"NNEHlslShaders",
			"RHI",
			"Projects",
			"TraceLog"
		});

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{	
			PrivateDependencyModuleNames.Add("MetalRHI");
		}

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{	
			PrivateDependencyModuleNames.Add("VulkanRHI");
		}

		if ((Target.Type == TargetType.Editor || Target.Type == TargetType.Program) &&
			(Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.Mac)
			)
		{
			PrivateDefinitions.Add("NNE_UTILITIES_AVAILABLE");
			PrivateDependencyModuleNames.Add("NNERuntimeRDGUtils");
		}

		PublicDefinitions.Add("NNERUNTIMERDGHLSL_BUFFER_LENGTH_ALIGNMENT=4");
	}
}
