// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IoStoreOnDemand : ModuleRules
{
	public IoStoreOnDemand(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.Add("Core");
		PublicDependencyModuleNames.Add("TraceLog");
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"HTTP",
				"Json",
				"Analytics"
			}
		);

		bAllowConfidentialPlatformDefines = true;
		UnsafeTypeCastWarningLevel = WarningLevel.Error; 

		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Desktop) &&
			(Target.Type == TargetType.Editor || Target.Type == TargetType.Program))
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "S3Client", "RSA" });
		}

		bool bFindUcasViaPakFileModule = false;

		if (bFindUcasViaPakFileModule)
		{
			PrivateDependencyModuleNames.Add("PakFile");
			PublicDefinitions.Add("UE_IAS_LINKPAKFILE=1");
		}
		else
		{
			PublicDefinitions.Add("UE_IAS_LINKPAKFILE=0");
		}
	}
}
