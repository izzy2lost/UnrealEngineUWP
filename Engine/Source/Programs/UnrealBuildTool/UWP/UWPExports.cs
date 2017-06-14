using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// Public UWPDeploy wrapper exposed to UAT
	/// </summary>
	public class UWPExports
	{
		private UWPDeploy InnerDeploy;

		/// <summary>
		/// 
		/// </summary>
		public UWPExports()
		{
			InnerDeploy = new UWPDeploy();
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
			return InnerDeploy.PrepForUATPackageOrDeploy(ProjectFile, InProjectName, InProjectDirectory, InTargetConfigurations, InExecutablePaths, InEngineDir, bForDistribution, CookFlavor, bIsDataDeploy);
		}

		/// <summary>
		/// Collect all the WinMD references
		/// </summary>
		/// <param name="Receipt"></param>
		/// <param name="SourceProjectDir"></param>
		/// <param name="DestPackageRoot"></param>
		public void AddWinMDReferencesFromReceipt(TargetReceipt Receipt, DirectoryReference SourceProjectDir, string DestPackageRoot)
		{
			InnerDeploy.AddWinMDReferencesFromReceipt(Receipt, SourceProjectDir, DestPackageRoot);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="specificVersion"></param>
		/// <returns></returns>
		public static string FindWindowsSDKInstallationFolder(string specificVersion)
		{
			return string.Empty; //VCEnvironment.FindWindowsSDKInstallationFolder(specificVersion);
		}
	}
}
