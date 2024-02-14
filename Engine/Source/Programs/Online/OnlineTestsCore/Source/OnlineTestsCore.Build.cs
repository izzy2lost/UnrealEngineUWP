// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System;
using System.Collections.Generic;
using System.Linq;

public class OnlineTestsCore : ModuleRules
{
	public virtual bool bRequireApplicationTick { get { return false; } }
	public virtual bool bRequirePlatformInit { get { return false; } }
	public virtual string PlatformFileName { get { return ""; } }
	public virtual bool bUseExternAuth { get { return false; } }

	public OnlineTestsCore(ReadOnlyTargetRules Target) : base(Target)
	{
		bTreatAsEngineModule = false;
		CppStandard = CppStandardVersion.EngineDefault;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"ApplicationCore",
				"Core",
				"CoreUObject",
				"Projects",
				"EngineSettings",
				"OnlineSubsystem",
				"OnlineServicesInterface",
				"OnlineServicesCommon",
				"OnlineServicesEOS",
				"OnlineServicesNull",
				"OnlineServicesOSSAdapter",
				"EOSSDK",
				"EOSShared",
				"SSL",
				"Json",
				"JsonUtilities"
			}
		);

		PublicDependencyModuleNames.AddRange(
		new string[]
		{
			"OnlineSubsystem"
		});

		PublicDefinitions.Add(String.Format("ONLINETESTS_REQUIREAPPLICATIONTICK={0}", bRequireApplicationTick ? 1 : 0));
		PublicDefinitions.Add(String.Format("ONLINETESTS_REQUIREPLATFORMINIT={0}", bRequirePlatformInit ? 1 : 0));
		PublicDefinitions.Add(String.Format("ONLINETESTS_PLATFORMFILENAME={0}", PlatformFileName));

		// Disable external auth if target doesn't define it.
		if (!Target.GlobalDefinitions.Contains("ONLINETESTS_USEEXTERNAUTH=1"))
		{
			PublicDefinitions.Add(String.Format("ONLINETESTS_USEEXTERNAUTH=0"));
		}
	}
}


