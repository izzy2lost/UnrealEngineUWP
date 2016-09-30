// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class libcurl : ModuleRules
{
	public libcurl(TargetInfo Target)
	{
		Type = ModuleType.External;

		Definitions.Add("WITH_LIBCURL=1");
		string LibCurlPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "libcurl/";
		if (Target.Platform == UnrealTargetPlatform.Linux)
        {
			PublicIncludePaths.Add(LibCurlPath + "include/Linux/" + Target.Architecture);
			PublicLibraryPaths.Add(LibCurlPath + "lib/Linux/" + Target.Architecture);
            PublicAdditionalLibraries.Add("curl");
            PublicAdditionalLibraries.Add("crypto");
            PublicAdditionalLibraries.Add("ssl");
            PublicAdditionalLibraries.Add("dl");
        }
        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
			// toolchain will filter properly
            PublicIncludePaths.Add(LibCurlPath + "include/Android/ARMv7");
            PublicLibraryPaths.Add(LibCurlPath + "lib/Android/ARMv7");
            PublicIncludePaths.Add(LibCurlPath + "include/Android/ARM64");
            PublicLibraryPaths.Add(LibCurlPath + "lib/Android/ARM64");
            PublicIncludePaths.Add(LibCurlPath + "include/Android/x86");
            PublicLibraryPaths.Add(LibCurlPath + "lib/Android/x86");
            PublicIncludePaths.Add(LibCurlPath + "include/Android/x64");
            PublicLibraryPaths.Add(LibCurlPath + "lib/Android/x64");

			PublicAdditionalLibraries.Add("curl");
//            PublicAdditionalLibraries.Add("crypto");
//            PublicAdditionalLibraries.Add("ssl");
//            PublicAdditionalLibraries.Add("dl");
        }
        else if (Target.Platform == UnrealTargetPlatform.Win32 ||
// @ATG_CHANGE : BEGIN UWP support
            Target.Platform == UnrealTargetPlatform.Win64 ||
            Target.Platform == UnrealTargetPlatform.UWP32 ||
            Target.Platform == UnrealTargetPlatform.UWP64 || 
            (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32"))
// @ATG_CHANGE : END
		{
            // @todo: If this pathing is changed, it must be updated to match in UEBuildWindows.cs so Windows XP can override.
			PublicIncludePaths.Add(LibCurlPath + "include/Windows");

			string LibCurlLibPath = LibCurlPath + "lib/";
// @ATG_CHANGE : BEGIN UWP support
			LibCurlLibPath += Target.Platform.ToString() + "/";
// @ATG_CHANGE : END
			LibCurlLibPath += "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(LibCurlLibPath);

			PublicAdditionalLibraries.Add("libcurl_a.lib");
			Definitions.Add("CURL_STATICLIB=1");
		}
	}
}
