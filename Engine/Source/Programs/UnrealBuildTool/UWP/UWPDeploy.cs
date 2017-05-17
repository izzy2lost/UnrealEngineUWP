// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;

// @todo UWP: this file is a work in progress and is not yet complete for the F5 scenario for UWP

namespace UnrealBuildTool
{
	/// <summary>
	///  Base class to handle deploy of a target for a given platform
	/// </summary>
// @ATG_CHANGE : BEGIN UWP packaging & F5 support
	public class UWPDeploy : UEBuildDeploy
// @ATG_CHANGE : END
	{
		/// <summary>
		/// Utility function to delete a file
		/// </summary>
		void DeployHelper_DeleteFile(string InFileToDelete)
		{
			Log.TraceInformation("UWPDeploy.DeployHelper_DeleteFile({0})", InFileToDelete);
			if (File.Exists(InFileToDelete) == true)
			{
				FileAttributes attributes = File.GetAttributes(InFileToDelete);
				if ((attributes & FileAttributes.ReadOnly) == FileAttributes.ReadOnly)
				{
					attributes &= ~FileAttributes.ReadOnly;
					File.SetAttributes(InFileToDelete, attributes);
				}
				File.Delete(InFileToDelete);
			}
		}

		/// <summary>
		/// Copy the contents of the given source directory to the given destination directory
		/// </summary>
		bool CopySourceToDestDir(string InSourceDirectory, string InDestinationDirectory, string InWildCard,
			bool bInIncludeSubDirectories, bool bInRemoveDestinationOrphans)
		{
			Log.TraceInformation("UWPDeploy.CopySourceToDestDir({0}, {1}, {2},...)", InSourceDirectory, InDestinationDirectory, InWildCard);
			if (Directory.Exists(InSourceDirectory) == false)
			{
				Log.TraceInformation("Warning: CopySourceToDestDir - SourceDirectory does not exist: {0}", InSourceDirectory);
				return false;
			}

			// Make sure the destination directory exists!
			Directory.CreateDirectory(InDestinationDirectory);

			SearchOption OptionToSearch = bInIncludeSubDirectories ? SearchOption.AllDirectories : SearchOption.TopDirectoryOnly;

			var SourceDirs = new List<string>(Directory.GetDirectories(InSourceDirectory, "*.*", OptionToSearch));
			foreach (string SourceDir in SourceDirs)
			{
				string SubDir = SourceDir.Replace(InSourceDirectory, "");
				string DestDir = InDestinationDirectory + SubDir;
				Directory.CreateDirectory(DestDir);
			}

			var SourceFiles = new List<string>(Directory.GetFiles(InSourceDirectory, InWildCard, OptionToSearch));
			var DestFiles = new List<string>(Directory.GetFiles(InDestinationDirectory, InWildCard, OptionToSearch));

			// Keep a list of the files in the source directory... without the source path
			List<string> FilesInSource = new List<string>();

			// Copy all the source files that are newer...
			foreach (string SourceFile in SourceFiles)
			{
				string Filename = SourceFile.Replace(InSourceDirectory, "");
				FilesInSource.Add(Filename.ToUpperInvariant());
				string DestFile = InDestinationDirectory + Filename;

				System.DateTime SourceTime = File.GetLastWriteTime(SourceFile);
				System.DateTime DestTime = File.GetLastWriteTime(DestFile);

				if (SourceTime > DestTime)
				{
					try
					{
						DeployHelper_DeleteFile(DestFile);
						File.Copy(SourceFile, DestFile, true);
					}
					catch (Exception exceptionMessage)
					{
						Log.TraceInformation("Failed to copy {0} to deployment: {1}", SourceFile, exceptionMessage);
					}
				}
			}

			if (bInRemoveDestinationOrphans == true)
			{
				// If requested, delete any destination files that do not have a corresponding
				// file in the source directory
				foreach (string DestFile in DestFiles)
				{
					string DestFilename = DestFile.Replace(InDestinationDirectory, "");
					if (FilesInSource.Contains(DestFilename.ToUpperInvariant()) == false)
					{
						Log.TraceInformation("Destination file does not exist in Source - DELETING: {0}", DestFile);
						FileAttributes attributes = File.GetAttributes(DestFile);
						try
						{
							DeployHelper_DeleteFile(DestFile);
						}
						catch (Exception exceptionMessage)
						{
							Log.TraceInformation("Failed to delete {0} from deployment: {1}", DestFile, exceptionMessage);
						}
					}
				}
			}

			return true;
		}


		/// <summary>
		/// Helper function for copying files
		/// </summary>
// @ATG_CHANGE : BEGIN UWP support
		void CopyFile(string InSource, string InDest, bool bForce)
		{
			if (File.Exists(InSource) == true)
			{
				if (File.Exists(InDest) == true)
				{
					if (File.GetLastWriteTime(InSource).CompareTo(File.GetLastWriteTime(InDest)) == 0)
					{
						//If the source and dest have the file and they have the same write times they are assumed to be equal and we don't need to copy.
						return;
					}
					if (bForce == true)
					{
						DeployHelper_DeleteFile(InDest);
					}
				}
				Log.TraceInformation("UWPDeploy.CopyFile({0}, {1}, {2})", InSource, InDest, bForce);
				File.Copy(InSource, InDest, true);
				File.SetAttributes(InDest, File.GetAttributes(InDest) & ~FileAttributes.ReadOnly);
			}
			else
			{
				Log.TraceInformation("UWPDeploy: File didn't exist - {0}", InSource);
			}
		}

		/// <summary>
		/// Helper function for copying a tree files
		/// </summary>
		void CopyDirectory(string InSource, string InDest, bool bForce, bool bRecurse)
		{
			if (Directory.Exists(InSource))
			{
				if (!Directory.Exists(InDest))
				{
					Directory.CreateDirectory(InDest);
				}

				// Copy all files
				string[] FilesInDir = Directory.GetFiles(InSource);
				foreach (string FileSourcePath in FilesInDir)
				{
					string FileDestPath = Path.Combine(InDest, Path.GetFileName(FileSourcePath));
					CopyFile(FileSourcePath, FileDestPath, true);
				}

				// Recurse sub directories
				string[] DirsInDir = Directory.GetDirectories(InSource);
				foreach (string DirSourcePath in DirsInDir)
				{
					string DirName = Path.GetFileName(DirSourcePath);
					string DirDestPath = Path.Combine(InDest, DirName);
					CopyDirectory(DirSourcePath, DirDestPath, bForce, bRecurse);
				}
			}
		}

		public bool PrepForUATPackageOrDeploy(FileReference ProjectFile, string InProjectName, string InProjectDirectory, string InExecutablePath, string InEngineDir, bool bForDistribution, string CookFlavor, bool bIsDataDeploy)
		{
			string IntermediateDirectory = Path.Combine(InProjectDirectory, "Intermediate", "Deploy");
			//@todo need to support dlc and other targets
			string LocalizedContentDirectory = Path.Combine(InProjectDirectory, "Content", "Localization", "Game");
			string AbsoluteExeDirectory = Path.GetDirectoryName(InExecutablePath);
			bool Is32bit = Path.GetFileName(AbsoluteExeDirectory).Equals("UWP32", StringComparison.OrdinalIgnoreCase);
            UnrealTargetPlatform Platform = Is32bit ? UnrealTargetPlatform.UWP32 : UnrealTargetPlatform.UWP64;
			bool IsGameSpecificExe = ProjectFile != null && AbsoluteExeDirectory.StartsWith(InProjectDirectory);
			string RelativeExeFilePath = Path.Combine(IsGameSpecificExe ? InProjectName : "Engine", "Binaries", Is32bit ? "UWP32" : "UWP64", Path.GetFileName(InExecutablePath));
			string AppxManifestTargetPath = Path.Combine(AbsoluteExeDirectory, "AppxManifest.xml");

			// Generate AppX manifest based on ini files and referenced winmd files.
			PackageManifestGenerator ManifestGenerator = new PackageManifestGenerator(RelativeExeFilePath, InProjectDirectory, ProjectFile, Platform, new string[] { "uap", "mp" }, WinMDReferences);
			ManifestGenerator.CreateManifest(AppxManifestTargetPath);

            // Generate resources based on ini files.
			PackageResourceGenerator ResourceGenerator = new PackageResourceGenerator(Platform, ProjectFile);
			ResourceGenerator.GenerateResources(AbsoluteExeDirectory, IntermediateDirectory, LocalizedContentDirectory, InProjectDirectory, AppxManifestTargetPath);

            // If using a secure networking manifest, copy it to the output directory.
            string NetworkManifest = Path.Combine(InProjectDirectory, "Config", "UWP", "NetworkManifest.xml");
            if (File.Exists(NetworkManifest))
            {
                CopyFile(NetworkManifest, Path.Combine(AbsoluteExeDirectory, "NetworkManifest.xml"), false);
            }

			// If using Xbox Live generate the json config file expected by the SDK
			DirectoryReference ConfigDirRef = DirectoryReference.FromFile(ProjectFile);
			if (ConfigDirRef == null && !string.IsNullOrEmpty(UnrealBuildTool.GetRemoteIniPath()))
			{
				ConfigDirRef = new DirectoryReference(UnrealBuildTool.GetRemoteIniPath());
			}

			ConfigHierarchy EngineIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFile), Platform);
			if (EngineIni != null)
            {
				string TitleId;
				string Scid;
				bool IsCreatorsProgram = false;
				EngineIni.GetString("/Script/UWPTargetPlatform.UWPTargetSettings", "TitleId", out TitleId);
				EngineIni.GetString("/Script/UWPTargetPlatform.UWPTargetSettings", "ServiceConfigId", out Scid);
				EngineIni.GetBool("/Script/UWPTargetPlatform.UWPTargetSettings", "bIsCreatorsProgramTitle", out IsCreatorsProgram);

				bool HasTitleId = !string.IsNullOrEmpty(TitleId);
				bool HasScid = !string.IsNullOrEmpty(Scid);
				if (HasTitleId && HasScid)
				{
					using (JsonWriter XboxServicesConfig = new JsonWriter(Path.Combine(AbsoluteExeDirectory, "xboxservices.config")))
					{
						int TitleIdAsInt;
						if (int.TryParse(TitleId, System.Globalization.NumberStyles.HexNumber, System.Globalization.CultureInfo.InvariantCulture, out TitleIdAsInt))
						{
							XboxServicesConfig.WriteObjectStart();
							XboxServicesConfig.WriteValue("TitleId", TitleIdAsInt);
							XboxServicesConfig.WriteValue("PrimaryServiceConfigId", Scid);
							if (IsCreatorsProgram)
							{
								XboxServicesConfig.WriteValue("XboxLiveCreatorsTitle", true);
							}

							XboxServicesConfig.WriteObjectEnd();
						}
						else
						{
							Log.TraceError("Xbox Live Title Id was not in a recognized format.  Specify a 32 bit hex number (without leading 0x)");
						}
					}
				}
				else if (HasTitleId != HasScid)
				{
					Log.TraceWarning("Only one of TitleId and Scid was provided.  This is probably a configuration error.  Either both should exist, or neither.");
				}
			}

            return true;
		}

		public override bool PrepTargetForDeployment(UEBuildDeployTarget InTarget)
		{
			string InAppName = InTarget.AppName;
			Log.TraceInformation("Prepping {0} for deployment to {1}", InAppName, InTarget.Platform.ToString());
			System.DateTime PrepDeployStartTime = DateTime.UtcNow;

            TargetReceipt Receipt = TargetReceipt.Read(InTarget.BuildReceiptFileName);
            AddWinMDReferencesFromReceipt(Receipt, InTarget.ProjectDirectory, string.Empty);

            PrepForUATPackageOrDeploy(InTarget.ProjectFile, InAppName, InTarget.ProjectDirectory.FullName, InTarget.OutputPath.FullName, BuildConfiguration.RelativeEnginePath, false, "", false);

            DirectoryReference ProjectBinaryFolder = InTarget.OutputPath.Directory;

            string[] AdditionalAppXFiles = new string[] { "NetworkManifest.xml", "xboxservices.config", "UE4Commandline.txt" };
			bool IsGameSpecificExe = InTarget.ProjectFile != null && ProjectBinaryFolder.IsUnderDirectory(InTarget.ProjectDirectory);
			
			string RecipeFileName = (IsGameSpecificExe ? InAppName : "UE4") + ".build.appxrecipe";

			FileReference AppxRecipeDest = FileReference.Combine(ProjectBinaryFolder, RecipeFileName);
            GeneratePackageAppXRecipe(AppxRecipeDest.FullName, InTarget, Receipt.RuntimeDependencies, AdditionalAppXFiles);

            // Log out the time taken to deploy...
            double PrepDeployDuration = (DateTime.UtcNow - PrepDeployStartTime).TotalSeconds;
            Log.TraceInformation("UWP deployment preparation took {0:0.00} seconds", PrepDeployDuration);

            return true;
		}
		// @ATG_CHANGE : END

		// @ATG_CHANGE : BEGIN winmd type registration support
		public void AddWinMDReferencesFromReceipt(TargetReceipt Receipt, DirectoryReference SourceProjectDir, string DestRelativeTo)
		{
			// Don't use Receipt.ExpandPathVariables because the variables are useful for both source and dest.

			Dictionary<string, string> SourceVariables = new Dictionary<string, string>();
			SourceVariables["EngineDir"] = UnrealBuildTool.EngineDirectory.FullName;
			SourceVariables["ProjectDir"] = SourceProjectDir.FullName;

			Dictionary<string, string> DestVariables = new Dictionary<string, string>();
			DestVariables["EngineDir"] = Path.Combine(DestRelativeTo, "Engine");
			DestVariables["ProjectDir"] = Path.Combine(DestRelativeTo, SourceProjectDir.GetDirectoryName());

			foreach (var Dep in Receipt.RuntimeDependencies)
			{
				if (Path.GetExtension(Dep.Path) == ".dll")
				{
					string SourcePath = Utils.ExpandVariables(Dep.Path, SourceVariables);
					string WinMDFile = Path.ChangeExtension(SourcePath, "winmd");
					if (File.Exists(WinMDFile))
					{
						string DestPath = Dep.Path;
						DestPath = Utils.ExpandVariables(DestPath, DestVariables);
						DestPath = Utils.MakePathRelativeTo(DestPath, DestRelativeTo);
						WinMDReferences.Add(new WinMDRegistrationInfo(new FileReference(WinMDFile), DestPath));
					}
				}
			}
		}

		private void GeneratePackageAppXRecipe(string InOutputFile, UEBuildDeployTarget InTarget, List<RuntimeDependency> Dependencies, IEnumerable<string> AdditionalFiles)
		{
			var AppXRecipeProjectFileContent = new StringBuilder();
            string VcProjectToolVersion;
            switch (UniversalWindowsPlatform.Compiler)
            {
                case WindowsCompiler.VisualStudio2017:
                    VcProjectToolVersion = VCProjectFileGenerator.GetProjectFileToolVersionString(VCProjectFileFormat.VisualStudio2017);
                    break;
                default:
                    VcProjectToolVersion = VCProjectFileGenerator.GetProjectFileToolVersionString(VCProjectFileFormat.VisualStudio2015);
                    break;
            }

            AppXRecipeProjectFileContent.Append(
				"<?xml version=\"1.0\" encoding=\"utf-8\"?>" + ProjectFileGenerator.NewLine +
				ProjectFileGenerator.NewLine +
				"<Project DefaultTargets=\"Build\" ToolsVersion=\"" + VcProjectToolVersion + "\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">" + ProjectFileGenerator.NewLine);

            DirectoryReference ProjectBinariesDirectory = new FileReference(InTarget.BuildReceiptFileName).Directory;

            // This is not the full set of properties that a VS build would add, but it's enough that VS deployment will work
            // both locally and on a remote machine.
            AppXRecipeProjectFileContent.Append(@"   <PropertyGroup>" + ProjectFileGenerator.NewLine);
            AppXRecipeProjectFileContent.Append(@"       <TargetOSVersion>10.0</TargetOSVersion>" + ProjectFileGenerator.NewLine);
            AppXRecipeProjectFileContent.Append(@"       <WindowsUser>" + Environment.UserName + "</WindowsUser>" + ProjectFileGenerator.NewLine);
            AppXRecipeProjectFileContent.Append(@"       <SolutionConfiguration>" + InTarget.Configuration + "|" + InTarget.Platform + "</SolutionConfiguration>" + ProjectFileGenerator.NewLine);
            AppXRecipeProjectFileContent.Append(@"   </PropertyGroup>" + ProjectFileGenerator.NewLine);

            // Add the manifest
            AppXRecipeProjectFileContent.Append(@"   <ItemGroup>" + ProjectFileGenerator.NewLine);
            AppXRecipeProjectFileContent.Append(@"       <AppxManifest Include=""" + FileReference.Combine(ProjectBinariesDirectory, "AppxManifest.xml") + @""">" + ProjectFileGenerator.NewLine);
            AppXRecipeProjectFileContent.Append(@"          <PackagePath>AppxManifest.xml</PackagePath>" + ProjectFileGenerator.NewLine);
            AppXRecipeProjectFileContent.Append(@"          <ReRegisterAppIfChanged>true</ReRegisterAppIfChanged>" + ProjectFileGenerator.NewLine);
            AppXRecipeProjectFileContent.Append(@"       </AppxManifest>" + ProjectFileGenerator.NewLine);
            AppXRecipeProjectFileContent.Append(@"   </ItemGroup>" + ProjectFileGenerator.NewLine);

            // Add the actual package content
            AppXRecipeProjectFileContent.Append(@"   <ItemGroup>" + ProjectFileGenerator.NewLine);

			// Game exe
			foreach (var BinaryOutput in InTarget.OutputPaths)
            {
				AppXRecipeProjectFileContent.Append(@"      <AppxPackagedFile Include=""" + BinaryOutput + @""">" + ProjectFileGenerator.NewLine);
				bool IsGameSpecificExe = InTarget.ProjectFile != null && BinaryOutput.IsUnderDirectory(InTarget.ProjectDirectory);
				if (IsGameSpecificExe)
				{
					AppXRecipeProjectFileContent.Append(@"          <PackagePath>" + Path.Combine(InTarget.AppName, BinaryOutput.MakeRelativeTo(InTarget.ProjectDirectory)) + @"</PackagePath>" + ProjectFileGenerator.NewLine);
				}
				else
				{
					AppXRecipeProjectFileContent.Append(@"          <PackagePath>" + Path.Combine("Engine", BinaryOutput.MakeRelativeTo(UnrealBuildTool.EngineDirectory)) + @"</PackagePath>" + ProjectFileGenerator.NewLine);
				}
				AppXRecipeProjectFileContent.Append(@"      </AppxPackagedFile>" + ProjectFileGenerator.NewLine);
            }

			if (InTarget.ProjectFile != null)
			{
				// Game project file
				AppXRecipeProjectFileContent.Append(@"      <AppxPackagedFile Include=""" + InTarget.ProjectFile + @""">" + ProjectFileGenerator.NewLine);
				AppXRecipeProjectFileContent.Append(@"          <PackagePath>" + Path.Combine(InTarget.AppName, InTarget.ProjectFile.MakeRelativeTo(InTarget.ProjectDirectory)) + @"</PackagePath>" + ProjectFileGenerator.NewLine);
				AppXRecipeProjectFileContent.Append(@"      </AppxPackagedFile>" + ProjectFileGenerator.NewLine);
			}

			// Runtime dependencies.
			Dictionary<string, string> SourceVariables = new Dictionary<string, string>();
            SourceVariables["EngineDir"] = UnrealBuildTool.EngineDirectory.FullName;
            SourceVariables["ProjectDir"] = InTarget.ProjectDirectory.FullName;

            Dictionary<string, string> DestVariables = new Dictionary<string, string>();
            DestVariables["EngineDir"] = "Engine";
            DestVariables["ProjectDir"] = InTarget.AppName;

            foreach (var RuntimeDep in Dependencies)
            {
                string SourcePath = Utils.ExpandVariables(RuntimeDep.Path, SourceVariables).Replace(@"/", @"\");
                string DeployPath = Utils.ExpandVariables(RuntimeDep.Path, DestVariables).Replace(@"/", @"\");

				// 4.12: Dependencies now support ... syntax for recursive directory traversal.
				// Translate this to MSBuild syntax. 
				bool IncludeDependencyInRecipe = true;
				if (SourcePath.Contains(@"..."))
				{
					SourcePath = SourcePath.Replace(@"...", @"**\*.*");
					DeployPath = DeployPath.Replace(@"...", @"%(RecursiveDir)%(Filename)%(Extension)");
				}
				else if (!File.Exists(SourcePath))
				{
					LogEventType TraceVerbosity;
					switch (Path.GetExtension(SourcePath))
					{
						case ".pdb":
							TraceVerbosity = LogEventType.Verbose;
							break;

						case ".dll":
							TraceVerbosity = LogEventType.Error;
							break;

						default:
							TraceVerbosity = LogEventType.Warning;
							break;
					}
					IncludeDependencyInRecipe = false;
					Log.WriteLine(TraceVerbosity, "Could not find source file for runtime dependency {0}.  Excluding from appxrecipe.", RuntimeDep.Path);
				}

				if (IncludeDependencyInRecipe)
				{
					AppXRecipeProjectFileContent.Append(@"      <AppxPackagedFile Include=""" + SourcePath + @""">" + ProjectFileGenerator.NewLine);
					AppXRecipeProjectFileContent.Append(@"          <PackagePath>" + DeployPath + "</PackagePath>" + ProjectFileGenerator.NewLine);
					AppXRecipeProjectFileContent.Append(@"      </AppxPackagedFile>" + ProjectFileGenerator.NewLine);
				}
			}

			//UWP resources
			AppXRecipeProjectFileContent.Append(@"      <AppxPackagedFile Include=""" + FileReference.Combine(ProjectBinariesDirectory, "resources.pri") + @""">" + ProjectFileGenerator.NewLine);
			AppXRecipeProjectFileContent.Append(@"          <PackagePath>resources.pri</PackagePath>" + ProjectFileGenerator.NewLine);
			AppXRecipeProjectFileContent.Append(@"      </AppxPackagedFile>" + ProjectFileGenerator.NewLine);
			AppXRecipeProjectFileContent.Append(@"      <AppxPackagedFile Include=""" + FileReference.Combine(ProjectBinariesDirectory, @"Resources\*.*") + @""">" + ProjectFileGenerator.NewLine);
			AppXRecipeProjectFileContent.Append(@"          <PackagePath>Resources\%(Filename)%(Extension)</PackagePath>" + ProjectFileGenerator.NewLine);
			AppXRecipeProjectFileContent.Append(@"          <ReRegisterAppIfChanged>true</ReRegisterAppIfChanged>" + ProjectFileGenerator.NewLine);
			AppXRecipeProjectFileContent.Append(@"      </AppxPackagedFile>" + ProjectFileGenerator.NewLine);
			AppXRecipeProjectFileContent.Append(@"      <AppxPackagedFile Include=""" + FileReference.Combine(ProjectBinariesDirectory, @"Resources\**\*.*") + @""">" + ProjectFileGenerator.NewLine);
			AppXRecipeProjectFileContent.Append(@"          <PackagePath>Resources\%(RecursiveDir)%(Filename)%(Extension)</PackagePath>" + ProjectFileGenerator.NewLine);
			AppXRecipeProjectFileContent.Append(@"          <ReRegisterAppIfChanged>true</ReRegisterAppIfChanged>" + ProjectFileGenerator.NewLine);
			AppXRecipeProjectFileContent.Append(@"      </AppxPackagedFile>" + ProjectFileGenerator.NewLine);

			// Anything else added by the build system
			foreach (var FileToPackage in AdditionalFiles)
            {
                FileReference FileRef = FileReference.Combine(ProjectBinariesDirectory, FileToPackage);
                if (FileRef.Exists())
                {
                    AppXRecipeProjectFileContent.Append(@"      <AppxPackagedFile Include=""" + FileRef + @""">" + ProjectFileGenerator.NewLine);
                    AppXRecipeProjectFileContent.Append(@"          <PackagePath>" + FileToPackage + @"</PackagePath>" + ProjectFileGenerator.NewLine);
                    AppXRecipeProjectFileContent.Append(@"      </AppxPackagedFile>" + ProjectFileGenerator.NewLine);
                }
            }

			DirectoryReference ConfigDirRef = DirectoryReference.FromFile(InTarget.ProjectFile);
			if (ConfigDirRef == null && !string.IsNullOrEmpty(UnrealBuildTool.GetRemoteIniPath()))
			{
				ConfigDirRef = new DirectoryReference(UnrealBuildTool.GetRemoteIniPath());
			}

			// Copy pre-cooked content into the package.  This is optional since it could be enormous.
			ConfigHierarchy EngineIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, ConfigDirRef, InTarget.Platform);
			if (EngineIni != null)
			{
				bool bCopyCookedContentForF5Deployment = false;
				EngineIni.GetBool("/Script/UWPTargetPlatform.UWPTargetSettings", "bCopyCookedContentForF5Deployment", out bCopyCookedContentForF5Deployment);
				if (bCopyCookedContentForF5Deployment)
				{
					DirectoryReference BaseCookedDir = DirectoryReference.Combine(InTarget.ProjectDirectory, "Saved", "Cooked", "UWP");

					AppXRecipeProjectFileContent.Append(@"      <AppxPackagedFile Include=""" + DirectoryReference.Combine(BaseCookedDir, "Engine", "**", "*.*").FullName + @""">" + ProjectFileGenerator.NewLine);
					AppXRecipeProjectFileContent.Append(@"          <PackagePath>Engine\%(RecursiveDir)%(Filename)%(Extension)</PackagePath>" + ProjectFileGenerator.NewLine);
					AppXRecipeProjectFileContent.Append(@"      </AppxPackagedFile>" + ProjectFileGenerator.NewLine);

					AppXRecipeProjectFileContent.Append(@"      <AppxPackagedFile Include=""" + DirectoryReference.Combine(BaseCookedDir, InTarget.AppName, "**", "*.*").FullName + @""">" + ProjectFileGenerator.NewLine);
					AppXRecipeProjectFileContent.Append(@"          <PackagePath>" + InTarget.AppName + @"\%(RecursiveDir)%(Filename)%(Extension)</PackagePath>" + ProjectFileGenerator.NewLine);
					AppXRecipeProjectFileContent.Append(@"      </AppxPackagedFile>" + ProjectFileGenerator.NewLine);
				}
			}

			// Copy internationalization files that are required to init the localization system and are consumed in source format
			AppXRecipeProjectFileContent.Append(@"      <AppxPackagedFile Include=""" + DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Content", "Internationalization", "**", "*.*") + @""" >" + ProjectFileGenerator.NewLine);
			AppXRecipeProjectFileContent.Append(@"          <PackagePath>Engine\Content\Internationalization\%(RecursiveDir)%(Filename)%(Extension)</PackagePath>" + ProjectFileGenerator.NewLine);
			AppXRecipeProjectFileContent.Append(@"      </AppxPackagedFile>" + ProjectFileGenerator.NewLine);

			// Copy config files
			AppXRecipeProjectFileContent.Append(@"      <AppxPackagedFile Include=""" + DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Config", "**", "*.*") + @""" >" + ProjectFileGenerator.NewLine);
			AppXRecipeProjectFileContent.Append(@"          <PackagePath>Engine\Config\%(RecursiveDir)%(Filename)%(Extension)</PackagePath>" + ProjectFileGenerator.NewLine);
			AppXRecipeProjectFileContent.Append(@"      </AppxPackagedFile>" + ProjectFileGenerator.NewLine);

			if (ConfigDirRef != null)
			{
				AppXRecipeProjectFileContent.Append(@"      <AppxPackagedFile Include=""" + DirectoryReference.Combine(ConfigDirRef, "Config", "**", "*.*") + @""" >" + ProjectFileGenerator.NewLine);
				AppXRecipeProjectFileContent.Append(@"          <PackagePath>" + InTarget.AppName + @"\Config\%(RecursiveDir)%(Filename)%(Extension)</PackagePath>" + ProjectFileGenerator.NewLine);
				AppXRecipeProjectFileContent.Append(@"      </AppxPackagedFile>" + ProjectFileGenerator.NewLine);
			}

			AppXRecipeProjectFileContent.Append(@"   </ItemGroup>" + ProjectFileGenerator.NewLine);
            AppXRecipeProjectFileContent.Append(@"</Project>" + ProjectFileGenerator.NewLine);
            File.WriteAllText(InOutputFile, AppXRecipeProjectFileContent.ToString(), Encoding.UTF8);
        }

        private List<WinMDRegistrationInfo> WinMDReferences = new List<WinMDRegistrationInfo>();
        // @ATG_CHANGE : END
    }
}
