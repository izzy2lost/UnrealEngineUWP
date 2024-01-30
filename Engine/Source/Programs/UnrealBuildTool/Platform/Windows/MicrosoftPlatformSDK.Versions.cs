// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.Linq;
using EpicGames.Core;
using UnrealBuildTool.PS4;

namespace UnrealBuildTool
{
	/////////////////////////////////////////////////////////////////////////////////////
	// If you are looking for any version numbers not listed here, see Windows_SDK.json
	/////////////////////////////////////////////////////////////////////////////////////

	partial class MicrosoftPlatformSDK : UEBuildPlatformSDK
	{
		/// <summary>
		/// The minimum Windows SDK version to be used. If this is null then it means there is no minimum version
		/// </summary>
		static readonly VersionNumber? MinimumWindowsSDKVersion = new VersionNumber(10, 0, 18362, 0);

		/// <summary>
		/// The maximum Windows SDK version to be used. If this is null then it means "Latest"
		/// </summary>
		static readonly VersionNumber? MaximumWindowsSDKVersion = null;

		/// <summary>
		/// The default compiler version to be used, if installed. 
		/// </summary>
		static readonly VersionNumberRange[] PreferredClangVersions =
		{
			// Clang 17: https://github.com/llvm/llvm-project/issues/71976 should not be used as a preferred version until this issue is resolved and the max affected version is added to ClangWarnings.cs
			VersionNumberRange.Parse("16.0.0", "16.999"), // VS2022 17.7.x runtime requires Clang 16
			VersionNumberRange.Parse("15.0.0", "15.999"), // VS2022 17.5.x runtime requires Clang 15
		};

		/// <summary>
		/// The minimum supported Clang compiler
		/// </summary>
		static readonly VersionNumber MinimumClangVersion = new VersionNumber(15, 0, 0);

		/// <summary>
		/// Ranges of tested compiler toolchains to be used, in order of preference. If multiple toolchains in a range are present, the latest version will be preferred.
		/// Note that the numbers here correspond to the installation *folders* rather than precise executable versions.
		/// </summary>
		/// <seealso href="https://learn.microsoft.com/en-us/lifecycle/products/visual-studio-2022"/>
		static readonly VersionNumberRange[] PreferredVisualCppVersions = new VersionNumberRange[]
		{
			VersionNumberRange.Parse("14.38.33130", "14.38.99999"), // VS2022 17.8.x
			VersionNumberRange.Parse("14.37.32822", "14.37.99999"), // VS2022 17.7.x
			VersionNumberRange.Parse("14.36.32532", "14.36.99999"), // VS2022 17.6.x
			VersionNumberRange.Parse("14.35.32215", "14.35.99999"), // VS2022 17.5.x
			VersionNumberRange.Parse("14.34.31933", "14.34.99999"), // VS2022 17.4.x
		};

		/// <summary>
		/// Minimum Clang version required for MSVC toolchain versions
		/// </summary>
		static readonly Tuple<VersionNumber, VersionNumber>[] MinimumRequiredClangVersion = new Tuple<VersionNumber, VersionNumber>[]
		{
			new(new VersionNumber(14, 37), new VersionNumber(16)), // VS2022 17.7.x - 17.8.x
			new(new VersionNumber(14, 35), new VersionNumber(15)), // VS2022 17.5.x - 17.6.x
			new(new VersionNumber(14, 34), new VersionNumber(14)), // VS2022 17.4.x
		};

		/// <summary>
		/// Tested compiler toolchains that should not be allowed.
		/// </summary>
		static readonly VersionNumberRange[] BannedVisualCppVersions = Array.Empty<VersionNumberRange>();

		/// <summary>
		/// The minimum supported MSVC compiler
		/// </summary>
		static readonly VersionNumber MinimumVisualCppVersion = new VersionNumber(14, 34, 31933);

		/// <summary>
		/// The default compiler version to be used, if installed. 
		/// https://www.intel.com/content/www/us/en/developer/articles/tool/oneapi-standalone-components.html#dpcpp-cpp
		/// </summary>
		static readonly VersionNumberRange[] PreferredIntelOneApiVersions =
		{
			VersionNumberRange.Parse("2024.0.0", "2024.9999"),
		};

		/// <summary>
		/// The minimum supported Intel compiler
		/// </summary>
		static readonly VersionNumber MinimumIntelOneApiVersion = new VersionNumber(2024, 0, 0);

		/// <inheritdoc/>
		protected override void GetValidSoftwareVersionRange(out string? minVersion, out string? maxVersion)
		{
			minVersion = MinimumWindowsSDKVersion?.ToString();
			maxVersion = MaximumWindowsSDKVersion?.ToString();
		}

		/// <summary>
		/// If a toolchain version is a preferred version
		/// </summary>
		/// <param name="toolchain">The toolchain type</param>
		/// <param name="version">The version number</param>
		/// <returns>If the version is preferred</returns>
		public static bool IsPreferredVersion(WindowsCompiler toolchain, VersionNumber version)
		{
			if (toolchain.IsMSVC())
			{
				return PreferredVisualCppVersions.Any(x => x.Contains(version));
			}
			else if (toolchain.IsClang())
			{
				return PreferredClangVersions.Any(x => x.Contains(version));
			}
			else if (toolchain.IsIntel())
			{
				return PreferredIntelOneApiVersions.Any(x => x.Contains(version));
			}
			return false;
		}

		/// <summary>
		/// Get the latest preferred toolchain version
		/// </summary>
		/// <param name="toolchain">The toolchain type</param>
		/// <returns>The version number</returns>
		public static VersionNumber GetLatestPreferredVersion(WindowsCompiler toolchain)
		{
			if (toolchain.IsMSVC())
			{
				return PreferredVisualCppVersions.Select(x => x.Min).Max()!;
			}
			else if (toolchain.IsClang())
			{
				return PreferredClangVersions.Select(x => x.Min).Max()!;
			}
			else if (toolchain.IsIntel())
			{
				return PreferredIntelOneApiVersions.Select(x => x.Min).Max()!;
			}
			return new VersionNumber(0);
		}

		/// <summary>
		/// The minimum supported Clang version for a given MSVC toolchain
		/// </summary>
		/// <param name="vcVersion"></param>
		/// <returns></returns>
		public static VersionNumber GetMinimumClangVersionForVcVersion(VersionNumber vcVersion)
		{
			return MinimumRequiredClangVersion.FirstOrDefault(x => vcVersion >= x.Item1)?.Item2 ?? MinimumClangVersion;
		}

		/// <summary>
		/// The base Clang version for a given Intel toolchain
		/// </summary>
		/// <param name="intelCompilerPath"></param>
		/// <returns></returns>
		public static VersionNumber GetClangVersionForIntelCompiler(FileReference intelCompilerPath)
		{
			FileReference ldLLdPath = FileReference.Combine(intelCompilerPath.Directory, "compiler", "ld.lld.exe");
			if (FileReference.Exists(ldLLdPath))
			{
				FileVersionInfo versionInfo = FileVersionInfo.GetVersionInfo(ldLLdPath.FullName);
				VersionNumber version = new VersionNumber(versionInfo.FileMajorPart, versionInfo.FileMinorPart, versionInfo.FileBuildPart);
				return version;
			}

			return MinimumClangVersion;
		}

		/// <summary>
		/// Whether toolchain errors should be ignored. Enable to ignore banned toolchains when generating projects,
		/// as components such as the recommended toolchain can be installed by opening the generated solution via the .vsconfig file.
		/// If enabled the error will be downgraded to a warning.
		/// </summary>
		public static bool IgnoreToolchainErrors { get; set; } = false;
	}
}
