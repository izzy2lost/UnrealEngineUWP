// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using EpicGames.Core;

namespace UnrealBuildTool.Rules
{
	public class PixelStreaming2Blueprint : ModuleRules
	{
		public PixelStreaming2Blueprint(ReadOnlyTargetRules Target) : base(Target)
		{
			//PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
			//PrivatePCHHeaderFile = "Private/PCH.h";

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
					"Engine",
					"PixelStreaming2"
				});
		}
	}
}
