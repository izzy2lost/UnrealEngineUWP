// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class CADKernel : ModuleRules
	{
		public CADKernel(ReadOnlyTargetRules Target)
			: base(Target)
		{
			//OptimizeCode = CodeOptimization.Never;
			DeterministicWarningLevel = WarningLevel.Off; // __DATE__ in Private/CADKernel/Core/System.cpp
			
			PublicDefinitions.Add("CADKERNEL_THINZONE=0");

			if (Target.bIsBuildingConsoleApplication)
			{
				PublicDefinitions.Add("DO_ENSURE=0");
			}

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"GeometryCore",
				}
			);
		}
	}
}