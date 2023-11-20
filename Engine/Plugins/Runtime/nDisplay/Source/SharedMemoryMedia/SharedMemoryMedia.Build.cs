// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class SharedMemoryMedia : ModuleRules
	{
		public SharedMemoryMedia(ReadOnlyTargetRules Target) : base(Target)
		{

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"Media",
					"MediaAssets",
					"MediaIOCore",
					"RenderCore",
					"Renderer",
					"RHI",
				});

			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
				PrivateDependencyModuleNames.Add("D3D12RHI");
			}

		}
	}
}
