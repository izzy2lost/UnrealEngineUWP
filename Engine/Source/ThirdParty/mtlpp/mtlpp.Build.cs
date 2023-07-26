// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class MTLPP : ModuleRules
{
	public MTLPP(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string MTLPPIncPath = System.IO.Path.Combine(ModuleDirectory, "mtlpp-master-7efad47") + "/";
		string MTLPPLibPath = System.IO.Path.Combine(PlatformModuleDirectory, "mtlpp-master-7efad47") + "/";

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Apple))
		{
			string PlatformName = PlatformSubdirectoryName;
			string LibExt = (Target.IsInPlatformGroup(UnrealPlatformGroup.IOS) && Target.Architecture == UnrealArch.IOSSimulator) ? ".sim.a" : ".a";
		
			PublicSystemIncludePaths.Add(MTLPPIncPath + "src");
			PublicSystemIncludePaths.Add(MTLPPIncPath + "interpose");
			
			// A full debug build without any optimisation and validation code enabled
			if (Target.Configuration == UnrealTargetConfiguration.Debug)
			{
				if (Target.bDebugBuildsActuallyUseDebugCRT)
				{
					// Use debug mtlpp library
					PublicAdditionalLibraries.Add(MTLPPLibPath + "lib/" + PlatformName + "/libmtlppd" + LibExt);
				}
				else
				{
					// Use development mtlpp library
					PublicAdditionalLibraries.Add(MTLPPLibPath + "lib/" + PlatformName + "/libmtlpp" + LibExt);
				}
			}
			// A development build that uses mtlpp compiled for release but with validation code enabled 
			else if (Target.Configuration == UnrealTargetConfiguration.Development || Target.Configuration == UnrealTargetConfiguration.DebugGame)
			{
				PublicAdditionalLibraries.Add(MTLPPLibPath + "lib/" + PlatformName + "/libmtlpp" + LibExt);
			}
			// A shipping configuration that disables all validation and is aggressively optimised.
			else
			{
				PublicDefinitions.Add("MTLPP_CONFIG_VALIDATE=0"); // Disables the mtlpp validation used for reporting resource misuse which is compiled out for test/shipping
				PublicAdditionalLibraries.Add(MTLPPLibPath + "lib/" + PlatformName + "/libmtlpps" + LibExt);
			}
		}
    }
}
