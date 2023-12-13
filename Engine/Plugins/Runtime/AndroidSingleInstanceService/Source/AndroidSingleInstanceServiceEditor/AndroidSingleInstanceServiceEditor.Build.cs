// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AndroidSingleInstanceServiceEditor : ModuleRules
	{
		public AndroidSingleInstanceServiceEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			ShortName = "AndSngInstSvcEd";

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"UnrealEd",
				});
		}
	}
}
