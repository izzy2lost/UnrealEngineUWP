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
		/// Architecture of Target.
		/// </summary>
		[CommandLine("x64", Value = "x64")]
		public WindowsArchitecture Architecture = WindowsArchitecture.x64;

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
		public string Win10SDKVersionString
		{
			get { return Inner.Win10SDKVersionString; }
		}
#if !__MonoCS__
#pragma warning restore CS1591
#endif
		#endregion
	}

	class UniversalWindowsPlatform : UEBuildPlatform
	{
		public static readonly Version MinimumSDKVersionRecommended = new Version(10, 0, 17134, 0);
		public static readonly Version MaximumSDKVersionTested = new Version(10, 0, 19041, int.MaxValue);
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

		public override void ValidateTarget(TargetRules Target)
		{
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

			Target.UWPPlatform.Architecture = WindowsArchitecture.x64;

			if (!Target.bGenerateProjectFiles)
			{
				Log.TraceInformationOnce("Using x64 architecture for deploying to UWP");
			}

			Target.WindowsPlatform.Compiler = Target.UWPPlatform.Compiler;
			Target.WindowsPlatform.Architecture = Target.UWPPlatform.Architecture;
			Target.WindowsPlatform.bPixProfilingEnabled = Target.UWPPlatform.bPixProfilingEnabled;
			Target.WindowsPlatform.bUseWindowsSDK10 = true;

			Target.bDeployAfterCompile = true;
			Target.bCompileNvCloth = false;      // requires CUDA

			// Disable Simplygon support if compiling against the NULL RHI.
			if (Target.GlobalDefinitions.Contains("USE_NULL_RHI=1"))
			{
				Target.bCompileSpeedTree = false;
			}

			// Use shipping binaries to avoid dependency on nvToolsExt which fails WACK.
			if (Target.Configuration == UnrealTargetConfiguration.Shipping)
			{
				Target.bUseShippingPhysXLibraries = true;
			}

			// Be resilient to SDKs being uninstalled but still referenced in the INI file
			VersionNumber SelectedWindowsSdkVersion;
			DirectoryReference SelectedWindowsSdkDir;
			if (!WindowsPlatform.TryGetWindowsSdkDir(Target.UWPPlatform.Win10SDKVersionString, out SelectedWindowsSdkVersion, out SelectedWindowsSdkDir))
			{
				Target.UWPPlatform.Win10SDKVersionString = "Latest";
			}

			// Initialize the VC environment for the target, and set all the version numbers to the concrete values we chose.
			VCEnvironment Environment = VCEnvironment.Create(Target.WindowsPlatform.Compiler, Platform, Target.WindowsPlatform.Architecture, Target.WindowsPlatform.CompilerVersion, Target.UWPPlatform.Win10SDKVersionString);
			Target.WindowsPlatform.Environment = Environment;
			Target.WindowsPlatform.Compiler = Environment.Compiler;
			Target.WindowsPlatform.CompilerVersion = Environment.CompilerVersion.ToString();
			Target.WindowsPlatform.WindowsSdkVersion = Environment.WindowsSdkVersion.ToString();

			// Windows 10 SDK version
			// Auto-detect latest compatible by default (recommended), allow for explicit override if necessary
			// Validate that the SDK isn't too old, and that the combination of VS and SDK is supported.

			Target.UWPPlatform.Win10SDKVersion = new Version(Environment.WindowsSdkVersion.ToString());

			if (!Target.bGenerateProjectFiles)
			{
				Log.TraceInformationOnce("Building using Windows SDK version {0} for UWP", Target.UWPPlatform.Win10SDKVersion);

				if (Target.UWPPlatform.Win10SDKVersion < MinimumSDKVersionRecommended)
				{
					Log.TraceWarning("Your Windows SDK version {0} is older than the minimum recommended version ({1}) for UWP.  Consider upgrading.", Target.UWPPlatform.Win10SDKVersion, MinimumSDKVersionRecommended);
				}
				else if (Target.UWPPlatform.Win10SDKVersion > MaximumSDKVersionTested)
				{
					Log.TraceInformationOnce("Your Windows SDK version ({0}) for UWP is newer than the highest tested with this version of UBT ({1}).  This is probably fine, but if you encounter issues consider using an earlier SDK.", Target.UWPPlatform.Win10SDKVersion, MaximumSDKVersionTested);
				}
			}

			UWPExports.InitWindowsSdkToolPath(Target.UWPPlatform.Win10SDKVersion.ToString());
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

		private static Version FindLatestVersionDirectory(string InDirectory, Version NoLaterThan)
		{
			Version LatestVersion = new Version(0, 0, 0, 0);
			if (Directory.Exists(InDirectory))
			{
				string[] VersionDirectories = Directory.GetDirectories(InDirectory);
				foreach (string Dir in VersionDirectories)
				{
					string VersionString = Path.GetFileName(Dir);
					Version FoundVersion;
					if (Version.TryParse(VersionString, out FoundVersion) && FoundVersion > LatestVersion)
					{
						if (NoLaterThan == null || FoundVersion <= NoLaterThan)
						{
							LatestVersion = FoundVersion;
						}
					}
				}
			}
			return LatestVersion;
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
				if (ProjectFileGenerator.bGenerateProjectFiles)
				{
					// Use latest SDK for Intellisense purposes
					WindowsCompiler CompilerForSdkRestriction = Target.UWPPlatform.Compiler != WindowsCompiler.Default ? Target.UWPPlatform.Compiler : Target.WindowsPlatform.Compiler;
					if (CompilerForSdkRestriction != WindowsCompiler.Default)
					{
						Version OutWin10SDKVersion;
						DirectoryReference OutSdkDir;
						if (WindowsExports.TryGetWindowsSdkDir(Target.UWPPlatform.Win10SDKVersionString, out OutWin10SDKVersion, out OutSdkDir))
						{
							Rules.PublicDefinitions.Add(string.Format("WIN10_SDK_VERSION={0}", OutWin10SDKVersion.Build));
						}
					}
				}

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
				}
			}

			if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
			{
				// Adjust any WinMD references that were provided as contract names.
				Version OutWin10SDKVersion;
				DirectoryReference OutSdkDir;
				if (WindowsExports.TryGetWindowsSdkDir(Target.UWPPlatform.Win10SDKVersionString, out OutWin10SDKVersion, out OutSdkDir))
				{
					ExpandWinMDReferences(Target, OutSdkDir.ToString(), OutWin10SDKVersion.ToString(), ref Rules.PublicWinMDReferences);
					ExpandWinMDReferences(Target, OutSdkDir.ToString(), OutWin10SDKVersion.ToString(), ref Rules.PrivateWinMDReferences);
				}
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
				Rules.PublicRuntimeLibraryPaths.Clear();
				Rules.PublicSystemLibraryPaths.Clear();
				Rules.PublicAdditionalLibraries.Clear();
				Rules.PublicDefinitions.Remove("WITH_D3DX_LIBS=1");
				Rules.PublicDefinitions.Add("WITH_D3DX_LIBS=0");
				Rules.PublicAdditionalLibraries.Remove("X3DAudio.lib");
				Rules.PublicAdditionalLibraries.Remove("XAPOFX.lib");
			}
			else if (ModuleName == "XAudio2")
			{
				Rules.PublicDefinitions.Add("XAUDIO_SUPPORTS_XMA2WAVEFORMATEX=0");
				Rules.PublicDefinitions.Add("XAUDIO_SUPPORTS_DEVICE_DETAILS=0");
				Rules.PublicDefinitions.Add("XAUDIO2_SUPPORTS_MUSIC=0");
				Rules.PublicDefinitions.Add("XAUDIO2_SUPPORTS_SENDLIST=1");
				Rules.PublicAdditionalLibraries.Add("XAudio2.lib");
			}
			else if (ModuleName == "DX11Audio")
			{
				Rules.PublicAdditionalLibraries.Remove("X3DAudio.lib");
				Rules.PublicAdditionalLibraries.Remove("XAPOFX.lib");
			}

			// Adjust any WinMD references that were provided as contract names.
			Version OutWin10SDKVersion;
			DirectoryReference OutSdkDir;
			if (WindowsExports.TryGetWindowsSdkDir(Target.UWPPlatform.Win10SDKVersionString, out OutWin10SDKVersion, out OutSdkDir))
			{
				ExpandWinMDReferences(Target, OutSdkDir.ToString(), OutWin10SDKVersion.ToString(), ref Rules.PublicWinMDReferences);
				ExpandWinMDReferences(Target, OutSdkDir.ToString(), OutWin10SDKVersion.ToString(), ref Rules.PrivateWinMDReferences);
			}
		}

		private void ExpandWinMDReferences(ReadOnlyTargetRules Target, string SDKFolder, string SDKVersion, ref List<string> WinMDReferences)
		{
			// Code below will fail when not using the Win10 SDK.  Early out to avoid warning spam.
			if (!Target.WindowsPlatform.bUseWindowsSDK10)
			{
				return;
			}
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

						Version ContractVersion = FindLatestVersionDirectory(ContractFolder, null);
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
			string Win10SDKRoot = Target.WindowsPlatform.WindowsSdkDir;

			// Include paths
			CompileEnvironment.SystemIncludePaths.Add(new DirectoryReference(string.Format(@"{0}\Include\{1}\ucrt", Win10SDKRoot, Target.UWPPlatform.Win10SDKVersion)));
			CompileEnvironment.SystemIncludePaths.Add(new DirectoryReference (string.Format(@"{0}\Include\{1}\um", Win10SDKRoot, Target.UWPPlatform.Win10SDKVersion)));
			CompileEnvironment.SystemIncludePaths.Add(new DirectoryReference (string.Format(@"{0}\Include\{1}\shared", Win10SDKRoot, Target.UWPPlatform.Win10SDKVersion)));
			CompileEnvironment.SystemIncludePaths.Add(new DirectoryReference (string.Format(@"{0}\Include\{1}\winrt", Win10SDKRoot, Target.UWPPlatform.Win10SDKVersion)));

			// Library paths
			string LibArchitecture = Platform == UnrealTargetPlatform.UWP64 ? "x64" : "x86";
			LinkEnvironment.LibraryPaths.Add(new DirectoryReference (string.Format(@"{0}\Lib\{1}\ucrt\{2}", Win10SDKRoot, Target.UWPPlatform.Win10SDKVersion, LibArchitecture)));
			LinkEnvironment.LibraryPaths.Add(new DirectoryReference (string.Format(@"{0}\Lib\{1}\um\{2}", Win10SDKRoot, Target.UWPPlatform.Win10SDKVersion, LibArchitecture)));

			// Reference (WinMD) paths
			// Only Foundation and Universal are referenced by default.  Modules can bring in additional
			// contracts via the PublicWinMDReferences/PrivateWinMDReferences.
			List<string> AlwaysReferenceContracts = new List<string>();
			AlwaysReferenceContracts.Add("Windows.Foundation.FoundationContract");
			AlwaysReferenceContracts.Add("Windows.Foundation.UniversalApiContract");
			ExpandWinMDReferences(Target, Win10SDKRoot, Target.UWPPlatform.Win10SDKVersion.ToString(), ref AlwaysReferenceContracts);

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
			CompileEnvironment.Definitions.Add("OVERRIDE_PLATFORM_HEADER_NAME=UWP");

			// Presumably not necessary? Causes the OVERRIDE_PLATFORM_HEADER_NAME to point to 1.
			//CompileEnvironment.Definitions.Add("UWP=1");

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

			LinkEnvironment.AdditionalArguments += "/NODEFAULTLIB";
			LinkEnvironment.AdditionalLibraries.Add("windowsapp.lib");

			CompileEnvironment.Definitions.Add(string.Format("WIN10_SDK_VERSION={0}", Target.UWPPlatform.Win10SDKVersion.Build));

			LinkEnvironment.AdditionalLibraries.Add("dloadhelper.lib");
			LinkEnvironment.AdditionalLibraries.Add("ws2_32.lib");

			if (CompileEnvironment.bUseDebugCRT)
			{
				LinkEnvironment.AdditionalLibraries.Add("vccorlibd.lib");
				LinkEnvironment.AdditionalLibraries.Add("ucrtd.lib");
				LinkEnvironment.AdditionalLibraries.Add("vcruntimed.lib");
				LinkEnvironment.AdditionalLibraries.Add("msvcrtd.lib");
				LinkEnvironment.AdditionalLibraries.Add("msvcprtd.lib");
			}
			else
			{
				LinkEnvironment.AdditionalLibraries.Add("vccorlib.lib");
				LinkEnvironment.AdditionalLibraries.Add("ucrt.lib");
				LinkEnvironment.AdditionalLibraries.Add("vcruntime.lib");
				LinkEnvironment.AdditionalLibraries.Add("msvcrt.lib");
				LinkEnvironment.AdditionalLibraries.Add("msvcprt.lib");
			}
			LinkEnvironment.AdditionalLibraries.Add("legacy_stdio_wide_specifiers.lib");
			LinkEnvironment.AdditionalLibraries.Add("uuid.lib");
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
		/// <param name="Target">The target being built</param>
		/// <returns>New toolchain instance.</returns>
		public override UEToolChain CreateToolChain(ReadOnlyTargetRules Target)
		{
			return new UniversalWindowsPlatformToolChain(Target);
		}
	}



	class UWPPlatformSDK : UEBuildPlatformSDK
	{
		static bool bIsInstalled = false;
		static string LatestVersionString = string.Empty;
		static string InstallLocation = string.Empty;

		static UWPPlatformSDK()
		{
#if !__MonoCS__
			if (Utils.IsRunningOnMono)
			{
				return;
			}

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
#endif
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
