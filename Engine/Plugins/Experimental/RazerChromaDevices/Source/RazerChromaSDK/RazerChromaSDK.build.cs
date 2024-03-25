// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class RazerChromaSDK : ModuleRules
	{
		public RazerChromaSDK(ReadOnlyTargetRules Target) : base(Target)
		{
			Type = ModuleType.External;
			
			// TODO: We may want to suport other platforms in the future as well
			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
			{
				// Add the third party include folders so that we can use Razer types
				PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty"));
				
				if (Target.Architecture.bIsX64)
				{
					// Redist the CChromaEditorLibrary64.dll with the game
					string DllPath = Path.GetFullPath(Path.Combine(PluginDirectory, "Binaries/ThirdParty/Win64"));

					RuntimeDependencies.Add(Path.Combine(DllPath, "CChromaEditorLibrary64.dll"));

					// We only want the debug symbols outside of shipping
					if (Target.Configuration != UnrealTargetConfiguration.Shipping)
					{
						RuntimeDependencies.Add(Path.Combine(DllPath, "CChromaEditorLibrary64.pdb"));
					}					
				}
				else
				{
					// Redist the CChromaEditorLibrary.dll with the game
					string DllPath = Path.GetFullPath(Path.Combine(PluginDirectory, "Binaries/ThirdParty/Win32"));

					RuntimeDependencies.Add(Path.Combine(DllPath, "CChromaEditorLibrary.dll"));

					// We only want the debug symbols outside of shipping
					if (Target.Configuration != UnrealTargetConfiguration.Shipping)
					{
						RuntimeDependencies.Add(Path.Combine(DllPath, "CChromaEditorLibrary.pdb"));
					}						
				}
			}
		}
	}
}
