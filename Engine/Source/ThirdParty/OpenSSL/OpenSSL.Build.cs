// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OpenSSL : ModuleRules
{
	public OpenSSL(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string OpenSSL111cPath = Path.Combine(Target.UEThirdPartySourceDirectory, "OpenSSL", "1.1.1c");
		string OpenSSL111kPath = Path.Combine(Target.UEThirdPartySourceDirectory, "OpenSSL", "1.1.1k");

		string PlatformSubdir = Target.Platform.ToString();
		string ConfigFolder = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "Debug" : "Release";

		if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicIncludePaths.Add(Path.Combine(OpenSSL111kPath, "include", PlatformSubdir));

			string LibPath = Path.Combine(OpenSSL111kPath, "lib", PlatformSubdir);

			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libssl.a"));
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libcrypto.a"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			string VSVersion = "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();

			// Add includes
			PublicIncludePaths.Add(Path.Combine(OpenSSL111kPath, "include", PlatformSubdir, VSVersion));

			// Add Libs
			string LibPath = Path.Combine(OpenSSL111kPath, "lib", PlatformSubdir, VSVersion, ConfigFolder);

			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libssl.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libcrypto.lib"));
			PublicSystemLibraries.Add("crypt32.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.HoloLens)
		{
			string VSVersion = "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();

			// Add includes
			PublicIncludePaths.Add(Path.Combine(OpenSSL111kPath, "include", PlatformSubdir, VSVersion));

			// Add Libs
			string LibPath = Path.Combine(OpenSSL111kPath, "lib", PlatformSubdir, VSVersion, ConfigFolder);

			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libssl.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libcrypto.lib"));
			PublicSystemLibraries.Add("crypt32.lib");
		}
		// @EMMETTJNR_CHANGE : BEGIN UWP support - OpenSSL 1.0.2 built from https://github.com/microsoft/openssl/tree/OpenSSL_1_0_2_WinRT-stable
		else if (Target.Platform == UnrealTargetPlatform.UWP64 || Target.Platform == UnrealTargetPlatform.UWP32)
        {
			string OpenSSL102UWPPath = Path.Combine(Target.UEThirdPartySourceDirectory, "OpenSSL", "1.0.2");

			string VSVersion = "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();

			// Add includes
			PublicIncludePaths.Add(Path.Combine(OpenSSL102UWPPath, "include", PlatformSubdir, VSVersion));

			// Add Libs
			string LibPath = Path.Combine(OpenSSL102UWPPath, "lib", PlatformSubdir, VSVersion, ConfigFolder);

			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "ssleay32.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libeay32.lib"));
		}
		// @EMMETTJNR_CHANGE : END
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string platform = "/Linux/" + Target.Architecture;
			string IncludePath = OpenSSL111cPath + "/include" + platform;
			string LibraryPath = OpenSSL111cPath + "/lib" + platform;

			PublicIncludePaths.Add(IncludePath);
			PublicAdditionalLibraries.Add(LibraryPath + "/libssl.a");
			PublicAdditionalLibraries.Add(LibraryPath + "/libcrypto.a");

			PublicDependencyModuleNames.Add("zlib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Android || Target.Platform == UnrealTargetPlatform.Lumin)
		{
			string[] Architectures = new string[] {
				"ARMv7",
				"ARM64",
				"x86",
				"x64",
			};

			PublicIncludePaths.Add(OpenSSL111kPath + "/include/Android/");

			foreach(var Architecture in Architectures)
			{
				PublicAdditionalLibraries.Add(OpenSSL111kPath + "/lib/Android/" + Architecture + "/libcrypto.a");
				PublicAdditionalLibraries.Add(OpenSSL111kPath + "/lib/Android/" + Architecture + "/libssl.a");
			}
		}
	}
}
