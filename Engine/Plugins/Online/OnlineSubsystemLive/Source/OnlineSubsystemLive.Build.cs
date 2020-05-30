// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineSubsystemLive : ModuleRules
{
	// Should match versions in GetUWPDependencies.ps1
	readonly string XsapiVersionUwp = "2017.11.20171204.001";
	readonly string XsapiVersionXboxOne = "2017.11.20171204.001";
	readonly string XimVersion = "1706.8.0";
	readonly string CppRestVersion = "2_9";

	static bool HasWarnedAboutLiveSdk = false;

	public OnlineSubsystemLive(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDefinitions.Add("ONLINESUBSYSTEMLIVE_PACKAGE=1");
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// @ATG_CHANGE : BEGIN XSAPI (decoupled from XDK) lives inside the OSSLive plugin.
		// Use alternate MS implementation of social features that leverages
		// XSAPI manager type to limit service calls and extend feature set.
		//PublicDefinitions.Add("USE_SOCIAL_MANAGER=1");

		if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			// We need etwplus.lib for events, leader boards and achievements
			PublicAdditionalLibraries.Add("etwplus.lib");
			PublicDefinitions.Add("WITH_GAME_CHAT=1");
			PublicDefinitions.Add("WITH_MARKETPLACE=1");
		}

		// Modules our Privates require
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine",
				"Sockets",
				"OnlineSubsystemUtils",
				"Voice",
				"Projects",
				"HTTP"
			}
			);

		// Modules our Publics require
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"OnlineSubsystem",
                "HTTP",
            }
            );

		string WinMDReferencePathRoot = Path.Combine(ModuleDirectory, "..", "ThirdParty");
		string RuntimeDependencyPathRoot = Path.Combine("$(PluginDir)", "ThirdParty");

		// XSAPI package names are long enough that a shortened symbolic link is set up in GetXboxLiveSDK.bat
		string XsapiPackageFolder = string.Empty;
		string XimPackageFolder = string.Empty;
		string PackageArch = string.Empty;
		string PlatformArchAndCompilerPathChunk = string.Empty;

		if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			XsapiPackageFolder = "XboxOne." + XsapiVersionXboxOne;
			XimPackageFolder = "XboxOne." + XimVersion;
			PackageArch = "Durango";
			PlatformArchAndCompilerPathChunk = Path.Combine("references", PackageArch, "v110");
		}
		// This case is currently used for intellisense generation.  Include UWP32 so it can find the winmd
		else if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.UWP32)
		{
			XsapiPackageFolder = "UWP." + XsapiVersionUwp;
			XimPackageFolder = "UWP." + XimVersion;
			PackageArch = "Win32";
			PlatformArchAndCompilerPathChunk = Path.Combine("lib", PackageArch, "v140");
		}
		// This case is currently used for intellisense generation.  Include UWP64 so it can find the winmd
		else if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.UWP64)
		{
			XsapiPackageFolder = "UWP." + XsapiVersionUwp;
			XimPackageFolder = "UWP." + XimVersion;
			PackageArch = "x64";
			PlatformArchAndCompilerPathChunk = Path.Combine("lib", PackageArch, "v140");
		}

		string NugetPathChunk = Path.Combine(XsapiPackageFolder, PlatformArchAndCompilerPathChunk, "release");
		string XSAPISubDir = Path.Combine("XSAPI", NugetPathChunk);
		if (!AddWinRTDllReference(XSAPISubDir, "Microsoft.Xbox.Services"))
		{
			if (!HasWarnedAboutLiveSdk)
			{
				Tools.DotNETCommon.Log.TraceWarning(" Xbox Live SDK (version {0}) not found.  Xbox Live features will not be available.  Run Setup.bat to ensure the SDK is in the expected location.", Target.Platform == UnrealTargetPlatform.XboxOne ? XsapiVersionXboxOne : XsapiVersionUwp);
				HasWarnedAboutLiveSdk = true;
			}
        }

		string PlatformSubDir = string.Empty;
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// This is currently used for intellisense generation.  Fall-through to UWP64 so it can find the winmd
			PlatformSubDir = UnrealTargetPlatform.UWP64.ToString();
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PlatformSubDir = UnrealTargetPlatform.UWP32.ToString();
		}
		else
		{
			PlatformSubDir = Target.Platform.ToString();
		}

		bool HasGameChat = true;
		string GameChatSubDir = Path.Combine("GameChat", "Binaries", PlatformSubDir);
		HasGameChat = HasGameChat && AddWinRTDllReference(GameChatSubDir, "Microsoft.Xbox.GameChat");
		HasGameChat = HasGameChat && AddWinRTDllReference(GameChatSubDir, "Microsoft.Xbox.ChatAudio");
		PublicDefinitions.Add(string.Format("WITH_GAME_CHAT={0}", HasGameChat ? 1 : 0));

		string EraAdapterSubDir = Path.Combine("EraAdapter", "Binaries", PlatformSubDir);
		AddWinRTDllReference(EraAdapterSubDir, "EraAdapter");

        if (Target.Platform != UnrealTargetPlatform.XboxOne)
        {
            // CppRest a little different - it's not a WinRT component, and it will need to be loaded explicitly
            string CppRestDll = Path.Combine("ThirdParty", XSAPISubDir, string.Format("cpprest140_uwp_{0}.dll", CppRestVersion));
			RuntimeDependencies.Add(Path.Combine("$(PluginDir)", CppRestDll));
			PublicDefinitions.Add(string.Format(@"CPP_REST_DLL=TEXT(""{0}"")", CppRestDll.Replace(@"\", "/")));

			if (Target.UWPPlatform.Win10SDKVersion >= new System.Version(10, 0, 14393, 0))
			{
				PublicDefinitions.Add("WITH_MARKETPLACE=1");
				PrivateWinMDReferences.Add("Windows.Services.Store.StoreContract");
			}
			else
			{
				PublicDefinitions.Add("WITH_MARKETPLACE=0");
			}
		}

		if (UseXim(Target))
		{
			PublicDefinitions.Add("USE_XIM=1");

			PublicIncludePaths.Add(Path.Combine(WinMDReferencePathRoot, "XIM", XimPackageFolder, "include"));
			PublicLibraryPaths.Add(Path.Combine(WinMDReferencePathRoot, "XIM", XimPackageFolder, "lib", PackageArch, "release"));
			PublicAdditionalLibraries.Add("xboxintegratedmultiplayer.lib");

			// Xim DLL is like cpprest.
			string XimDll = Path.Combine("ThirdParty", "XIM", XimPackageFolder, "lib", PackageArch, "release", "XboxIntegratedMultiplayer.dll");
			RuntimeDependencies.Add(Path.Combine("$(PluginDir)", XimDll));
			PublicDelayLoadDLLs.Add("XboxIntegratedMultiplayer.dll");

			PublicDefinitions.Add(string.Format(@"XIM_DLL=TEXT(""{0}"")", XimDll.Replace(@"\", "/")));
		}
		else
		{
			PublicDefinitions.Add("USE_XIM=0");
		}

        PublicDefinitions.Add(string.Format(@"USE_ACHIEVEMENTS_2017={0}", UseAchievements2017(Target) ? 1 : 0));
        PublicDefinitions.Add(string.Format(@"USE_STATS_2017={0}", UseStats2017(Target) ? 1 : 0));
        // @ATG_CHANGE : END
    }

	// @ATG_CHANGE : BEGIN XSAPI (decoupled from XDK) lives inside the OSSLive plugin.
	private bool AddWinRTDllReference(string SubDir, string BaseFileName)
	{
		string WinMDPath = Path.Combine(ModuleDirectory, "..", "ThirdParty", SubDir, BaseFileName + ".winmd");
		if (!File.Exists(WinMDPath))
		{
			return false;
		}

		PublicWinMDReferences.Add(WinMDPath);
		RuntimeDependencies.Add(Path.Combine("$(PluginDir)", "ThirdParty", SubDir, BaseFileName + ".dll"));
		return true;
	}
	// @ATG_CHANGE : END 

	// @ATG_CHANGE : BEGIN XIM toggle
	private bool UseXim(ReadOnlyTargetRules Target)
	{
		if (Target.Platform == UnrealTargetPlatform.UWP64 || Target.Platform == UnrealTargetPlatform.UWP32)
			return Target.UWPPlatform.bUseXim;

		return false;
	}
	// @ATG_CHANGE : END XIM toggle

	// @ATG_CHANGE : BEGIN Achievements 2017 toggle
	private bool UseAchievements2017(ReadOnlyTargetRules Target)
	{
		if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			//return Target.XboxOnePlatform.bUseAchievements2017;
			return false;
		}

		if (Target.Platform == UnrealTargetPlatform.UWP64 || Target.Platform == UnrealTargetPlatform.UWP32)
		{
			return Target.UWPPlatform.bUseAchievements2017;
		}

		return false;
	}
	// @ATG_CHANGE : END Achievements 2017 toggle

	// @ATG_CHANGE : BEGIN Stats 2017 toggle
	private bool UseStats2017(ReadOnlyTargetRules Target)
	{
		if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			//return Target.XboxOnePlatform.bUseStats2017;
			return false;
		}

		if (Target.Platform == UnrealTargetPlatform.UWP64 || Target.Platform == UnrealTargetPlatform.UWP32)
		{
			return Target.UWPPlatform.bUseStats2017;
		}

		return false;
	}
	// @ATG_CHANGE : END Stats 2017 toggle

}
