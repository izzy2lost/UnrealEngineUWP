using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Public UWPDeploy wrapper exposed to UAT
	/// </summary>
	public class UWPExports
	{
		/// <summary>
		/// 
		/// </summary>
		public UWPExports()
		{

		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <param name="InProjectName"></param>
		/// <param name="InProjectDirectory"></param>
		/// <param name="InTargetConfigurations"></param>
		/// <param name="InExecutablePaths"></param>
		/// <param name="InEngineDir"></param>
		/// <param name="bForDistribution"></param>
		/// <param name="CookFlavor"></param>
		/// <param name="bIsDataDeploy"></param>
		/// <returns></returns>
		public bool PrepForUATPackageOrDeploy(FileReference ProjectFile, string InProjectName, string InProjectDirectory, List<UnrealTargetConfiguration> InTargetConfigurations, List<string> InExecutablePaths, string InEngineDir, bool bForDistribution, string CookFlavor, bool bIsDataDeploy)
		{
			return new UWPDeploy(ProjectFile).PrepForUATPackageOrDeploy(InProjectName, InProjectDirectory, InTargetConfigurations, InExecutablePaths, InEngineDir, bForDistribution, CookFlavor, bIsDataDeploy);
		}

		/// <summary>
		/// Collect all the WinMD references
		/// </summary>
		/// <param name="Receipt"></param>
		/// <param name="SourceProjectDir"></param>
		/// <param name="DestPackageRoot"></param>
		public void AddWinMDReferencesFromReceipt(TargetReceipt Receipt, DirectoryReference SourceProjectDir, string DestPackageRoot)
		{
			new UWPDeploy(Receipt.ProjectFile).AddWinMDReferencesFromReceipt(Receipt, SourceProjectDir, DestPackageRoot);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <returns></returns>
		public static void GetWindowsSDKInstallationFolder(out DirectoryReference SDKFolder, out Version SDKVersion)
		{
			// TODO: Use configured UWP SDK?
			if (!WindowsPlatform.TryGetWindowsSdkDir("Latest", out VersionNumber SelectedWindowsSdkVersion, out DirectoryReference SelectedWindowsSdkDir))
				throw new Exception("Could not find latest windows sdk dir");

			SDKFolder = SelectedWindowsSdkDir;
			SDKVersion = new Version(SelectedWindowsSdkVersion.ToString());
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="ToolName"></param>
		/// <returns></returns>
		public static FileReference GetWindowsSdkToolPath(string ToolName)
		{
			return UniversalWindowsPlatformToolChain.GetWindowsSdkToolPath(ToolName);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="SdkVersion"></param>
		/// <returns></returns>
		public static bool InitWindowsSdkToolPath(string SdkVersion)
		{
			return UniversalWindowsPlatformToolChain.InitWindowsSdkToolPath(SdkVersion);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="DLCFile"></param>
		/// <param name="OutputDirectory"></param>
		public static void CreateManifestForDLC(FileReference DLCFile, DirectoryReference OutputDirectory)
		{
			string IntermediateDirectory = DirectoryReference.Combine(DLCFile.Directory, "Intermediate", "Deploy").FullName;
			new UWPManifestGenerator().CreateManifest(UnrealTargetPlatform.UWP64, OutputDirectory.FullName, IntermediateDirectory, DLCFile, DLCFile.Directory.FullName, new List<UnrealTargetConfiguration>(), new List<string>(), null);
		}
	}
}
