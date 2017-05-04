// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineSubsystemLive : ModuleRules
{
	public OnlineSubsystemLive(TargetInfo Target)
	{
		// @ATG_CHANGE : BEGIN XSAPI (decoupled from XDK) lives inside the OSSLive plugin.
		Definitions.Add("ONLINESUBSYSTEMLIVE_PACKAGE=1");

		// Use alternate MS implementation of social features that leverages
		// XSAPI manager type to limit service calls and extend feature set.
		Definitions.Add("USE_SOCIAL_MANAGER=1");

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			// We need etwplus.lib for events, leader boards and achievements
			PublicAdditionalLibraries.Add("etwplus.lib");
			Definitions.Add("WITH_GAME_CHAT=1");
		}

		// Modules our Privates require
		PrivateDependencyModuleNames.AddRange(
			new string[] {
			"Core",
			"Engine",
			"Sockets",
			"OnlineSubsystemUtils",
			"Voice",
			"Projects"
			}
			);

		// Modules our Publics require
		PublicDependencyModuleNames.AddRange(
			new string[] {
			"OnlineSubsystem",
			}
			);

		string WinMDReferencePathRoot = Path.Combine(ModuleDirectory, "..", "ThirdParty");
		string RuntimeDependencyPathRoot = Path.Combine("$(PluginDir)", "ThirdParty");

		// XSAPI package names are long enough that a shortened symbolic link is set up in GetXboxLiveSDK.bat
		string PackageFolder = string.Empty;
		string PlatformArchAndCompilerPathChunk = string.Empty;
		switch (Target.Platform)
		{
			case UnrealTargetPlatform.XboxOne:
				PackageFolder = "XboxOne";
				PlatformArchAndCompilerPathChunk = Path.Combine("references", "Durango", "v110");
				break;

			case UnrealTargetPlatform.Win32:
				// This case is currently used for intellisense generation.  Fall-through to UWP32 so it can find the winmd
			case UnrealTargetPlatform.UWP32:
				PackageFolder = "UWP";
				PlatformArchAndCompilerPathChunk = Path.Combine("lib", "Win32", "v140");
				break;

			case UnrealTargetPlatform.Win64:
				// This case is currently used for intellisense generation.  Fall-through to UWP64 so it can find the winmd
			case UnrealTargetPlatform.UWP64:
				PackageFolder = "UWP";
				PlatformArchAndCompilerPathChunk = Path.Combine("lib", "x64", "v140");
				break;
		}
		string NugetPathChunk = Path.Combine(PackageFolder, "build", "native", PlatformArchAndCompilerPathChunk, "release");

		string XSAPISubDir = Path.Combine("XSAPI", NugetPathChunk);
		if (!AddWinRTDllReference(XSAPISubDir, "Microsoft.Xbox.Services"))
		{
			Log.TraceError("Error: Xbox Live SDK not found.  Run Engine/Plugins/Online/XboxOne/OnlineSubsystemLive/GetXboxLiveSDK.bat");
		}

		if (Target.Platform != UnrealTargetPlatform.XboxOne)
		{
			string PlatformSubDir = string.Empty;
			switch (Target.Platform)
			{
				case UnrealTargetPlatform.Win64:
					// This case is currently used for intellisense generation.  Fall-through to UWP64 so it can find the winmd
					PlatformSubDir = UnrealTargetPlatform.UWP64.ToString();
					break;

				case UnrealTargetPlatform.Win32:
					PlatformSubDir = UnrealTargetPlatform.UWP32.ToString();
					break;

				default:
					PlatformSubDir = Target.Platform.ToString();
					break;
			}

			bool HasGameChat = true;
			string GameChatSubDir = Path.Combine("GameChat", "Binaries", PlatformSubDir);
			HasGameChat = HasGameChat && AddWinRTDllReference(GameChatSubDir, "Microsoft.Xbox.GameChat");
			HasGameChat = HasGameChat && AddWinRTDllReference(GameChatSubDir, "Microsoft.Xbox.ChatAudio");
			Definitions.Add(string.Format("WITH_GAME_CHAT={0}", HasGameChat ? 1 : 0));

			string EraAdapterSubDir = Path.Combine("EraAdapter", "Binaries", PlatformSubDir);
			AddWinRTDllReference(EraAdapterSubDir, "EraAdapter");

			// CppRest a little different - it's not a WinRT component, and it will need to be loaded explicitly
			const string CppRestVersion = "2_8";
			string CppRestDll = Path.Combine("ThirdParty", XSAPISubDir, string.Format("cpprest140_uwp_{0}.dll", CppRestVersion));
			RuntimeDependencies.Add(new RuntimeDependency(Path.Combine("$(PluginDir)", CppRestDll)));
			Definitions.Add(string.Format(@"CPP_REST_DLL=TEXT(""{0}"")", CppRestDll.Replace(@"\", "/")));
		}
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
		RuntimeDependencies.Add(new RuntimeDependency(Path.Combine("$(PluginDir)", "ThirdParty", SubDir, BaseFileName + ".dll")));
		return true;
	}
	// @ATG_CHANGE : END 
}
