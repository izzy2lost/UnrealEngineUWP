// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using EpicGames.Core;

namespace UnrealBuildTool.Rules
{
	public class PixelStreaming2BlueprintEditor : ModuleRules
	{
		public PixelStreaming2BlueprintEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

			PublicIncludePaths.AddRange(
				new string[] {
				});

			PrivateIncludePaths.AddRange(
				new string[] {
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"SlateCore",
					"PixelStreaming2Core",
					"PixelStreaming2",
					"PixelStreaming2Blueprint",
					"PixelStreaming2Editor",
					"UnrealEd"
				});
		}
	}
}
