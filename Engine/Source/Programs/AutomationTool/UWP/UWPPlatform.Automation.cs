using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Threading.Tasks;
using AutomationTool;
using UnrealBuildTool;

namespace UWP.Automation
{
    public abstract class UWPPlatform : Platform
    {
        public UWPPlatform(UnrealTargetPlatform P)
            : base(P)
        {
        }

		public override void Deploy(ProjectParams Params, DeploymentContext SC)
		{
			string AppxManifestPath = Path.Combine(SC.StageDirectory, "AppxManifest.xml");
			System.Xml.Linq.XDocument doc = System.Xml.Linq.XDocument.Load(AppxManifestPath);
			System.Xml.Linq.XElement package = doc.Root;
			System.Xml.Linq.XElement identity = package.Element(System.Xml.Linq.XName.Get("Identity", package.Name.NamespaceName));

			Windows.Management.Deployment.PackageManager PackMgr = new Windows.Management.Deployment.PackageManager();
			try
			{
				var ExistingPackage = PackMgr.FindPackagesForUser("", identity.Attribute("Name").Value, identity.Attribute("Publisher").Value).FirstOrDefault();

				// Only remove an existing package if it's in development mode; otherwise the removal might silently delete stuff
				// that the user wanted.
				if (ExistingPackage != null)
				{
					if (ExistingPackage.IsDevelopmentMode)
					{
						PackMgr.RemovePackageAsync(ExistingPackage.Id.FullName, Windows.Management.Deployment.RemovalOptions.PreserveApplicationData).AsTask().Wait();
					}
					else if (!Params.Package)
					{
						throw new AutomationException(ExitCode.Error_AppInstallFailed, "A packaged version of the application already exists.  It must be uninstalled manually - note this will remove user data.");
					}
				}

				if (!Params.Package)
				{
					PackMgr.RegisterPackageAsync(new Uri(AppxManifestPath), null, Windows.Management.Deployment.DeploymentOptions.DevelopmentMode).AsTask().Wait();
				}
				else
				{
					string PackagePath = Path.Combine(SC.StageDirectory, Params.ShortProjectName + ".appx");
					PackMgr.AddPackageAsync(new Uri(PackagePath), null, Windows.Management.Deployment.DeploymentOptions.None).AsTask().Wait();
				}
			}
			catch (AggregateException agg)
			{
				throw new AutomationException(ExitCode.Error_AppInstallFailed, agg.InnerException, "");
			}

			// Package should now be installed.  Locate it and make sure it's permitted to connect over loopback.
			try
			{
				var InstalledPackage = PackMgr.FindPackagesForUser("", identity.Attribute("Name").Value, identity.Attribute("Publisher").Value).FirstOrDefault();
				string LoopbackExemptCmdLine = string.Format("loopbackexempt -a -n={0}", InstalledPackage.Id.FamilyName);
				RunAndLog(CmdEnv, "checknetisolation.exe", LoopbackExemptCmdLine, null, 0, null, ERunOptions.None);
			}
			catch
			{
				LogWarning("Failed to apply a loopback exemption to the deployed app.  Connection to a local cook server will fail.");
			}
		}

		public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
		{
			UWPDeploy DeployHandler = new UWPDeploy();
			// Stage all the build products
			foreach (StageTarget Target in SC.StageTargets)
			{
				SC.StageBuildProductsFromReceipt(Target.Receipt, Target.RequireFilesExist);
				DeployHandler.AddWinMDReferencesFromReceipt(Target.Receipt, Params.RawProjectPath.Directory, SC.LocalRoot);
			}

			string ExePath = Path.Combine(Params.ProjectBinariesFolder, SC.StageExecutables[0] + Platform.GetExeExtension(SC.StageTargetPlatform.PlatformType));
			DeployHandler.PrepForUATPackageOrDeploy(Params.RawProjectPath, Params.ShortProjectName, SC.ProjectRoot, ExePath, SC.LocalRoot + "/Engine", Params.Distribution, "", Params.Deploy);

			// Stage UWP-specific assets (tile, splash, etc.)
			string assetsPath = Path.Combine(Params.ProjectBinariesFolder, "Resources");
			SC.StageFiles(StagedFileType.NonUFS, assetsPath, "*.png", true, null, "Resources");

			SC.StageFile(StagedFileType.NonUFS, Path.Combine(Params.ProjectBinariesFolder, "AppxManifest.xml"), "AppxManifest.xml");
			SC.StageFile(StagedFileType.NonUFS, Path.Combine(Params.ProjectBinariesFolder, "resources.pri"), "resources.pri");

            SC.StageFile(StagedFileType.NonUFS, Path.Combine(Params.ProjectBinariesFolder, "NetworkManifest.xml"), "NetworkManifest.xml");
            SC.StageFile(StagedFileType.NonUFS, Path.Combine(Params.ProjectBinariesFolder, "xboxservices.config"), "xboxservices.config");
        }

        public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly, string CookFlavor)
		{
			return "UWP";
		}

		public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
		{
			string SDKFolder = VCEnvironment.FindWindowsSDKInstallationFolder("v10.0", false);
			string MakeAppXPath = Path.Combine(SDKFolder, "bin", Environment.Is64BitProcess ? "x64" : "x86", "makeappx.exe");
			string OutputAppX = Path.Combine(SC.StageDirectory, Params.ShortProjectName + ".appx");
			string MakeAppXCommandLine = string.Format(@"pack /o /d ""{0}"" /p ""{1}""", SC.StageDirectory, OutputAppX);
			RunAndLog(CmdEnv, MakeAppXPath, MakeAppXCommandLine, null, 0, null, ERunOptions.None);

			string SigningCertificate = null;
			ConfigCacheIni PlatformEngineConfig = null;
			if (Params.EngineConfigs.TryGetValue(SC.StageTargetPlatform.PlatformType, out PlatformEngineConfig))
			{
				PlatformEngineConfig.GetString("/Script/UWPTargetPlatform.UWPTargetSettings", "SigningCertificate", out SigningCertificate);
			}

			if (!string.IsNullOrEmpty(SigningCertificate))
			{
				string SignToolPath = Path.Combine(SDKFolder, "bin", Environment.Is64BitProcess ? "x64" : "x86", "signtool.exe");
				string SignToolCommandLine = string.Format(@"sign /a /f ""{0}"" /fd SHA256 {1}", Path.Combine(SC.ProjectRoot, SigningCertificate), OutputAppX);
				RunAndLog(CmdEnv, SignToolPath, SignToolCommandLine, null, 0, null, ERunOptions.None);
			}
			else
			{
				LogWarning("No signing certificate provided.  App will not be deployable.  Specify a valid pfx in UWP platform settings.");
			}
		}

		//public override ProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
		//{
		//	string ActualCmdLine = "shell:AppsFolder\\ShooterGame_zjr0dfhgjwvde!ShooterGame " + ClientCmdLine;
		//	ClientRunFlags &= ~ERunOptions.AppMustExist;
		//	return Run("explorer.exe", ActualCmdLine, null, ClientRunFlags | ERunOptions.NoWaitForExit);
		//}

		public override bool IsSupported { get { return true; } }
		public override bool UseAbsLog { get { return false; } }
		public override bool LaunchViaUFE { get { return true; } }
		public override string UFEPlatformName
		{
			get { return "UWP"; }
		}

		public override List<string> GetDebugFileExtentions()
		{
			return new List<string> { ".pdb", ".map" };
		}
	}

    public class UWP64Platform : UWPPlatform
    {
        public UWP64Platform()
            : base(UnrealTargetPlatform.UWP64)
        {
        }
    }

    public class UWP32Platform : UWPPlatform
    {
        public UWP32Platform()
            : base(UnrealTargetPlatform.UWP32)
        {
        }
    }
}
