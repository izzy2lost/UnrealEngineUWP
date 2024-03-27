// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Threading;
using System.Text.RegularExpressions;
using System.Linq;
using EpicGames.Core;

namespace Gauntlet
{
	public class IOSBuild : IBuild
	{
		public int PreferenceOrder { get { return 0; } }

		public UnrealTargetConfiguration Configuration { get; protected set; }

		public string SourcePath;

		public bool IsIPAFile;

		public Dictionary<string, string> FilesToInstall;

		public string PackageName;

		public BuildFlags Flags { get; protected set; }

		public string Flavor { get { return ""; } }

		public UnrealTargetPlatform Platform { get { return UnrealTargetPlatform.IOS; } }

		public bool SupportsAdditionalFileCopy { get; }

		public IOSBuild(UnrealTargetConfiguration InConfig, string InPackageName, string InSourcePath, Dictionary<string, string> InFilesToInstall, BuildFlags InFlags)
		{
			Configuration = InConfig;
			PackageName = InPackageName;
			SourcePath = InSourcePath;
			FilesToInstall = InFilesToInstall;
			Flags = InFlags;
			SupportsAdditionalFileCopy = true;
			IsIPAFile = Path.GetExtension(InSourcePath).Equals(".ipa", StringComparison.OrdinalIgnoreCase);
		}

		public bool CanSupportRole(UnrealTargetRole RoleType)
		{
			if (RoleType.IsClient())
			{
				return true;
			}

			return false;
		}

		internal static IProcessResult ExecuteCommand(String Command, String Arguments)
		{
			CommandUtils.ERunOptions RunOptions = CommandUtils.ERunOptions.AppMustExist;

			if (Log.IsVeryVerbose)
			{
				RunOptions |= CommandUtils.ERunOptions.AllowSpew;
			}
			else
			{
				RunOptions |= CommandUtils.ERunOptions.NoLoggingOfRunCommand;
			}

			Log.Verbose("Executing '{0} {1}'", Command, Arguments);

			IProcessResult Result = CommandUtils.Run(Command, Arguments, Options: RunOptions);

			return Result;
		}

		// There are issues with IPA Zip64 files being created with Ionic.Zip possibly limited to when running on mono (see IOSPlatform.PackageIPA)
		// This manifests as header overflow errors, etc in 7zip, Ionic.Zip, System.IO.Compression, and OSX system unzip		
		internal static bool ExecuteIPAZipCommand(String Arguments, out String Output, String ShouldExist = "")
		{
			using (new ScopedSuspendECErrorParsing())
			{
				IProcessResult Result = ExecuteCommand("unzip", Arguments);
				Output = Result.Output;

				if (Result.ExitCode != 0)
				{
					if (!String.IsNullOrEmpty(ShouldExist))
					{
						if (!File.Exists(ShouldExist) && !Directory.Exists(ShouldExist))
						{
							Log.Error(KnownLogEvents.Gauntlet_BuildDropEvent, "unzip encountered an error or warning procesing IPA, possibly due to Zip64 issue, {File} missing", ShouldExist);
							return false;
						}
					}

					Log.Info(String.Format("unzip encountered an issue procesing IPA, possibly due to Zip64. Future steps may fail."));
				}
			}

			return true;
		}

		// IPA handling using ditto command, which is capable of handling IPA's > 4GB/Zip64
		internal static bool ExecuteIPADittoCommand(String Arguments, out String Output, String ShouldExist = "")
		{
			using (new ScopedSuspendECErrorParsing())
			{
				IProcessResult Result = ExecuteCommand("ditto", Arguments);
				Output = Result.Output;

				if (Result.ExitCode != 0)
				{
					if (!String.IsNullOrEmpty(ShouldExist))
					{
						if (!File.Exists(ShouldExist) && !Directory.Exists(ShouldExist))
						{
							Log.Error(String.Format("ditto encountered an error or warning procesing IPA, {0} missing", ShouldExist));
							return false;
						}
					}

					Log.Error(String.Format("ditto encountered an issue procesing IPA"));
					return false;

				}
			}

			return true;
		}


		private static string GetBundleIdentifier(string Source)
		{
			string PlistInfo = string.Empty;
			bool IsIPAFile = Path.GetExtension(Source).Equals(".ipa", StringComparison.OrdinalIgnoreCase);

			if (IsIPAFile)
			{
				string Output;

				// Get a list of files in the IPA
				if (!ExecuteIPAZipCommand(String.Format("-Z1 {0}", Source), out Output))
				{
					Log.Warning(String.Format("Unable to list files for IPA {0}", Source));
					return null;
				}


				string[] Filenames = Regex.Split(Output, "\r\n|\r|\n");
				string PList = Filenames.Where(F => Regex.IsMatch(F.ToLower().Trim(), @"(payload\/)([^\/]+)(\/info\.plist)")).FirstOrDefault();

				if (String.IsNullOrEmpty(PList))
				{
					Log.Warning(String.Format("Unable to find plist for IPA {0}", Source));
					return null;
				}

				// Get the plist info
				if (!ExecuteIPAZipCommand(String.Format("-p '{0}' '{1}'", Source, PList), out Output))
				{
					Log.Warning(String.Format("Unable to extract plist data for IPA {0}", Source));
					return null;
				}

				PlistInfo = Output;
			}
			else
			{
				// Find plist file
				DirectoryInfo Di = new DirectoryInfo(Source);
				FileInfo PlistFile = new FileInfo(Path.Combine(Di.FullName, "Info.plist"));
				if (!PlistFile.Exists)
				{
					Log.Warning(String.Format("Unable to find plist from {0}", Source));
					return null;
				}

				StreamReader PListStream = new StreamReader(PlistFile.FullName);
				PlistInfo = PListStream.ReadToEnd();
			}

			// todo: plist parsing, could be better
			string PackageName = null;
			string KeyString = "<key>CFBundleIdentifier</key>";
			int KeyIndex = PlistInfo.IndexOf(KeyString);
			if (KeyIndex > 0)
			{
				int StartIdx = PlistInfo.IndexOf("<string>", KeyIndex + KeyString.Length) + "<string>".Length;
				int EndIdx = PlistInfo.IndexOf("</string>", StartIdx);
				if (StartIdx > 0 && EndIdx > StartIdx)
				{
					PackageName = PlistInfo.Substring(StartIdx, EndIdx - StartIdx);
				}
			}

			if (String.IsNullOrEmpty(PackageName))
			{
				Log.Warning(String.Format("Unable to find CFBundleIdentifier in plist info for App {0}", Source));
				return null;
			}

			Log.Verbose("Found bundle id: {0}", PackageName);

			return PackageName;

		}

		public static IOSBuild CreateFromPath(string InProjectName, string InPath)
		{
			string BuildPath = InPath;

			IOSBuild DiscoveredBuilds = null;

			DirectoryInfo Di = new DirectoryInfo(BuildPath);

			var UnrealConfig = UnrealHelpers.GetConfigurationFromExecutableName(InProjectName, Di.Name);

			if (UnrealConfig != UnrealTargetConfiguration.Unknown)
			{
				// check there's an executable with the right name 
				string ShortName = Regex.Replace(InProjectName, "Game", "", RegexOptions.IgnoreCase);
				FileInfo Executable = new DirectoryInfo(InPath).GetFiles().Where(Fi => Fi.Name.StartsWith(ShortName, StringComparison.OrdinalIgnoreCase)).FirstOrDefault();
				if (Executable != null)
				{
					Log.Verbose("Pulling package data from {0}", Di.FullName);

					string AbsPath = Di.FullName;

					// IOS builds are always packaged, and can always replace the command line and executable as we cache the unzip'd IPA
					BuildFlags Flags = BuildFlags.Packaged | BuildFlags.CanReplaceCommandLine | BuildFlags.CanReplaceExecutable;

					if (AbsPath.Contains("Bulk"))
					{
						Flags |= BuildFlags.Bulk;
					}
					else
					{
						Flags |= BuildFlags.NotBulk;
					}

					string PackageName = GetBundleIdentifier(Di.FullName);

					if (!String.IsNullOrEmpty(PackageName))
					{
						Dictionary<string, string> FilesToInstall = new Dictionary<string, string>();

						DiscoveredBuilds = new IOSBuild(UnrealConfig, PackageName, Di.FullName, FilesToInstall, Flags);

						Log.Verbose("Found {0} {1} build at {2}", UnrealConfig, ((Flags & BuildFlags.Bulk) == BuildFlags.Bulk) ? "(bulk)" : "(not bulk)", AbsPath);
					}
				}
			}

			return DiscoveredBuilds;
		}
	}

	public class IOSBuildSource : IFolderBuildSource
	{
		public string BuildName { get { return "IOSBuildSource"; } }

		public bool CanSupportPlatform(UnrealTargetPlatform InPlatform)
		{
			return InPlatform == UnrealTargetPlatform.IOS;
		}

		public string ProjectName { get; protected set; }

		public IOSBuildSource()
		{
		}

		public List<IBuild> GetBuildsAtPath(string InProjectName, string InPath, int MaxRecursion = 3)
		{
			// We only want iOS builds on Mac host
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				return new List<IBuild>();
			}

			List<IBuild> Builds = new List<IBuild>();

			// c:\path\to\build
			DirectoryInfo PathDI = new DirectoryInfo(InPath);

			if (PathDI.Exists)
			{
				List<DirectoryInfo> SearchDirs = new List<DirectoryInfo>();

				if (PathDI.Name.IndexOf("IOS", StringComparison.OrdinalIgnoreCase) >= 0)
				{
					SearchDirs.Add(PathDI);
				}
				else
				{
					// find all directories that begin with IOS
					SearchDirs.AddRange(PathDI.GetDirectories("IOS*", SearchOption.TopDirectoryOnly));
				}

				IEnumerable<DirectoryInfo> DirsToRecurse = new List<DirectoryInfo>(SearchDirs);

				List<DirectoryInfo> AllDirs = new List<DirectoryInfo>();

				// get subdirs
				while (MaxRecursion-- > 0)
				{
					IEnumerable<DirectoryInfo> DiscoveredDirs = DirsToRecurse.SelectMany(D => D.GetDirectories("*", SearchOption.TopDirectoryOnly));

					// mac packages are folders so we only want things that end with .app
					IEnumerable<DirectoryInfo> Packages = DiscoveredDirs.Where(D => Path.GetExtension(D.Name).Equals(".app", StringComparison.OrdinalIgnoreCase));

					AllDirs.AddRange(Packages);

					// don't recurse into the dirs that end with .app since those are builds we'll look at
					DirsToRecurse = DiscoveredDirs.Except(Packages);
				}

				string IOSBuildFilter = Globals.Params.ParseValue("IOSBuildFilter", "");
				foreach (DirectoryInfo Di in AllDirs)
				{
					IOSBuild FoundBuild = IOSBuild.CreateFromPath(InProjectName, Di.FullName);

					if (FoundBuild != null)
					{
						if (!string.IsNullOrEmpty(IOSBuildFilter) && FoundBuild.SourcePath.IndexOf(IOSBuildFilter, StringComparison.OrdinalIgnoreCase) >= 0)
						{
							continue;
						}
						Builds.Add(FoundBuild);
					}
				}
			}

			return Builds;
		}

	}
}