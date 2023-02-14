// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class libcurl : ModuleRules
{
	public libcurl(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicDefinitions.Add("WITH_LIBCURL=1");

		string LinuxLibCurlPath = Target.UEThirdPartySourceDirectory + "libcurl/7_65_3/";
		string WinLibCurlPath = Target.UEThirdPartySourceDirectory + "libcurl/curl-7.55.1/";
		string AndroidLibCurlPath = Target.UEThirdPartySourceDirectory + "libcurl/7_75_0/";

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string platform = "/Linux/" + Target.Architecture;
			string IncludePath = LinuxLibCurlPath + "include" + platform;
			string LibraryPath = LinuxLibCurlPath + "lib" + platform;

			PublicIncludePaths.Add(IncludePath);
			PublicAdditionalLibraries.Add(LibraryPath + "/libcurl.a");

			PrivateDependencyModuleNames.Add("SSL");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			string[] Architectures = new string[] {
				"ARMv7",
				"ARM64",
				"x86",
				"x64",
			};
 
			PublicIncludePaths.Add(AndroidLibCurlPath + "include/Android/");
			foreach(var Architecture in Architectures)
			{
				PublicAdditionalLibraries.Add(AndroidLibCurlPath + "lib/Android/" + Architecture + "/libcurl.a");
			}
		}
// @ATG_CHANGE : BEGIN UWP support
		else if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.HoloLens  //|| Target.Platform == UnrealTargetPlatform.UWP32 || Target.Platform == UnrealTargetPlatform.UWP64
                                                                                                                                                                     )
// @ATG_CHANGE : END
		{
			PublicIncludePaths.Add(WinLibCurlPath + "include/" + Target.Platform.ToString() +  "/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
			string LibDir = WinLibCurlPath + "lib/" + Target.Platform.ToString() +  "/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
			PublicAdditionalLibraries.Add(LibDir + "libcurl_a.lib");
			PublicDefinitions.Add("CURL_STATICLIB=1");

			// Our build requires OpenSSL and zlib, so ensure thye're linked in
			AddEngineThirdPartyPrivateStaticDependencies(Target, new string[]
			{
				"OpenSSL",
				"zlib"
			});
		}else if(Target.Platform == UnrealTargetPlatform.UWP32 || Target.Platform == UnrealTargetPlatform.UWP64)
		{

			

			PublicIncludePaths.Add(WinLibCurlPath + "include/" + "Win64" + "/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
			string LibDir = WinLibCurlPath + "lib/" + "Win64" + "/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
			PublicAdditionalLibraries.Add(LibDir + "libcurl_a.lib");
			PublicDefinitions.Add("CURL_STATICLIB=1");

			// Our build requires OpenSSL and zlib, so ensure thye're linked in
			AddEngineThirdPartyPrivateStaticDependencies(Target, new string[]
			{
				"OpenSSL",
				"zlib"
			});

		}

	}
}
