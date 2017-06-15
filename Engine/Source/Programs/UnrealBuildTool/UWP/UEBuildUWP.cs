// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Microsoft.Win32;

namespace UnrealBuildTool
{

	class UniversalWindowsPlatform : UEBuildPlatform
	{

		UWPPlatformSDK SDK;

		public UniversalWindowsPlatform(UnrealTargetPlatform InPlatform, CppPlatform InDefaultCppPlatform, UWPPlatformSDK InSDK) : base(InPlatform, InDefaultCppPlatform)
		{
			SDK = InSDK;
		}

		public override SDKStatus HasRequiredSDKsInstalled()
		{
			return SDK.HasRequiredSDKsInstalled();
		}

		public override void ValidateTarget(TargetRules Target)
		{
			// WindowsTargetRules are reused for UWP, so that build modules can keep the model that reuses "windows" configs for most cases
			// That means overriding those settings here that need to be adjusted for UWP

			// Compiler version and pix flags must be reloaded from the UWP hive

			// Read the project setting
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(Target.ProjectFile), Target.Platform);

			Target.WindowsPlatform.Compiler = WindowsCompiler.Default;
			// prefer ini configuration first...
			string CompilerVersionString;
			if (Ini.GetString("/Script/UWPPlatformEditor.UWPTargetSettings", "CompilerVersion", out CompilerVersionString))
			{
				WindowsCompiler CompilerIniSetting;
				if (Enum.TryParse(CompilerVersionString, out CompilerIniSetting))
				{
					if (CompilerIniSetting >= WindowsCompiler.VisualStudio2015)
					{
						Target.WindowsPlatform.Compiler = CompilerIniSetting;
					}
					else if (CompilerIniSetting != WindowsCompiler.Default)
					{
						Log.TraceWarning("Selected compiler ({0}) requested by config is not supported.  Setting will be ignored.", CompilerIniSetting);
					}
				}
			}
			// then default to command line overrides
			if (Target.WindowsPlatform.Compiler == WindowsCompiler.Default)
			{
				Target.WindowsPlatform.Compiler = CommandLineCompilerOverride;
			}
			// then default to the generic default if necessary
			if (Target.WindowsPlatform.Compiler == WindowsCompiler.Default)
			{
				Target.WindowsPlatform.Compiler = WindowsPlatform.GetDefaultCompiler();
			}

			Target.WindowsPlatform.bPixProfilingEnabled = true; // default, turned off later for shipping\test
			bool bPixProfilingEnabled;
			if (Ini.GetBool("/Script/UWPPlatformEditor.UWPTargetSettings", "bEnablePIXProfiling", out bPixProfilingEnabled))
			{
				Target.WindowsPlatform.bPixProfilingEnabled = bPixProfilingEnabled;
			}

			Target.bDeployAfterCompile = true;
			Target.bCompileNvCloth = false;		 // requires CUDA

			// Disable Simplygon support if compiling against the NULL RHI.
			if (Target.GlobalDefinitions.Contains("USE_NULL_RHI=1"))
			{
				Target.bCompileSimplygon = false;
				Target.bCompileSimplygonSSF = false;
			}

			// Use shipping binaries to avoid dependency on nvToolsExt which fails WACK.
			Target.WindowsPlatform.bUseWindowsSDK10 = true;
			//Target.WindowsPlatform.bWinApiFamilyApp = true;

			if (Target.Configuration == UnrealTargetConfiguration.Shipping)
			{
				Target.bUseShippingPhysXLibraries = true;
			}

		}

		[CommandLine("-2015", Value = "VisualStudio2015")]
		[CommandLine("-2017", Value = "VisualStudio2017")]
		public WindowsCompiler CommandLineCompilerOverride = WindowsCompiler.Default;

		public override bool RequiresDeployPrepAfterCompile()
		{
			return true;
		}

		/// <summary>
		/// Get the extension to use for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binrary type being built</param>
		/// <returns>string	The binary extenstion (ie 'exe' or 'dll')</returns>
		public override string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
					return ".dll";
				case UEBuildBinaryType.Executable:
					return ".exe";
				case UEBuildBinaryType.StaticLibrary:
					return ".lib";
				case UEBuildBinaryType.Object:
					return ".obj";
				case UEBuildBinaryType.PrecompiledHeader:
					return ".pch";
			}
			return base.GetBinaryExtension(InBinaryType);
		}

		/// <summary>
		/// Get the extension to use for debug info for the given binary type
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string	The debug info extension (i.e. 'pdb')</returns>
		public override string GetDebugInfoExtension(ReadOnlyTargetRules Target, UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
				case UEBuildBinaryType.Executable:
					return ".pdb";
			}
			return "";
		}


		/// <summary>
		/// Whether the editor should be built for this platform or not
		/// </summary>
		/// <param name="InPlatform"> The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration">The UnrealTargetConfiguration being built</param>
		/// <returns>bool   true if the editor should be built, false if not</returns>
		public override bool ShouldNotBuildEditor(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return true;
		}

		public override bool BuildRequiresCookedData(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			return false;
		}

		/// <summary>
		/// Modify the rules for a newly created module, where the target is a different host platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForOtherPlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				if (!Target.bBuildRequiresCookedData)
				{
					if (ModuleName == "Engine")
					{
						if (Target.bBuildDeveloperTools)
						{
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("UWPTargetPlatform");
							Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("UWP32TargetPlatform");
						}
					}
				}

				// allow standalone tools to use targetplatform modules, without needing Engine
				if (ModuleName == "TargetPlatform")
				{
					if (Target.bForceBuildTargetPlatforms)
					{
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("UWPTargetPlatform");
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("UWP32TargetPlatform");
					}
				}

				// This forces OSSLive to be enumerated for Intellisense data when generating project 
				// files, so long as the project has any kind of dependency on online features.
				if (ProjectFileGenerator.bGenerateProjectFiles)
				{
					if (ModuleName == "OnlineSubsystem")
					{
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("OnlineSubsystemLive");
					}

					if (ModuleName == "Engine")
					{
						Rules.PlatformSpecificDynamicallyLoadedModuleNames.Add("UWPPlatformFeatures");
					}
				}
			}
		}

		/// <summary>
		/// Return whether this platform has uniquely named binaries across multiple games
		/// </summary>
		public override bool HasUniqueBinaries()
		{
			// Windows applications have many shared binaries between games
			return false;
		}

		/// <summary>
		/// Deploys the given target
		/// </summary>
		/// <param name="Target">Information about the target being deployed</param>
		public override void Deploy(UEBuildDeployTarget Target)
		{
			new UWPDeploy().PrepTargetForDeployment(Target);
		}

		/// <summary>
		/// Modify the rules for a newly created module, in a target that's being built for this platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForActivePlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
			if (ModuleName == "Core")
			{
				//Rules.PrivateDependencyModuleNames.Add("UWPSDK");
			}
			else if (ModuleName == "Engine")
			{
				Rules.PrivateDependencyModuleNames.Add("zlib");
				Rules.PrivateDependencyModuleNames.Add("UElibPNG");
				Rules.PublicDependencyModuleNames.Add("UEOgg");
				Rules.PublicDependencyModuleNames.Add("Vorbis");
			}
			else if (ModuleName == "D3D11RHI")
			{
				Rules.Definitions.Add("D3D11_WITH_DWMAPI=0");
				Rules.Definitions.Add("WITH_DX_PERF=0");
			}
			else if (ModuleName == "D3D12RHI")
			{
				if (Target.WindowsPlatform.bPixProfilingEnabled && Target.Platform == UnrealTargetPlatform.UWP64 && Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Configuration != UnrealTargetConfiguration.Test)
				{
					// Define to indicate profiling enabled (64-bit only)
					Rules.Definitions.Add("D3D12_PROFILING_ENABLED=1");
					Rules.Definitions.Add("PROFILE");
				}
				else
				{
					Rules.Definitions.Add("D3D12_PROFILING_ENABLED=0");
				}

				// To enable platform specific D3D12 RHI Types
				Rules.PrivateIncludePaths.Add("Runtime/Windows/D3D12RHI/Private/UWP");
			}
			else if (ModuleName == "DX11")
			{
				// Clear out all the Windows include paths and libraries...
				// The UWPSDK module handles proper paths and libs for UWP.
				// However, the D3D11RHI module will include the DX11 module.
				Rules.PublicIncludePaths.Clear();
				Rules.PublicLibraryPaths.Clear();
				Rules.PublicAdditionalLibraries.Clear();
				Rules.Definitions.Remove("WITH_D3DX_LIBS=1");
				Rules.Definitions.Add("WITH_D3DX_LIBS=0");
				Rules.PublicAdditionalLibraries.Remove("X3DAudio.lib");
				Rules.PublicAdditionalLibraries.Remove("XAPOFX.lib");
			}
			else if (ModuleName == "XAudio2")
			{
				Rules.Definitions.Add("XAUDIO_SUPPORTS_XMA2WAVEFORMATEX=0");
				Rules.Definitions.Add("XAUDIO_SUPPORTS_DEVICE_DETAILS=0");
				Rules.Definitions.Add("XAUDIO2_SUPPORTS_MUSIC=0");
				Rules.Definitions.Add("XAUDIO2_SUPPORTS_SENDLIST=1");
				Rules.PublicAdditionalLibraries.Add("XAudio2.lib");
			}
			else if (ModuleName == "DX11Audio")
			{
				Rules.PublicAdditionalLibraries.Remove("X3DAudio.lib");
				Rules.PublicAdditionalLibraries.Remove("XAPOFX.lib");
			}
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="Target">Settings for the target being compiled</param>
		/// <param name="CompileEnvironment">The compile environment for this target</param>
		/// <param name="LinkEnvironment">The link environment for this target</param>
		public override void SetUpEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment)
		{
			//CompileEnvironment.Definitions.Add("PLATFORM_DESKTOP=0");
			//CompileEnvironment.Definitions.Add("PLATFORM_64BITS=1");
			CompileEnvironment.Definitions.Add("EXCEPTIONS_DISABLED=0");

			CompileEnvironment.Definitions.Add("_WIN32_WINNT=0x0A00");
			CompileEnvironment.Definitions.Add("WINVER=0x0A00");

			CompileEnvironment.Definitions.Add("PLATFORM_UWP=1");
			CompileEnvironment.Definitions.Add("UWP=1");	

			CompileEnvironment.Definitions.Add("WINAPI_FAMILY=WINAPI_FAMILY_APP");

			// @todo UWP: UE4 is non-compliant when it comes to use of %s and %S
			// Previously %s meant "the current character set" and %S meant "the other one".
			// Now %s means multibyte and %S means wide. %Ts means "natural width".
			// Reverting this behavior until the UE4 source catches up.
			CompileEnvironment.Definitions.Add("_CRT_STDIO_LEGACY_WIDE_SPECIFIERS=1");

			// No D3DX on UWP!
			CompileEnvironment.Definitions.Add("NO_D3DX_LIBS=1");


			// Explicitly exclude the MS C++ runtime libraries we're not using, to ensure other libraries we link with use the same
			// runtime library as the engine.
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				LinkEnvironment.ExcludedLibraries.Add("MSVCRT");
				LinkEnvironment.ExcludedLibraries.Add("MSVCPRT");
			}
			else
			{
				LinkEnvironment.ExcludedLibraries.Add("MSVCRTD");
				LinkEnvironment.ExcludedLibraries.Add("MSVCPRTD");
			}
			LinkEnvironment.ExcludedLibraries.Add("LIBC");
			LinkEnvironment.ExcludedLibraries.Add("LIBCMT");
			LinkEnvironment.ExcludedLibraries.Add("LIBCPMT");
			LinkEnvironment.ExcludedLibraries.Add("LIBCP");
			LinkEnvironment.ExcludedLibraries.Add("LIBCD");
			LinkEnvironment.ExcludedLibraries.Add("LIBCMTD");
			LinkEnvironment.ExcludedLibraries.Add("LIBCPMTD");
			LinkEnvironment.ExcludedLibraries.Add("LIBCPD");

			LinkEnvironment.ExcludedLibraries.Add("ole32");

			LinkEnvironment.AdditionalLibraries.Add("windowsapp.lib");

			// In the 10586 SDK TLS APIs are not inlined, but they're also not in windowsapp.lib
			string SDKFolder = VCEnvironment.FindWindowsSDKInstallationFolder(CompileEnvironment.Platform, Target.WindowsPlatform.Compiler);
			Version SDKVersion = VCEnvironment.FindWindowsSDKExtensionLatestVersion(SDKFolder, Target.WindowsPlatform.Compiler);
			if (SDKVersion.Build == 10586)
			{
				LinkEnvironment.AdditionalLibraries.Add("kernel32.lib");
			}
			CompileEnvironment.Definitions.Add(string.Format("WIN10_SDK_VERSION={0}", SDKVersion.Build));

			LinkEnvironment.AdditionalLibraries.Add("dloadhelper.lib");
			LinkEnvironment.AdditionalLibraries.Add("ws2_32.lib");
		}

		/// <summary>
		/// Setup the configuration environment for building
		/// </summary>
		/// <param name="Target"> The target being built</param>
		/// <param name="GlobalCompileEnvironment">The global compile environment</param>
		/// <param name="GlobalLinkEnvironment">The global link environment</param>
		public override void SetUpConfigurationEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment GlobalCompileEnvironment, LinkEnvironment GlobalLinkEnvironment)
		{
			// Determine the C++ compile/link configuration based on the Unreal configuration.

			if (GlobalCompileEnvironment.bUseDebugCRT)
			{
				GlobalCompileEnvironment.Definitions.Add("_DEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("NDEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
			}

			//CppConfiguration CompileConfiguration;
			UnrealTargetConfiguration CheckConfig = Target.Configuration;
			switch (CheckConfig)
			{
				default:
				case UnrealTargetConfiguration.Debug:
					GlobalCompileEnvironment.Definitions.Add("UE_BUILD_DEBUG=1");
					break;
				case UnrealTargetConfiguration.DebugGame:
				// Default to Development; can be overriden by individual modules.
				case UnrealTargetConfiguration.Development:
					GlobalCompileEnvironment.Definitions.Add("UE_BUILD_DEVELOPMENT=1");
					break;
				case UnrealTargetConfiguration.Shipping:
					GlobalCompileEnvironment.Definitions.Add("UE_BUILD_SHIPPING=1");
					break;
				case UnrealTargetConfiguration.Test:
					GlobalCompileEnvironment.Definitions.Add("UE_BUILD_TEST=1");
					break;
			}

			// Create debug info based on the heuristics specified by the user.
			GlobalCompileEnvironment.bCreateDebugInfo =
				!Target.bDisableDebugInfo && ShouldCreateDebugInfo(Target);

			// NOTE: Even when debug info is turned off, we currently force the linker to generate debug info
			//	   anyway on Visual C++ platforms.  This will cause a PDB file to be generated with symbols
			//	   for most of the classes and function/method names, so that crashes still yield somewhat
			//	   useful call stacks, even though compiler-generate debug info may be disabled.  This gives
			//	   us much of the build-time savings of fully-disabled debug info, without giving up call
			//	   data completely.
			GlobalLinkEnvironment.bCreateDebugInfo = true;
		}

		/// <summary>
		/// Whether this platform should create debug information or not
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <returns>bool	true if debug info should be generated, false if not</returns>
		public override bool ShouldCreateDebugInfo(ReadOnlyTargetRules Target)
		{
			switch (Target.Configuration)
			{
				case UnrealTargetConfiguration.Development:
				case UnrealTargetConfiguration.Shipping:
				case UnrealTargetConfiguration.Test:
					return !Target.bOmitPCDebugInfoInDevelopment;
				case UnrealTargetConfiguration.DebugGame:
				case UnrealTargetConfiguration.Debug:
				default:
					return true;
			};
		}

		/// <summary>
		/// Creates a toolchain instance for the given platform.
		/// </summary>
		/// <param name="CppPlatform">The platform to create a toolchain for</param>
		/// <param name="Target">The target being built</param>
		/// <returns>New toolchain instance.</returns>
		public override UEToolChain CreateToolChain(CppPlatform CppPlatform, ReadOnlyTargetRules Target)
		{
			WindowsPlatform.bUseWindowsSDK10 = true;

			return new UniversalWindowsPlatformToolChain(CppPlatform, Target.WindowsPlatform.Compiler);
		}
	}



	class UWPPlatformSDK : UEBuildPlatformSDK
	{
		static bool bIsInstalled = false;
		static string LatestVersionString = string.Empty;
		static string InstallLocation = string.Empty;

		static UWPPlatformSDK()
		{
			string Version = "v10.0";
			string[] possibleRegLocations =
			{
				@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\",
				@"HKEY_CURRENT_USER\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\"
			};
			foreach (string regLocation in possibleRegLocations)
			{
				object Result = Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\" + Version, "InstallationFolder", null);

				if (Result != null)
				{
					bIsInstalled = true;
					InstallLocation = (string)Result;
					LatestVersionString = Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\" + Version, "ProductVersion", null) as string;
					break;
				}
			}

		}

		protected override SDKStatus HasRequiredManualSDKInternal()
		{
			return bIsInstalled ? SDKStatus.Valid : SDKStatus.Invalid;
		}
	}

	class UWPPlatformFactory : UEBuildPlatformFactory
	{
		protected override UnrealTargetPlatform TargetPlatform
		{
			get { return UnrealTargetPlatform.UWP64; }
		}

		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		/// <param name="OutputLevel"></param>
		protected override void RegisterBuildPlatforms(SDKOutputLevel OutputLevel)
		{
			UWPPlatformSDK SDK = new UWPPlatformSDK();
			SDK.ManageAndValidateSDK(OutputLevel);

			// Register this build platform for UWP
			if (SDK.HasRequiredSDKsInstalled() == SDKStatus.Valid)
			{
				Log.TraceVerbose("		Registering for {0}", UnrealTargetPlatform.UWP64.ToString());
				UEBuildPlatform.RegisterBuildPlatform(new UniversalWindowsPlatform(UnrealTargetPlatform.UWP64, CppPlatform.UWP64, SDK));
				UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.UWP64, UnrealPlatformGroup.Microsoft);
				UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.UWP64, UnrealPlatformGroup.UWP);

				Log.TraceVerbose("		Registering for {0}", UnrealTargetPlatform.UWP32.ToString());
				UEBuildPlatform.RegisterBuildPlatform(new UniversalWindowsPlatform(UnrealTargetPlatform.UWP32, CppPlatform.UWP32, SDK));
				UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.UWP32, UnrealPlatformGroup.Microsoft);
				UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.UWP32, UnrealPlatformGroup.UWP);
			}
		}

	}
}
