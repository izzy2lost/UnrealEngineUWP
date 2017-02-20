// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OnlineSubsystemLive : ModuleRules
{
	public OnlineSubsystemLive(TargetInfo Target)
	{
		Definitions.Add("ONLINESUBSYSTEMLIVE_PACKAGE=1");

		// @ATG_CHANGE : BEGIN XSAPI (decoupled from XDK) lives inside the OSSLive plugin.
		string SDKVersion = "2016.12.20170107.01";
		if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			// We need etwplus.lib for events, leader boards and achievements
			PublicAdditionalLibraries.Add("etwplus.lib");
			Definitions.Add("WITH_GAME_CHAT=1");
			SDKVersion = "2016.12.20170126.001";
		}

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"Engine", 
				"OnlineSubsystem", 
				"Sockets",
				"OnlineSubsystemUtils",
				"Voice",
				"Projects"
			}
			);

		string WinMDReferencePathRoot = Path.Combine(ModuleDirectory, "..", "ThirdParty");
		string RuntimeDependencyPathRoot = Path.Combine("$(PluginDir)", "ThirdParty");

		// XSAPI 
		string PackageFolder = string.Format("Microsoft.Xbox.Live.SDK.WinRT.{0}.{1}", Target.Platform == UnrealTargetPlatform.XboxOne ? "XboxOneXDK" : "UWP", SDKVersion);
		string PlatformArchAndCompilerPathChunk = string.Empty;
		switch (Target.Platform)
		{
			case UnrealTargetPlatform.XboxOne:
				PlatformArchAndCompilerPathChunk = Path.Combine("references", "Durango", "v110");
				break;

			case UnrealTargetPlatform.UWP32:
				PlatformArchAndCompilerPathChunk = Path.Combine("lib", "Win32", "v140");
				break;

			case UnrealTargetPlatform.UWP64:
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
			bool HasGameChat = true;
			string GameChatSubDir = Path.Combine("GameChat", "Binaries", Target.Platform.ToString());
			HasGameChat = HasGameChat && AddWinRTDllReference(GameChatSubDir, "Microsoft.Xbox.GameChat");
			HasGameChat = HasGameChat && AddWinRTDllReference(GameChatSubDir, "Microsoft.Xbox.ChatAudio");
			Definitions.Add(string.Format("WITH_GAME_CHAT={0}", HasGameChat ? 1 : 0));

			string EraAdapterSubDir = Path.Combine("EraAdapter", "Binaries", Target.Platform.ToString());
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
