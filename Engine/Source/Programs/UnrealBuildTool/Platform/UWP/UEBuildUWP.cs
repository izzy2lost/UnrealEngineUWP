// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Microsoft.Win32;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// UWP-specific target settings
	/// </summary>
	public class UWPTargetRules
	{
		/// <summary>
		/// Version of the compiler toolchain to use on UWP. A value of "default" will be changed to a specific version at UBT startup.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/UWPPlatformEditor.UWPTargetSettings", "CompilerVersion")]
		[CommandLine("-2017", Value = "VisualStudio2017")]
		[CommandLine("-2019", Value = "VisualStudio2019")]
		public WindowsCompiler Compiler = WindowsCompiler.Default;

		/// <summary>
		/// Enable PIX debugging (automatically disabled in Shipping and Test configs)
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/UWPPlatformEditor.UWPTargetSettings", "bEnablePIXProfiling")]
		public bool bPixProfilingEnabled = true;

		/// <summary>
		/// Version of the compiler toolchain to use on UWP. A value of "default" will be changed to a specific version at UBT startup.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/UWPPlatformEditor.UWPTargetSettings", "bBuildForRetailWindowsStore")]
		public bool bBuildForRetailWindowsStore = false;

		/// <summary>
		/// Controls whether to use XIM for Xbox Live multiplayer and chat.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/UWPPlatformEditor.UWPTargetSettings", "bUseXim")]
		public bool bUseXim = false;

		/// <summary>
		/// Controls whether to use Achievements 2017 APIs.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/UWPPlatformEditor.UWPTargetSettings", "bUseAchievements2017")]
		public bool bUseAchievements2017 = false;

		/// <summary>
		/// Controls whether to use Stats 2017 APIs.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/UWPPlatformEditor.UWPTargetSettings", "bUseStats2017")]
		public bool bUseStats2017 = false;

		/// <summary>
		/// Controls whether the title accesses Xbox Live via the Creators Program
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/UWPPlatformEditor.UWPTargetSettings", "bIsCreatorsProgramTitle")]
		public bool bIsCreatorsProgramTitle = false;

		/// <summary>
		/// Controls whether the D3D12 RHI should be included in the build.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/UWPPlatformEditor.UWPTargetSettings", "bBuildD3D12RHI")]
		public bool bBuildD3D12RHI = true;

		/// <summary>
		/// Contains the specific version of the Windows 10 SDK that we will build against.
		/// Note that this is separate from the version of the OS that will be targeted at runtime.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/UWPPlatformEditor.UWPTargetSettings", "Windows10SDKVersion")]
		public string Win10SDKVersionString = null;

		internal Version Win10SDKVersion = null;
	}

	/// <summary>
	/// Read-only wrapper for UWP-specific target settings
	/// </summary>
	public class ReadOnlyUWPTargetRules
	{
		/// <summary>
		/// The private mutable settings object
		/// </summary>
		private UWPTargetRules Inner;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Inner">The settings object to wrap</param>
		public ReadOnlyUWPTargetRules(UWPTargetRules Inner)
		{
			this.Inner = Inner;
		}

		/// <summary>
		/// Accessors for fields on the inner TargetRules instance
		/// </summary>
		#region Read-only accessor properties 
#if !__MonoCS__
#pragma warning disable CS1591
#endif
		public WindowsCompiler Compiler
		{
			get { return Inner.Compiler; }
		}

		public bool bPixProfilingEnabled
		{
			get { return Inner.bPixProfilingEnabled; }
		}

		public bool bBuildForRetailWindowsStore
		{
			get { return Inner.bBuildForRetailWindowsStore; }
		}

		public bool bUseXim
		{
			get { return Inner.bUseXim; }
		}

		public bool bUseAchievements2017
		{
			get { return Inner.bUseAchievements2017; }
		}

		public bool bUseStats2017
		{
			get { return Inner.bUseStats2017; }
		}

		public bool bIsCreatorsProgramTitle
		{
			get { return Inner.bIsCreatorsProgramTitle; }
		}

		public bool bBuildD3D12RHI
		{
			get { return Inner.bBuildD3D12RHI; }
		}

		public Version Win10SDKVersion
		{
			get { return Inner.Win10SDKVersion; }
		}
#if !__MonoCS__
#pragma warning restore CS1591
#endif
		#endregion
	}

	class UniversalWindowsPlatform : UEBuildPlatform
	{
		public static readonly Version MinimumSDKVersionRecommended = new Version(10, 0, 14393, 0);
		public static readonly Version MaximumSDKVersionTested = new Version(10, 0, 17134, int.MaxValue);
		public static readonly Version MaximumSDKVersionForVS2015 = new Version(10, 0, 14393, int.MaxValue);
		public static readonly Version MinimumSDKVersionForD3D12RHI = new Version(10, 0, 15063, 0);

		UWPPlatformSDK SDK;


		public UniversalWindowsPlatform(UnrealTargetPlatform InPlatform, UWPPlatformSDK InSDK) : base(InPlatform)
		{
			SDK = InSDK;
		}

		public override SDKStatus HasRequiredSDKsInstalled()
		{
			return SDK.HasRequiredSDKsInstalled();
		}

		public override void ResetTarget(TargetRules Target)
		{
			ValidateTarget(Target);

			Target.bDeployAfterCompile = true;
		}

		public override void ValidateTarget(TargetRules Target)
		{
			// WindowsTargetRules are reused for UWP, so that build modules can keep the model that reuses "windows" configs for most cases
			// That means overriding those settings here that need to be adjusted for UWP

			// Compiler version and pix flags must be reloaded from the UWP hive

			// Currently BP-only projects don't load build-related settings from their remote ini when building UE4Game.exe
			// (see TargetRules.cs, where the possibly-null project directory is passed to ConfigCache.ReadSettings).
			// It's important for UWP that we *do* use the project-specific settings when building (VS 2017 vs 2015 and
			// retail Windows Store are both examples).  Possibly this should be done on all platforms?  But in the interest
			// of not changing behavior on other platforms I'm limiting the scope.

			DirectoryReference IniDirRef = DirectoryReference.FromFile(Target.ProjectFile);
			if (IniDirRef == null && !string.IsNullOrEmpty(UnrealBuildTool.GetRemoteIniPath()))
			{
				IniDirRef = new DirectoryReference(UnrealBuildTool.GetRemoteIniPath());
			}

			// Stash the current compiler choice (accounts for command line) in case ReadSettings reverts it to default
			WindowsCompiler CompilerBeforeReadSettings = Target.UWPPlatform.Compiler;

			ConfigCache.ReadSettings(IniDirRef, Platform, Target.UWPPlatform);

			if (Target.UWPPlatform.Compiler == WindowsCompiler.Default)
			{
				if (CompilerBeforeReadSettings != WindowsCompiler.Default)
				{
					// Previous setting was more specific, use that
					Target.UWPPlatform.Compiler = CompilerBeforeReadSettings;
				}
				else
				{
					Target.UWPPlatform.Compiler = WindowsPlatform.GetDefaultCompiler(Target.ProjectFile);
				}
			}

			Target.WindowsPlatform.Compiler = Target.UWPPlatform.Compiler;
			Target.WindowsPlatform.bPixProfilingEnabled = Target.UWPPlatform.bPixProfilingEnabled;
			Target.WindowsPlatform.bUseWindowsSDK10 = true;

			Target.bDeployAfterCompile = true;
			Target.bCompileNvCloth = false;      // requires CUDA

			// Use shipping binaries to avoid dependency on nvToolsExt which fails WACK.
			if (Target.Configuration == UnrealTargetConfiguration.Shipping)
			{
				Target.bUseShippingPhysXLibraries = true;
			}

			// All Creators Program titles use stats 2017
			Target.UWPPlatform.bUseStats2017 |= Target.UWPPlatform.bIsCreatorsProgramTitle;

			// Windows 10 SDK version
			// Auto-detect latest compatible by default (recommended), allow for explicit override if necessary
			// Validate that the SDK isn't too old, and that the combination of VS and SDK is supported.
			string SDKFolder = VCEnvironment.FindWindowsSDKInstallationFolder(UnrealTargetPlatform.UWP64, Target.UWPPlatform.Compiler);

			if (string.IsNullOrEmpty(Target.UWPPlatform.Win10SDKVersionString))
			{
				Log.TraceInformation("Auto-detecting Windows 10 SDK version...");
				Target.UWPPlatform.Win10SDKVersion = VCEnvironment.FindWindowsSDKExtensionLatestVersion(SDKFolder, Target.UWPPlatform.Compiler);

				if (Target.UWPPlatform.Win10SDKVersion.Major == 0)
				{
					throw new BuildException("Could not locate a Windows 10 SDK version compatible with your build environment.  For VS2015 use the {0} SDK.  For VS2107 use the latest available.", MaximumSDKVersionForVS2015.Build);
				}
			}
			else
			{
				if (!Version.TryParse(Target.UWPPlatform.Win10SDKVersionString, out Target.UWPPlatform.Win10SDKVersion))
				{
					throw new BuildException("Requested Windows 10 SDK version ({0}) is not in a recognized format.  Expected something like {1}.", Target.UWPPlatform.Win10SDKVersionString, MinimumSDKVersionRecommended);
				}
				else if (!Directory.Exists(Path.Combine(SDKFolder, "include", Target.UWPPlatform.Win10SDKVersionString)))
				{
					throw new BuildException("Requested Windows 10 SDK version ({0}) was not found in {1}.  Please check your installation.", Target.UWPPlatform.Win10SDKVersionString, SDKFolder);
				}
			}

			Log.TraceInformation("Building using Windows SDK version {0}", Target.UWPPlatform.Win10SDKVersion);

			if (Target.UWPPlatform.Win10SDKVersion < MinimumSDKVersionRecommended)
			{
				Log.TraceWarning("Your Windows SDK version {0} is older than the minimum recommended version ({1}).  Consider upgrading.", Target.UWPPlatform.Win10SDKVersion, MinimumSDKVersionRecommended);
			}
			else if (Target.UWPPlatform.Win10SDKVersion > MaximumSDKVersionTested)
			{
				Log.TraceInformation("Your Windows SDK version ({0}) is newer than the highest tested with this version of UBT ({1}).  This is probably fine, but if you encounter issues consider using an earlier SDK.", Target.UWPPlatform.Win10SDKVersion, MaximumSDKVersionTested);
			}

			if (Target.UWPPlatform.bBuildD3D12RHI && Target.UWPPlatform.Win10SDKVersion < MinimumSDKVersionForD3D12RHI)
			{
				Log.TraceWarning("Ignoring 'Build with D3D12 support' flag: the D3D12 RHI requires at least the {0} SDK.", MinimumSDKVersionForD3D12RHI);
				Target.UWPPlatform.bBuildD3D12RHI = false;
			}

			// Initialize the VC environment for the target, and set all the version numbers to the concrete values we chose.
			VCEnvironment Environment = VCEnvironment.Create(Target.WindowsPlatform.Compiler, Platform, Target.WindowsPlatform.Architecture, Target.WindowsPlatform.CompilerVersion, Target.UWPPlatform.Win10SDKVersionString, null);
			Target.WindowsPlatform.Environment = Environment;
			Target.WindowsPlatform.Compiler = Environment.Compiler;
			Target.WindowsPlatform.CompilerVersion = Environment.CompilerVersion.ToString();
			Target.WindowsPlatform.WindowsSdkVersion = Environment.WindowsSdkVersion.ToString();
		}

		/// <summary>
		/// Determines if the given name is a build product for a target.
		/// </summary>
		/// <param name="FileName">The name to check</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UE4Editor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <returns>True if the string matches the name of a build product, false otherwise</returns>
		public override bool IsBuildProduct(string FileName, string[] NamePrefixes, string[] NameSuffixes)
		{
			return IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".exe")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".dll")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".dll.response")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".lib")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".pdb")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".exp")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".obj")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".map")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".objpaths");
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
			}
			return base.GetBinaryExtension(InBinaryType);
		}

		/// <summary>
		/// Get the extension to use for debug info for the given binary type
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string	The debug info extension (i.e. 'pdb')</returns>
		public override string[] GetDebugInfoExtensions(ReadOnlyTargetRules Target, UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
				case UEBuildBinaryType.Executable:
					return new string[] { ".pdb" };
			}
			return new string[] { "" };
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
							Rules.DynamicallyLoadedModuleNames.Add("UWPTargetPlatform");
							Rules.DynamicallyLoadedModuleNames.Add("UWP32TargetPlatform");
						}
					}
				}

				// allow standalone tools to use targetplatform modules, without needing Engine
				if (ModuleName == "TargetPlatform")
				{
					if (Target.bForceBuildTargetPlatforms)
					{
						Rules.DynamicallyLoadedModuleNames.Add("UWPTargetPlatform");
						Rules.DynamicallyLoadedModuleNames.Add("UWP32TargetPlatform");
					}
				}

				// This forces OSSLive to be enumerated for Intellisense data when generating project 
				// files, so long as the project has any kind of dependency on online features.
				if (ProjectFileGenerator.bGenerateProjectFiles)
				{
					if (ModuleName == "OnlineSubsystem")
					{
						Rules.DynamicallyLoadedModuleNames.Add("OnlineSubsystemLive");
					}

					if (ModuleName == "Engine")
					{
						Rules.DynamicallyLoadedModuleNames.Add("UWPPlatformFeatures");
					}

					// Use latest SDK for Intellisense purposes
					WindowsCompiler CompilerForSdkRestriction = Target.UWPPlatform.Compiler != WindowsCompiler.Default ? Target.UWPPlatform.Compiler : Target.WindowsPlatform.Compiler;
					if (CompilerForSdkRestriction != WindowsCompiler.Default)
					{
						string SDKFolder = VCEnvironment.FindWindowsSDKInstallationFolder(UnrealTargetPlatform.UWP64, CompilerForSdkRestriction);
						Version SDKVersion = VCEnvironment.FindWindowsSDKExtensionLatestVersion(SDKFolder, Target.UWPPlatform.Compiler);
						Rules.PublicDefinitions.Add(string.Format("WIN10_SDK_VERSION={0}", SDKVersion.Build));
					}
				}
			}

			if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
			{
				// Adjust any WinMD references that were provided as contract names.
				string SDKFolder = VCEnvironment.FindWindowsSDKInstallationFolder(UnrealTargetPlatform.UWP32, Target.WindowsPlatform.Compiler);
				string SDKVersionString = VCEnvironment.FindWindowsSDKExtensionLatestVersion(SDKFolder, Target.WindowsPlatform.Compiler).ToString();
				ExpandWinMDReferences(SDKFolder, SDKVersionString, ref Rules.PublicWinMDReferences);
				ExpandWinMDReferences(SDKFolder, SDKVersionString, ref Rules.PrivateWinMDReferences);
			}
		}

		/// <summary>
		/// Deploys the given target
		/// </summary>
		/// <param name="Receipt">Receipt for the target being deployed</param>
		public override void Deploy(TargetReceipt Receipt)
		{
			new UWPDeploy(Receipt.ProjectFile).PrepTargetForDeployment(Receipt);
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
				Rules.PublicDefinitions.Add("D3D11_WITH_DWMAPI=0");
				Rules.PublicDefinitions.Add("WITH_DX_PERF=0");
			}
			else if (ModuleName == "D3D12RHI")
			{
				if (Target.WindowsPlatform.bPixProfilingEnabled && Target.Platform == UnrealTargetPlatform.UWP64 && Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Configuration != UnrealTargetConfiguration.Test)
				{
					// Define to indicate profiling enabled (64-bit only)
					Rules.PublicDefinitions.Add("D3D12_PROFILING_ENABLED=1");
					Rules.PublicDefinitions.Add("PROFILE");
				}
				else
				{
					Rules.PublicDefinitions.Add("D3D12_PROFILING_ENABLED=0");
				}
			}
			else if (ModuleName == "DX11")
			{
				// Clear out all the Windows include paths and libraries...
				// The UWPSDK module handles proper paths and libs for UWP.
				// However, the D3D11RHI module will include the DX11 module.
				Rules.PublicIncludePaths.Clear();
				Rules.PublicLibraryPaths.Clear();
				Rules.PublicAdditionalLibraries.Clear();
				Rules.PublicDefinitions.Remove("WITH_D3DX_LIBS=1");
				Rules.PublicDefinitions.Add("WITH_D3DX_LIBS=0");
				Rules.PublicAdditionalLibraries.Remove("X3DAudio.lib");
				Rules.PublicAdditionalLibraries.Remove("XAPOFX.lib");
			}
			else if (ModuleName == "XAudio2")
			{
				//Rules.PublicDefinitions.Add("XAUDIO_SUPPORTS_XMA2WAVEFORMATEX=0");
				//Rules.PublicDefinitions.Add("XAUDIO_SUPPORTS_DEVICE_DETAILS=0");
				//Rules.PublicDefinitions.Add("XAUDIO2_SUPPORTS_MUSIC=0");
				//Rules.PublicDefinitions.Add("XAUDIO2_SUPPORTS_SENDLIST=1");
				//Rules.PublicAdditionalLibraries.Add("XAudio2.lib");
			}
			else if (ModuleName == "DX11Audio")
			{
				Rules.PublicAdditionalLibraries.Remove("X3DAudio.lib");
				Rules.PublicAdditionalLibraries.Remove("XAPOFX.lib");
			}

			// Adjust any WinMD references that were provided as contract names.
			string SDKFolder = VCEnvironment.FindWindowsSDKInstallationFolder(UnrealTargetPlatform.UWP64, Target.UWPPlatform.Compiler);
			string SDKVersionString = Target.UWPPlatform.Win10SDKVersion.ToString();
			ExpandWinMDReferences(SDKFolder, SDKVersionString, ref Rules.PublicWinMDReferences);
			ExpandWinMDReferences(SDKFolder, SDKVersionString, ref Rules.PrivateWinMDReferences);
		}

		private void ExpandWinMDReferences(string SDKFolder, string SDKVersion, ref List<string> WinMDReferences)
		{
			// Code below will fail when not using the Win10 SDK.  Early out to avoid warning spam.
			//if (!WindowsPlatform.bUseWindowsSDK10)
			//{
			//	return;
			//}
			if (WinMDReferences.Count > 0)
			{
				// Allow bringing in Windows SDK contracts just by naming the contract
				// These are files that look like References/10.0.98765.0/AMadeUpWindowsApiContract/5.0.0.0/AMadeUpWindowsApiContract.winmd
				List<string> ExpandedWinMDReferences = new List<string>();

				// The first few releases of the Windows 10 SDK didn't put the SDK version in the reference path
				string ReferenceRoot = Path.Combine(SDKFolder, "References");
				string VersionedReferenceRoot = Path.Combine(ReferenceRoot, SDKVersion);
				if (Directory.Exists(VersionedReferenceRoot))
				{
					ReferenceRoot = VersionedReferenceRoot;
				}

				foreach (string WinMDRef in WinMDReferences)
				{
					if (File.Exists(WinMDRef))
					{
						// Already a valid path
						ExpandedWinMDReferences.Add(WinMDRef);
					}
					else
					{
						string ContractFolder = Path.Combine(ReferenceRoot, WinMDRef);

						Version ContractVersion = VCEnvironment.FindLatestVersionDirectory(ContractFolder, null);
						string ExpandedWinMDRef = Path.Combine(ContractFolder, ContractVersion.ToString(), WinMDRef + ".winmd");
						if (File.Exists(ExpandedWinMDRef))
						{
							ExpandedWinMDReferences.Add(ExpandedWinMDRef);
						}
						else
						{
							Log.TraceWarning("Unable to resolve location for WinMD api contract {0}", WinMDRef);
						}
					}
				}

				WinMDReferences = ExpandedWinMDReferences;
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
			// Add Win10 SDK pieces - moved here since it allows better control over SDK version
			string Win10SDKRoot = VCEnvironment.FindWindowsSDKInstallationFolder(UnrealTargetPlatform.UWP64, Target.UWPPlatform.Compiler);

			// Include paths
			CompileEnvironment.SystemIncludePaths.Add(new DirectoryReference(string.Format(@"{0}\Include\{1}\ucrt", Win10SDKRoot, Target.UWPPlatform.Win10SDKVersion)));
			CompileEnvironment.SystemIncludePaths.Add(new DirectoryReference(string.Format(@"{0}\Include\{1}\um", Win10SDKRoot, Target.UWPPlatform.Win10SDKVersion)));
			CompileEnvironment.SystemIncludePaths.Add(new DirectoryReference(string.Format(@"{0}\Include\{1}\shared", Win10SDKRoot, Target.UWPPlatform.Win10SDKVersion)));
			CompileEnvironment.SystemIncludePaths.Add(new DirectoryReference(string.Format(@"{0}\Include\{1}\winrt", Win10SDKRoot, Target.UWPPlatform.Win10SDKVersion)));

			// Library paths
			string LibArchitecture = Platform == UnrealTargetPlatform.UWP64 ? "x64" : "x86";
			LinkEnvironment.LibraryPaths.Add(new DirectoryReference(string.Format(@"{0}\Lib\{1}\ucrt\{2}", Win10SDKRoot, Target.UWPPlatform.Win10SDKVersion, LibArchitecture)));
			LinkEnvironment.LibraryPaths.Add(new DirectoryReference(string.Format(@"{0}\Lib\{1}\um\{2}", Win10SDKRoot, Target.UWPPlatform.Win10SDKVersion, LibArchitecture)));

			// Reference (WinMD) paths
			// Only Foundation and Universal are referenced by default.  Modules can bring in additional
			// contracts via the PublicWinMDReferences/PrivateWinMDReferences.
			List<string> AlwaysReferenceContracts = new List<string>();
			AlwaysReferenceContracts.Add("Windows.Foundation.FoundationContract");
			AlwaysReferenceContracts.Add("Windows.Foundation.UniversalApiContract");
			ExpandWinMDReferences(Win10SDKRoot, Target.UWPPlatform.Win10SDKVersion.ToString(), ref AlwaysReferenceContracts);

			StringBuilder WinMDReferenceArguments = new StringBuilder();
			foreach (string WinMDReference in AlwaysReferenceContracts)
			{
				WinMDReferenceArguments.AppendFormat(@" /FU""{0}""", WinMDReference);
			}
			CompileEnvironment.AdditionalArguments += WinMDReferenceArguments;

			CompileEnvironment.Definitions.Add("EXCEPTIONS_DISABLED=0");

			CompileEnvironment.Definitions.Add("_WIN32_WINNT=0x0A00");
			CompileEnvironment.Definitions.Add("WINVER=0x0A00");

			CompileEnvironment.Definitions.Add("PLATFORM_UWP=1");
			CompileEnvironment.Definitions.Add("UWP=1");
			CompileEnvironment.Definitions.Add("WITH_EDITOR=0");

			CompileEnvironment.Definitions.Add("WINAPI_FAMILY=WINAPI_FAMILY_APP");

			// No D3DX on UWP!
			CompileEnvironment.Definitions.Add("NO_D3DX_LIBS=1");

			if (Target.UWPPlatform.bBuildForRetailWindowsStore)
			{
				CompileEnvironment.Definitions.Add("USING_RETAIL_WINDOWS_STORE=1");
			}
			else
			{
				CompileEnvironment.Definitions.Add("USING_RETAIL_WINDOWS_STORE=0");
			}

			if (Target.UWPPlatform.bBuildD3D12RHI)
			{
				CompileEnvironment.Definitions.Add("WITH_D3D12_RHI=1");
			}
			else
			{
				CompileEnvironment.Definitions.Add("WITH_D3D12_RHI=0");
			}

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
			if (Target.UWPPlatform.Win10SDKVersion.Build == 10586)
			{
				LinkEnvironment.AdditionalLibraries.Add("kernel32.lib");
			}
			CompileEnvironment.Definitions.Add(string.Format("WIN10_SDK_VERSION={0}", Target.UWPPlatform.Win10SDKVersion.Build));

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


		public override UEToolChain CreateToolChain( ReadOnlyTargetRules Target)
		{
			//WindowsPlatform.bUseWindowsSDK10 = true;

			return new UniversalWindowsPlatformToolChain( Target);
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
		public override UnrealTargetPlatform TargetPlatform
		{
			get { return UnrealTargetPlatform.UWP64; }
		}

		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		public override void RegisterBuildPlatforms()
		{
			UWPPlatformSDK SDK = new UWPPlatformSDK();
			SDK.ManageAndValidateSDK();

			// Register this build platform for UWP
			if (SDK.HasRequiredSDKsInstalled() == SDKStatus.Valid)
			{
				Log.TraceVerbose("		Registering for {0}", UnrealTargetPlatform.UWP64.ToString());
				UEBuildPlatform.RegisterBuildPlatform(new UniversalWindowsPlatform(UnrealTargetPlatform.UWP64, SDK));
				UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.UWP64, UnrealPlatformGroup.Microsoft);
				UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.UWP64, UnrealPlatformGroup.UWP);

				Log.TraceVerbose("		Registering for {0}", UnrealTargetPlatform.UWP32.ToString());
				UEBuildPlatform.RegisterBuildPlatform(new UniversalWindowsPlatform(UnrealTargetPlatform.UWP32, SDK));
				UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.UWP32, UnrealPlatformGroup.Microsoft);
				UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.UWP32, UnrealPlatformGroup.UWP);
			}
		}

	}
}