// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class CADLibrary : ModuleRules
{
	public CADLibrary(ReadOnlyTargetRules Target) : base(Target)
	{
		bLegalToDistributeObjectCode = true;

		if (Target.bIsBuildingConsoleApplication)
		{
			PublicDefinitions.Add("DO_ENSURE=0");
		}

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CADTools",
				"CADKernel",
				"DatasmithCore",
				"DynamicMesh",
				"MeshConversion",
				"MeshDescription",
				"GeometryCore",
				"StaticMeshDescription"
			}
		);
	}
}
