// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
// @ATG_CHANGE : BEGIN UWP Packaging support
using System.Diagnostics;
// @ATG_CHANGE : END
using System.Linq;
using System.IO;
using System.Resources;
using System.Xml;

namespace UnrealBuildTool
{
	/// <summary>
	///  Class to handle generating an AppxManifest.xml file
	/// </summary>
	// @ATG_CHANGE : BEGIN: originally moved this to a separate generic class, but reverting to
	//                                              putting everything inline for ease of taking Epic integrations for now.
	public class PackageResourceGenerator
	// @ATG_CHANGE : END
	{
		// @ATG_CHANGE : BEGIN UWP Packaging support
		private FileReference ProjectFile;
		private ConfigCacheIni EngineIni;
		private ConfigCacheIni GameIni;
		private UnrealTargetPlatform Platform;
		// @ATG_CHANGE : END

		// @ATG_CHANGE : BEGIN UWP Packaging support
		/// <summary>
		/// Constructor
		/// </summary>
		public PackageResourceGenerator(UnrealTargetPlatform InPlatform, FileReference InProjectFile)
		{
			Platform = InPlatform;
			ProjectFile = InProjectFile;
		}
		// @ATG_CHANGE : END

		/// <summary>
		/// Generates the localized resource tables, combines them into the localized resource directories with images, and generates
		/// the PRI index to govern them.
		/// </summary>
		/// <param name="OutputPath">     Path to base resource tree in and location for pri file. (The package/deploy root.)</param>
		/// <param name="IntermediatePath">   Path to store temporary intermediate data (e.g. XML resource file).</param>
		/// <param name="">LocalContentPath    Path to localized content storage (<Project>/Content/Localization/<Target>).</param>
		/// <param name="ProjectPathParam">Path to the project.</param>
		/// <param name="">AppxManifestPath    Path to the AppxManifest file.</param>
		/// <returns>string[]   List of files that have been updated (if any)</returns>
		public List<string> GenerateResources(string OutputPath, string IntermediatePath, string LocalContentPath, string ProjectPathParam, string AppxManifestPath)
		{
			List<string> UpdatedFilePaths = new List<string>();

			// Attempt to correct or handle incorrect OutputPath
			if (File.Exists(OutputPath))
			{
				Log.TraceWarning("Path {0} is a file. Should be a directory. Continuing using parent directory.", OutputPath);
				OutputPath = Path.Combine(OutputPath, "..");
				Utils.CollapseRelativeDirectories(ref OutputPath);
			}
			if (!Directory.Exists(OutputPath))
			{
				try
				{
					Directory.CreateDirectory(OutputPath);
				}
				catch (Exception)
				{
					Log.TraceError("Could not create directory {0}.", OutputPath);
					return UpdatedFilePaths;
				}
				if (!Directory.Exists(OutputPath))
				{
					Log.TraceError("Path {0} does not exist or is not a directory.", OutputPath);
					return UpdatedFilePaths;
				}
			}
			// Attempt to correct or handle incorrect IntermediatePath
			if (File.Exists(IntermediatePath))
			{
				Log.TraceWarning("Path {0} is a file. Should be a directory. Continuing using parent directory.", IntermediatePath);
				IntermediatePath = Path.Combine(IntermediatePath, "..");
				Utils.CollapseRelativeDirectories(ref IntermediatePath);
			}
			if (!Directory.Exists(IntermediatePath))
			{
				try
				{
					Directory.CreateDirectory(IntermediatePath);
				}
				catch (Exception)
				{
					Log.TraceError("Could not create directory {0}.", IntermediatePath);
					return UpdatedFilePaths;
				}
				if (!Directory.Exists(IntermediatePath))
				{
					Log.TraceError("Path {0} does not exist or is not a directory.", IntermediatePath);
					return UpdatedFilePaths;
				}
			}
			// Attempt to correct or handle incorrect LocalContentPath
			if (File.Exists(LocalContentPath))
			{
				Log.TraceWarning("Path {0} is a file. Should be a directory. Continuing using parent directory.", LocalContentPath);
				LocalContentPath = Path.Combine(LocalContentPath, "..");
				Utils.CollapseRelativeDirectories(ref LocalContentPath);
			}
			if (!Directory.Exists(LocalContentPath))
			{
				try
				{
					Directory.CreateDirectory(LocalContentPath);
				}
				catch (Exception)
				{
					Log.TraceError("Could not create directory {0}.", LocalContentPath);
					return UpdatedFilePaths;
				}
				if (!Directory.Exists(LocalContentPath))
				{
					Log.TraceError("Path {0} does not exist or is not a directory.", LocalContentPath);
					return UpdatedFilePaths;
				}
			}

			// Load up INI settings. We'll use engine settings to retrieve the manifest configuration and most other resource
			// settings from the game settings.
			// @ATG_CHANGE : BEGIN UWP Packaging support
			GameIni = ConfigCacheIni.CreateConfigCacheIni(Platform, "Game", DirectoryReference.FromFile(ProjectFile));
			EngineIni = ConfigCacheIni.CreateConfigCacheIni(Platform, "Engine", DirectoryReference.FromFile(ProjectFile));
			// @ATG_CHANGE : END

			List<string> CulturesToStageWithDuplicates = null;
			List<string> CulturesToStage = null;
			string DefaultCulture = null;
			GameIni.GetArray("/Script/UnrealEd.ProjectPackagingSettings", "CulturesToStage", out CulturesToStageWithDuplicates);
			GameIni.GetString("/Script/UnrealEd.ProjectPackagingSettings", "DefaultCulture", out DefaultCulture);

			if (CulturesToStageWithDuplicates == null || CulturesToStageWithDuplicates.Count < 1)
			{
				Log.TraceError("At least one culture must be selected to stage.");
				return UpdatedFilePaths;
			}
			if (DefaultCulture == null || DefaultCulture.Length < 1)
			{
				Log.TraceError("A default culture must be selected to stage.");
				return UpdatedFilePaths;
			}
			if (!CulturesToStageWithDuplicates.Contains(DefaultCulture))
			{
				Log.TraceError("The default culture must be one of the staged cultures.");
				return UpdatedFilePaths;
			}

			// De-duplicate any cultures. We only stage once for each.
			CulturesToStage = CulturesToStageWithDuplicates.Distinct().ToList();

			List<ResXResourceWriter> ResXWriters = new List<ResXResourceWriter>();
			foreach (string Culture in CulturesToStage)
			{
				string IntermediateStringResourcePath = Path.Combine(IntermediatePath, Culture);
				string IntermediateStringResourceFile = Path.Combine(IntermediateStringResourcePath, "resources.resw");
				if (!Directory.Exists(IntermediateStringResourcePath))
				{
					try
					{
						Directory.CreateDirectory(IntermediateStringResourcePath);
					}
					catch (Exception)
					{
						Log.TraceError("Could not create directory {0}.", IntermediateStringResourcePath);
						return UpdatedFilePaths;
					}
					if (!Directory.Exists(IntermediateStringResourcePath))
					{
						Log.TraceError("Path {0} does not exist or is not a directory.", IntermediateStringResourcePath);
						return UpdatedFilePaths;
					}
				}
				ResXWriters.Add(new ResXResourceWriter(IntermediateStringResourceFile));
			}

			ConfigCacheIni.IniSection AppxManifestIniSection = EngineIni.FindSection("AppxManifest");
			foreach (KeyValuePair<string, ConfigCacheIni.IniValues> AppxManifestIniSetting in AppxManifestIniSection)
			{
				if (AppxManifestIniSetting.Value.Count > 1)
				{
					Log.TraceWarning("Ini setting {0} contains multiple values. This is not supported for this value type.", AppxManifestIniSetting.Key);
				}

				string AppxManifestIniSettingValue = AppxManifestIniSetting.Value[0];

				const string ResourceStringTag = "%ResourceString:";
				if (AppxManifestIniSettingValue.Contains(ResourceStringTag))
				{
					// Parse manifest value to find key name
					int SectionKeyPairStart = AppxManifestIniSettingValue.IndexOf(ResourceStringTag) + ResourceStringTag.Length;
					int SectionKeyPairLen = AppxManifestIniSettingValue.IndexOf('%', SectionKeyPairStart) - SectionKeyPairStart;
					string SectionKeyPair = AppxManifestIniSettingValue.Substring(SectionKeyPairStart, SectionKeyPairLen);
					string SettingSection = SectionKeyPair.Substring(0, SectionKeyPair.IndexOf(':'));
					string SettingKey = SectionKeyPair.Substring(SectionKeyPair.IndexOf(':') + 1);
					String SettingValue = null;

					// Output for each culture
					for (int CultureIndex = 0; CultureIndex < CulturesToStage.Count; CultureIndex++)
					{
						//@todo get from localized strings, not INIs
						GameIni.GetString(SettingSection, SettingKey, out SettingValue);
						// If not found in Game INIs, search for the same Key in Engine INIs
						if (SettingValue == null || SettingValue.Length == 0)
						{
							EngineIni.GetString(SettingSection, SettingKey, out SettingValue);
							// @ATG_CHANGE : BEGIN 
							if (SettingValue == null || SettingValue.Length == 0)
							{
								// Still empty?  Warn and fill in a default
								Log.TraceWarning("Config string {0} referenced as resource is not set", SectionKeyPair);
								SettingValue = "MISSING STRING " + SectionKeyPair;
							}
							// @ATG_CHANGE : END 
						}

						if (SettingValue != null && SettingValue.Length > 0)
						{
							ResXWriters[CultureIndex].AddResource(SettingKey, SettingValue);
						}
					}
				}

				const string ResourceBinaryTag = "%ResourceBinary:";
				if (AppxManifestIniSettingValue.Contains(ResourceBinaryTag))
				{
					// Parse manifest value to find key name
					int SectionKeyPairStart = AppxManifestIniSettingValue.IndexOf(ResourceBinaryTag) + ResourceBinaryTag.Length;
					int SectionKeyPairLen = AppxManifestIniSettingValue.IndexOf('%', SectionKeyPairStart) - SectionKeyPairStart;
					string SectionKeyPair = AppxManifestIniSettingValue.Substring(SectionKeyPairStart, SectionKeyPairLen);
					string SettingSection = SectionKeyPair.Substring(0, SectionKeyPair.IndexOf(':'));
					string SettingKey = SectionKeyPair.Substring(SectionKeyPair.IndexOf(':') + 1);
					String SettingValue = null;

					// Output for each culture
					for (int CultureIndex = 0; CultureIndex < CulturesToStage.Count; CultureIndex++)
					{
						//@todo get from localized strings, not INIs
						GameIni.GetString(SettingSection, SettingKey, out SettingValue);
						// If not found in Game INIs, search for the same Key in Engine INIs
						if (SettingValue == null || SettingValue.Length == 0)
						{
							EngineIni.GetString(SettingSection, SettingKey, out SettingValue);
						}

						// @ATG_CHANGE : BEGIN 
						// If we still don't find a customized value, use the default source resources folder with the key
						// as the file name.
						if (SettingValue == null || SettingValue.Length == 0)
						{
							string PlatformDir = (Platform == UnrealTargetPlatform.XboxOne ? "XboxOne" : "UWP");
							SettingValue = Path.Combine("Build", PlatformDir, "Resources", SettingKey) + ".png";
						}
						// @ATG_CHANGE : END 

						if (SettingValue != null && SettingValue.Length > 0)
						{
							string SourceFile = Path.Combine(ProjectPathParam, SettingValue);

							// Check that we have a valid source file
							if (!File.Exists(SourceFile))
							{
								// Try falling back to the engine directory
								Log.TraceWarning("Required binary source file does not exist in project. {0}", SourceFile);
								SourceFile = Path.Combine(BuildConfiguration.RelativeEnginePath, SettingValue);
								if (!File.Exists(SourceFile))
								{
									Log.TraceError("Required binary file could not be found in project or engine paths. {0}", SourceFile);
									continue;
								}
							}

							string DestPath = Path.Combine(OutputPath, "Resources", CulturesToStage[CultureIndex]);
							// @ATG_CHANGE : BEGIN resource qualifier support
							string BaseImageSuffix = (Platform == UnrealTargetPlatform.XboxOne ? ".png" : ".scale-100.png");
							string DestFile = Path.Combine(DestPath, SettingKey + BaseImageSuffix);

							// Copy file into destination
							if (CopySingleResourceFile(SourceFile, DestFile))
							{
								UpdatedFilePaths.Add(DestFile);
							}

							// Qualified resources are only relevant on UWP
							if (Platform != UnrealTargetPlatform.XboxOne)
							{
								string[] FileNamePartsToRemoveQualifiers = Path.GetFileName(SourceFile).Split('.');
								if (FileNamePartsToRemoveQualifiers.Length >= 2)
								{
									string FilePatternAllowingQualifiers = string.Format("{0}.*.{1}", FileNamePartsToRemoveQualifiers[0], FileNamePartsToRemoveQualifiers[FileNamePartsToRemoveQualifiers.Length - 1]);
									IEnumerable<string> VariantsWithQualifiers = Directory.EnumerateFiles(Path.GetDirectoryName(SourceFile), FilePatternAllowingQualifiers);
									foreach (string VariantFile in VariantsWithQualifiers)
									{
										string[] VariantFileParts = VariantFile.Split('.');
										string VariantTypePortion = VariantFileParts[VariantFileParts.Length - 2];
										string VariantDestFile = Path.Combine(DestPath, SettingKey + "." + VariantTypePortion + ".png");
										if (CopySingleResourceFile(VariantFile, VariantDestFile))
										{
											UpdatedFilePaths.Add(VariantDestFile);
										}
									}
								}
							}

							// The default culture must also be copied into the root of the resources tree
							if (CulturesToStage[CultureIndex].Equals(DefaultCulture))
							{
								string DefaultDestPath = Path.Combine(OutputPath, "Resources");
								string DefaultDestFile = Path.Combine(DefaultDestPath, SettingKey + ".png");
								if (CopySingleResourceFile(SourceFile, DefaultDestFile))
								{
									UpdatedFilePaths.Add(DefaultDestFile);
								}

								// Note we don't copy the qualified resources here.  If these are present
								// the indexing process will put them in a separate pri file, which we
								// don't use when packaging, so creates less mess to just leave them out.
								// They're really just fallbacks anyway, since under normal operation the
								// culture qualified versions will be used.
							}
							// @ATG_CHANGE : END resource qualifier support
						}
					}
				}
			}

			for (int CultureIndex = 0; CultureIndex < CulturesToStage.Count; CultureIndex++)
			{
				ResXWriters[CultureIndex].Close();
				//@todo compare original xml data

				string IntermediateStringResourcePath = Path.Combine(IntermediatePath, CulturesToStage[CultureIndex], "resources.resw");
				//@todo from project settings
				string FinalStringResourcePath = Path.Combine(OutputPath, "Resources", CulturesToStage[CultureIndex]);
				string FinalStringResourceFile = Path.Combine(FinalStringResourcePath, "resources.resw");
				if (!Directory.Exists(FinalStringResourcePath))
				{
					try
					{
						Directory.CreateDirectory(FinalStringResourcePath);
					}
					catch (Exception)
					{
						Log.TraceError("Could not create directory {0}.", FinalStringResourcePath);
						return UpdatedFilePaths;
					}
					if (!Directory.Exists(FinalStringResourcePath))
					{
						Log.TraceError("Path {0} does not exist or is not a directory.", FinalStringResourcePath);
						return UpdatedFilePaths;
					}
				}
				if (File.Exists(FinalStringResourceFile))
				{
					try
					{
						File.Delete(FinalStringResourceFile);
					}
					catch (Exception)
					{
						Log.TraceError("Could not replace file {0}.", FinalStringResourceFile);
						return UpdatedFilePaths;
					}
				}
				File.Copy(IntermediateStringResourcePath, FinalStringResourceFile);
				UpdatedFilePaths.Add(FinalStringResourceFile);

				// The default culture must also be copied into the root of the resources tree
				if (CulturesToStage[CultureIndex].Equals(DefaultCulture))
				{
					string DefaultStringResourceFile = Path.Combine(OutputPath, "Resources", "resources.resw");
					if (File.Exists(DefaultStringResourceFile))
					{
						try
						{
							File.Delete(DefaultStringResourceFile);
						}
						catch (Exception)
						{
							Log.TraceError("Could not replace file {0}.", DefaultStringResourceFile);
							return UpdatedFilePaths;
						}
					}
					File.Copy(FinalStringResourceFile, DefaultStringResourceFile);
					UpdatedFilePaths.Add(DefaultStringResourceFile);
				}
			}

			// Create resource index configuration
			// @ATG_CHANGE : BEGIN UWP Packaging support
			string PriExecutable = GetPathToMakePriExe();
			string ResourceConfigFile = Path.Combine(IntermediatePath, "priconfig.xml");
			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = PriExecutable;
			StartInfo.Arguments = "createconfig /cf \"" + ResourceConfigFile + "\" /dq " + DefaultCulture + " /o";

			// Windows wants platform version specified
			if (Platform != UnrealTargetPlatform.XboxOne)
			{
				StartInfo.Arguments += " /pv 10.0.0";
			}
			
			StartInfo.CreateNoWindow = true;
			StartInfo.StandardErrorEncoding = System.Text.Encoding.Unicode;
			StartInfo.StandardOutputEncoding = System.Text.Encoding.Unicode;
			Utils.RunLocalProcessAndLogOutput(StartInfo);
			// @ATG_CHANGE : END

			// Modify configuration to restrict indexing to the Resources directory (saves time and space)
			XmlDocument PriConfig = new XmlDocument();
			PriConfig.Load(ResourceConfigFile);

			// @ATG_CHANGE : The Xbox One approach to limiting the indexer causes files to have dodgy uris in the 
			// generated pri e.g. ms-resource://PackageIdentityName/Files/Logo.png instead of ms-resource://PackageIdentityName/Files/Resources/Logo.png
			// This appears to affect Windows's ability to locate a valid image in some scenarios such as a
			// desktop shortcut.  So on UWP we start from the root and add exclusions.
			if (Platform == UnrealTargetPlatform.XboxOne)
			{
				XmlNode PriIndexNode = PriConfig.SelectSingleNode("/resources/index");
				XmlAttribute PriStartIndex = PriIndexNode.Attributes["startIndexAt"];
				PriStartIndex.Value = "\\Resources\\";
			}
			else
			{
				XmlNodeList ConfigNodes = PriConfig.SelectNodes("/resources/index/indexer-config");
				foreach (XmlNode ConfigNode in ConfigNodes)
				{
					if (ConfigNode.Attributes["type"].Value == "folder")
					{
						IEnumerable<string> AllSubItems = Directory.EnumerateFileSystemEntries(OutputPath);
						foreach (string FileSystemEntry in AllSubItems)
						{
							if (Path.GetFileName(FileSystemEntry) != "Resources")
							{
								XmlElement ExcludeElement = PriConfig.CreateElement("exclude");
								if (File.Exists(FileSystemEntry))
								{
									ExcludeElement.SetAttribute("type", "path");
								}
								else
								{
									ExcludeElement.SetAttribute("type", "tree");
								}
								ExcludeElement.SetAttribute("value", Path.GetFileName(FileSystemEntry));
								ExcludeElement.SetAttribute("doNotTraverse", "true");
								ExcludeElement.SetAttribute("doNotIndex", "true");
								ConfigNode.AppendChild(ExcludeElement);
							}
						}
					}
				}
			}
			PriConfig.Save(ResourceConfigFile);
			// @ATG_CHANGE : END


			// Generate the resource index
			// @ATG_CHANGE : BEGIN UWP Packaging support
			string ResourceLogFile = Path.Combine(IntermediatePath, "ResIndexLog.xml");
			string ResourceIndexStagingFile = Path.Combine(IntermediatePath, "resources.pri");
			string ResourceIndexFile = Path.Combine(OutputPath, "resources.pri");
			if (File.Exists(ResourceIndexFile))
			{
				File.Delete(ResourceIndexFile);
			}
			StartInfo = new ProcessStartInfo();
			StartInfo.FileName = PriExecutable;
			// @ATG_CHANGE : Win10 version puts output file in the wrong place without /of; no harm in specifying it on Xbox also.
			StartInfo.Arguments = "new /pr \"" + Path.GetDirectoryName(ResourceIndexFile) + "\" /cf \"" + ResourceConfigFile + "\" /mn \"" + AppxManifestPath + "\" /il \"" + ResourceLogFile + "\" /o /of \"" + ResourceIndexStagingFile + "\"";
			// @ATG_CHANGE : END
			StartInfo.CreateNoWindow = true;
			StartInfo.StandardErrorEncoding = System.Text.Encoding.Unicode;
			StartInfo.StandardOutputEncoding = System.Text.Encoding.Unicode;
			Utils.RunLocalProcessAndLogOutput(StartInfo);
			File.Copy(ResourceIndexStagingFile, ResourceIndexFile);
			// @ATG_CHANGE : END
			UpdatedFilePaths.Add(ResourceIndexFile);

			return UpdatedFilePaths;
		}

		// @ATG_CHANGE : BEGIN UWP Packaging support
		private string GetPathToMakePriExe()
		{
			if (Platform == UnrealTargetPlatform.XboxOne)
			{
				// @ATG_CHANGE : BEGIN Future XDK support
				string XDKDirectory = Environment.ExpandEnvironmentVariables("%DurangoXDK%");
				return Path.Combine(XDKDirectory, "bin", "makepri.exe");
				// @ATG_CHANGE : END
			}

			string SDKFolder = VCEnvironment.FindWindowsSDKInstallationFolder("v10.0");
			return Path.Combine(SDKFolder, "bin", Environment.Is64BitProcess ? "x64" : "x86", "makepri.exe");
		}
		// @ATG_CHANGE : END		

		// @ATG_CHANGE : BEGIN Helper for target resource layout
		bool CopySingleResourceFile(string InSource, string InDest)
		{
			if (File.Exists(InDest))
			{
				if (File.GetLastWriteTimeUtc(InSource) > File.GetLastWriteTimeUtc(InDest))
				{
					try
					{
						// Remove read-only flag
						File.SetAttributes(InDest, File.GetAttributes(InDest) & ~FileAttributes.ReadOnly);
						File.Delete(InDest);
					}
					catch (Exception)
					{
						Log.TraceError("Could not replace {0}.", InDest);
						return false;
					}
				}
			}
			if (!File.Exists(InDest))
			{
				if (!Directory.Exists(Path.GetDirectoryName(InDest)))
				{
					try
					{
						Directory.CreateDirectory(Path.GetDirectoryName(InDest));
					}
					catch (Exception)
					{
						Log.TraceError("Could not create output directory {0}.", InDest);
						return false;
					}
				}
				try
				{
					File.Copy(InSource, InDest);
					// Remove read-only flag
					File.SetAttributes(InDest, File.GetAttributes(InDest) & ~FileAttributes.ReadOnly);
				}
				catch (Exception)
				{
					Log.TraceError("Could not replace {0}.", InDest);
					return false;
				}
				return true;
			}

			return false;
		}
		// @ATG_CHANGE : END

	}
}
