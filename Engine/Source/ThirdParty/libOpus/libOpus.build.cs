// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class libOpus : ModuleRules
{
	public libOpus(TargetInfo Target)
	{
		/** Mark the current version of the library */
		string OpusVersion = "1.1";
		Type = ModuleType.External;

		PublicIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "libOpus/opus-" + OpusVersion + "/include");
		string LibraryPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "libOpus/opus-" + OpusVersion + "/";

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
// @ATG_CHANGE : BEGIN UWP support
			(Target.Platform == UnrealTargetPlatform.Win32) ||
			(Target.Platform == UnrealTargetPlatform.UWP32) ||
			(Target.Platform == UnrealTargetPlatform.UWP64))
		{
			// ATG - it appears that the 2013-built version of this dependency is not part of the normal enlistment
			LibraryPath += "win32/VS" + (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 ? "2015" : "2012");
			if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.UWP64)
			{
				LibraryPath += "/x64/";
			}
			else
			{
				LibraryPath += "/win32/";
			}
// @ATG_CHANGE : END

			LibraryPath += "Release/";

			PublicLibraryPaths.Add(LibraryPath);

 			PublicAdditionalLibraries.Add("silk_common.lib");
 			PublicAdditionalLibraries.Add("silk_float.lib");
 			PublicAdditionalLibraries.Add("celt.lib");
			PublicAdditionalLibraries.Add("opus.lib");
			PublicAdditionalLibraries.Add("speex_resampler.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string OpusPath = LibraryPath + "/Mac/libopus.a";
			string SpeexPath = LibraryPath + "/Mac/libspeex_resampler.a";

			PublicAdditionalLibraries.Add(OpusPath);
			PublicAdditionalLibraries.Add(SpeexPath);
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
            if (Target.IsMonolithic)
            {
                PublicAdditionalLibraries.Add(LibraryPath + "Linux/" + Target.Architecture + "/libopus.a");
            }
            else
            {
                PublicAdditionalLibraries.Add(LibraryPath + "Linux/" + Target.Architecture + "/libopus_fPIC.a");
            }
		}
	}
}
