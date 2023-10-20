// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Base class for platform-specific project generators
	/// </summary>
	class AndroidProjectGenerator : PlatformProjectGenerator
	{
		/// <summary>
		/// Whether Android Game Development Extension is installed in the system. See https://developer.android.com/games/agde for more details.
		/// May be disabled by using -noagde on commandline
		/// </summary>
		private bool AGDEInstalled = false;

		public AndroidProjectGenerator(CommandLineArguments Arguments, ILogger Logger)
			: base(Arguments, Logger)
		{
			AGDEInstalled = false;
			if (OperatingSystem.IsWindows() && !Arguments.HasOption("-noagde"))
			{
				AGDEInstalled = Microsoft.Win32.Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Google\AndroidGameDevelopmentExtension")?.ValueCount > 0;

				if (!AGDEInstalled)
				{
					try
					{
						string? programFiles86 = Environment.GetEnvironmentVariable("ProgramFiles(x86)");
						if (programFiles86 != null)
						{
							string vswhereExe = Path.Join(programFiles86, @"Microsoft Visual Studio\Installer\vswhere.exe");
							if (File.Exists(vswhereExe))
							{
								using (Process p = new Process())
								{
									ProcessStartInfo info = new ProcessStartInfo
									{
										FileName = vswhereExe,
										Arguments = @"-find Common7\IDE\Extensions\*\Google.VisualStudio.Android.dll",
										RedirectStandardOutput = true,
										UseShellExecute = false
									};
									p.StartInfo = info;
									p.Start();
									AGDEInstalled = p.StandardOutput.ReadToEnd().Contains("Google.VisualStudio.Android.dll");
								}
							}
						}
					}
					catch (Exception ex)
					{
						Logger.LogInformation("Failed to identify AGDE installation status: {Message}", ex.Message);
					}
				}
			}
		}

		/// <summary>
		/// Enumerate all the platforms that this generator supports
		/// </summary>
		public override IEnumerable<UnrealTargetPlatform> GetPlatforms()
		{
			yield return UnrealTargetPlatform.Android;
		}

		/// <inheritdoc/>
		public override bool HasVisualStudioSupport(VSSettings InVSSettings)
		{
			// Debugging, etc. are dependent on the TADP being installed
			return AGDEInstalled;
		}


		/// <inheritdoc/>
		public override string GetVisualStudioPlatformName(VSSettings InVSSettings)
		{
			string PlatformName = InVSSettings.Platform.ToString();

			if (InVSSettings.Platform == UnrealTargetPlatform.Android && AGDEInstalled)
			{
				PlatformName = "Android-arm64-v8a";
			}

			return PlatformName;
		}

		/// <inheritdoc/>
		public override void GetAdditionalVisualStudioPropertyGroups(VSSettings InVSSettings, StringBuilder ProjectFileBuilder)
		{
			if (AGDEInstalled)
			{
				base.GetAdditionalVisualStudioPropertyGroups(InVSSettings, ProjectFileBuilder);
			}
		}

		/// <inheritdoc/>
		public override void GetVisualStudioPathsEntries(VSSettings InVSSettings, TargetType TargetType, FileReference TargetRulesPath, FileReference ProjectFilePath, FileReference NMakeOutputPath, StringBuilder ProjectFileBuilder)
		{
			if (AGDEInstalled)
			{
				string apkLocation = Path.Combine(
					Path.GetDirectoryName(NMakeOutputPath.FullName)!,
					Path.GetFileNameWithoutExtension(NMakeOutputPath.FullName) + "-arm64.apk");

				ProjectFileBuilder.AppendLine($"    <AndroidApkLocation>{apkLocation}</AndroidApkLocation>");
				string intermediateRootPath = Path.GetFullPath(Path.GetDirectoryName(NMakeOutputPath.FullName) + @"\..\..\Intermediate\Android\");
				string intermediatePath = Path.Combine(intermediateRootPath, "arm64");
				string intermediateAGDESymbolsPath = Path.Combine(intermediateRootPath, "LLDBSymbolsLibs", "arm64");
				List<string> symbolLocations = new List<string>
				{
					$@"{intermediatePath}jni\arm64-v8a",
					$@"{intermediatePath}libs\arm64-v8a",
					intermediateAGDESymbolsPath // support bDontBundleLibrariesInAPK
				};
				ProjectFileBuilder.AppendLine($"    <AndroidSymbolDirectories>{string.Join(";", symbolLocations)}</AndroidSymbolDirectories>");

				// At this stage we don't know if bDontBundleLibrariesInAPK is enabled or not, so make a fail-safe check.
				string pushSOScript = Path.Combine(
					Path.GetDirectoryName(NMakeOutputPath.FullName)!,
					"Push_" + Path.GetFileNameWithoutExtension(NMakeOutputPath.FullName) + "-arm64_so.bat");
				ProjectFileBuilder.AppendLine($"    <AndroidPostApkInstallCommands>IF EXIST {pushSOScript} {pushSOScript};$(AndroidPostApkInstallCommands)</AndroidPostApkInstallCommands>");
			}
			else
			{
				base.GetVisualStudioPathsEntries(InVSSettings, TargetType, TargetRulesPath, ProjectFilePath, NMakeOutputPath, ProjectFileBuilder);
			}
		}

		public override string GetExtraBuildArguments(VSSettings InVSSettings)
		{
			// do not need to check InPlatform since it will always be UnrealTargetPlatform.Android
			return (AGDEInstalled ? " -Architectures=arm64 -ForceAPKGeneration" : "") + base.GetExtraBuildArguments(InVSSettings);
		}

		public override string GetVisualStudioUserFileStrings(VSSettings InVSSettings,
			string InConditionString, TargetRules InTargetRules, FileReference TargetRulesPath, FileReference ProjectFilePath)
		{
			if (AGDEInstalled
				&& (InVSSettings.Platform == UnrealTargetPlatform.Android)
				&& ((InTargetRules.Type == TargetRules.TargetType.Client) || (InTargetRules.Type == TargetRules.TargetType.Game)))
			{
				string UserFileEntry = "<PropertyGroup " + InConditionString + ">\n";
				UserFileEntry += "	<AndroidLldbStartupCommands>" +
												"command script import \"" + Path.Combine(Unreal.EngineDirectory.FullName, "Extras", "LLDBDataFormatters", "UEDataFormatters_2ByteChars.py") + "\";" +
												"$(AndroidLldbStartupCommands)" +
											"</AndroidLldbStartupCommands>\n";
				UserFileEntry += "</PropertyGroup>\n";
				return UserFileEntry;
			}

			return base.GetVisualStudioUserFileStrings(InVSSettings, InConditionString, InTargetRules, TargetRulesPath, ProjectFilePath);
		}
	}
}
