//*********************************************************
// Copyright (c) Microsoft. All rights reserved.
//*********************************************************
using UnrealBuildTool;

public class UWPMoviePlayer : ModuleRules
{
	public UWPMoviePlayer(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
					"Core",
					"MoviePlayer",
					"RenderCore",
					"RHI",
					"SlateCore",
					"Slate"
			}
			);
	}
}