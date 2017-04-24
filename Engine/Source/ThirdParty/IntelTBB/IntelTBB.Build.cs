// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IntelTBB : ModuleRules
{
	public IntelTBB(TargetInfo Target)
	{
		Type = ModuleType.External;

// @ATG_CHANGE : BEGIN UWP support
		if ((Target.Platform == UnrealTargetPlatform.Win64) || 
            (Target.Platform == UnrealTargetPlatform.Win32) ||
            (Target.Platform == UnrealTargetPlatform.UWP32) ||
            (Target.Platform == UnrealTargetPlatform.UWP64))
// @ATG_CHANGE : END
		{
			string IntelTBBPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "IntelTBB/";
			switch (WindowsPlatform.Compiler)
			{
				case WindowsCompiler.VisualStudio2017:
				case WindowsCompiler.VisualStudio2015: IntelTBBPath += "IntelTBB-4.4u3/"; break;
				case WindowsCompiler.VisualStudio2013: IntelTBBPath += "IntelTBB-4.0/"; break;
			}

			PublicSystemIncludePaths.Add(IntelTBBPath + "Include");

// @ATG_CHANGE : BEGIN UWP support
			if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.UWP64))
// @ATG_CHANGE : END
			{
				if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 || WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2017)
				{
					PublicLibraryPaths.Add(IntelTBBPath + "lib/Win64/vc14");
				}
				else if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2013)
				{
					PublicLibraryPaths.Add(IntelTBBPath + "lib/Win64/vc12");
				}
			}
// @ATG_CHANGE : BEGIN  UWP support
			else if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.UWP32))
// @ATG_CHANGE : END
			{
				if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 || WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2017)
				{
					PublicLibraryPaths.Add(IntelTBBPath + "lib/Win32/vc14");
				}
				else if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2013)
				{
					PublicLibraryPaths.Add(IntelTBBPath + "lib/Win32/vc12");
				}
			}

			// Disable the #pragma comment(lib, ...) used by default in MallocTBB...
			// We want to explicitly include the library.
			Definitions.Add("__TBBMALLOC_BUILD=1");

			string LibName = "tbbmalloc";
			if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
			{
				LibName += "_debug";
			}
			LibName += ".lib";
			PublicAdditionalLibraries.Add(LibName);
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicSystemIncludePaths.AddRange(
				new string[] {
					UEBuildConfiguration.UEThirdPartySourceDirectory + "IntelTBB/IntelTBB-4.0/include",
				}
			);

			PublicAdditionalLibraries.AddRange(
				new string[] {
					UEBuildConfiguration.UEThirdPartySourceDirectory + "IntelTBB/IntelTBB-4.0/lib/Mac/libtbb.a",
					UEBuildConfiguration.UEThirdPartySourceDirectory + "IntelTBB/IntelTBB-4.0/lib/Mac/libtbbmalloc.a",
				}
			);
		}
	}
}
