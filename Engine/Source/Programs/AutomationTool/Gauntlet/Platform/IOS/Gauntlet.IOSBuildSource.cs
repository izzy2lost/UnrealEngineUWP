// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Xml.Linq;
using System.Text.RegularExpressions;
using System.Linq;
using EpicGames.Core;

namespace Gauntlet
{
	public class AppleBuild : IBuild
	{
		public int PreferenceOrder { get { return 0; } }

		public UnrealTargetConfiguration Configuration { get; protected set; }

		public string SourcePath;

		public bool IsIPAFile;

		public Dictionary<string, string> FilesToInstall;

		public string PackageName;

		public BuildFlags Flags { get; protected set; }

		public string Flavor { get { return ""; } }

		public virtual UnrealTargetPlatform Platform { get; }

		public bool SupportsAdditionalFileCopy { get; }

		public AppleBuild(UnrealTargetConfiguration InConfig, string InPackageName, string InSourcePath, Dictionary<string, string> InFilesToInstall, BuildFlags InFlags)
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

		private static PlistInfo GetPlistInfo(string Source)
		{
			PlistInfo Info = null;
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

				Info = new PlistInfo(Output);
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
				Info = new PlistInfo(PListStream.ReadToEnd());
			}

			return Info;
		}

		private class PlistInfo
		{
			private XDocument Document;

			public PlistInfo(string InContent)
			{
				try
				{
					Document = XDocument.Parse(InContent);
				}
				catch (Exception Ex)
				{
					// Ignore errors
					Log.Warning(KnownLogEvents.Gauntlet_BuildDropEvent, "Fail to parse PlistInfo:\n{Exception}", Ex);
					Document = new XDocument();
				}
			}

			/// <summary>
			/// Get first value from corresponding key
			/// </summary>
			/// <param name="Key"></param>
			/// <returns></returns>
			public string GetFirstValue(string Key)
			{
				foreach (XElement element in Document.Descendants("key"))
				{
					if (element.Value == Key)
					{
						XElement NextElement = element.ElementsAfterSelf().FirstOrDefault();
						if (NextElement != null)
						{
							if (NextElement.Name == "string")
							{
								return NextElement.Value;
							}
							else if (NextElement.Name == "array")
							{
								return NextElement.Descendants("string").Select(e => e.Value).FirstOrDefault();
							}
						}
					}
				}

				return null;
			}

			/// <summary>
			/// Get all values from corresponding key
			/// </summary>
			/// <param name="Key"></param>
			/// <returns></returns>
			public IEnumerable<string> GetAllValues(string Key)
			{
				foreach (XElement element in Document.Descendants("key"))
				{
					if (element.Value == Key)
					{
						XElement NextElement = element.ElementsAfterSelf().FirstOrDefault();
						if (NextElement != null)
						{
							if (NextElement.Name == "array")
							{
								return NextElement.Descendants("string").Select(e => e.Value);
							}
							else if (NextElement.Name == "string")
							{
								return new List<string> { NextElement.Value };
							}
						}
					}
				}

				return null;
			}
		}

		public static T CreateFromPath<T>(string InProjectName, string InPath, AppleBuildSource<T> BuildSource)
			where T : AppleBuild
		{
			T DiscoveredBuild = null;

			DirectoryInfo Di = new DirectoryInfo(InPath);

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

					PlistInfo Info = GetPlistInfo(Di.FullName);
					if (Info != null)
					{
						IEnumerable<string> CFBundlePlatformNames = Info.GetAllValues("CFBundleSupportedPlatforms");
						if (CFBundlePlatformNames != null && CFBundlePlatformNames.Contains(BuildSource.CFBundlePlatformName))
						{
							string PackageName = Info.GetFirstValue("CFBundleIdentifier");

							if (!String.IsNullOrEmpty(PackageName))
							{
								Dictionary<string, string> FilesToInstall = new Dictionary<string, string>();

								DiscoveredBuild = Activator.CreateInstance(typeof(T), new object[] { UnrealConfig, PackageName, Di.FullName, FilesToInstall, Flags }) as T;

								Log.Verbose("Found bundle id: {0}", PackageName);
								Log.Verbose("Found {0} {1} build at {2}", UnrealConfig, ((Flags & BuildFlags.Bulk) == BuildFlags.Bulk) ? "(bulk)" : "(not bulk)", AbsPath);
							}
							else
							{
								Log.Warning(String.Format("Unable to find CFBundleIdentifier in plist info for App {0}", Di.FullName));
							}
						}
						else
						{
							Log.Verbose("Unable to find matching platform '{0}' for CFBundleSupportedPlatforms in plist info for App {1}", BuildSource.CFBundlePlatformName, Di.FullName);
						}
					}
				}
			}

			return DiscoveredBuild;
		}
	}

	public abstract class AppleBuildSource<T> : IFolderBuildSource
		where T : AppleBuild
	{
		protected abstract UnrealTargetPlatform Platform { get; }

		public abstract string CFBundlePlatformName { get; }

		public string BuildName { get { return $"{Platform}BuildSource"; } }

		public bool CanSupportPlatform(UnrealTargetPlatform InPlatform)
		{
			return InPlatform == Platform;
		}

		public string ProjectName { get; protected set; }

		public virtual List<IBuild> GetBuildsAtPath(string InProjectName, string InPath, int MaxRecursion = 3)
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

				if (PathDI.Name.IndexOf(Platform.ToString(), StringComparison.OrdinalIgnoreCase) >= 0)
				{
					SearchDirs.Add(PathDI);
				}
				else
				{
					// find all directories that begin with IOS
					SearchDirs.AddRange(PathDI.GetDirectories($"{Platform}*", SearchOption.TopDirectoryOnly));
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

				string IOSBuildFilter = Globals.Params.ParseValue($"{Platform}BuildFilter", "");
				foreach (DirectoryInfo Di in AllDirs)
				{
					AppleBuild FoundBuild = AppleBuild.CreateFromPath<T>(InProjectName, Di.FullName, this);

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

	public class IOSBuild : AppleBuild
	{
		public IOSBuild(UnrealTargetConfiguration InConfig, string InPackageName, string InSourcePath, Dictionary<string, string> InFilesToInstall, BuildFlags InFlags)
			: base(InConfig, InPackageName, InSourcePath, InFilesToInstall, InFlags)
		{ }

		public override UnrealTargetPlatform Platform => UnrealTargetPlatform.IOS;
	}

	public class IOSBuildSource : AppleBuildSource<IOSBuild>
	{
		protected override UnrealTargetPlatform Platform => UnrealTargetPlatform.IOS;
		public override string CFBundlePlatformName => "iPhoneOS";
	}

	public class TVOSBuild : AppleBuild
	{
		public TVOSBuild(UnrealTargetConfiguration InConfig, string InPackageName, string InSourcePath, Dictionary<string, string> InFilesToInstall, BuildFlags InFlags)
			: base(InConfig, InPackageName, InSourcePath, InFilesToInstall, InFlags)
		{ }

		public override UnrealTargetPlatform Platform => UnrealTargetPlatform.TVOS;
	}

	public class TVOSBuildSource : AppleBuildSource<TVOSBuild>
	{
		protected override UnrealTargetPlatform Platform => UnrealTargetPlatform.TVOS;
		public override string CFBundlePlatformName => "AppleTVOS";
	}
}