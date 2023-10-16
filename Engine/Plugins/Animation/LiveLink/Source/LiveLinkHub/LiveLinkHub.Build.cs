// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LiveLinkHub : ModuleRules
	{
		public LiveLinkHub(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
			});

			PrivateDependencyModuleNames.AddRange(
			new string[]
			{
			});
		}
	}
}
