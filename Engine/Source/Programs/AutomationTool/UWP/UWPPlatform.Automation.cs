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
				SC.StageBuildProductsFromReceipt(Target.Receipt, Target.RequireFilesExist, Params.bTreatNonShippingBinariesAsDebugFiles);
				DeployHandler.AddWinMDReferencesFromReceipt(Target.Receipt, Params.RawProjectPath.Directory, SC.LocalRoot);
			}

			string ExePath = Path.Combine(Params.ProjectBinariesFolder, SC.StageExecutables[0] + Platform.GetExeExtension(SC.StageTargetPlatform.PlatformType));
			DeployHandler.PrepForUATPackageOrDeploy(Params.RawProjectPath, Params.ShortProjectName, SC.ProjectRoot, ExePath, SC.LocalRoot + "/Engine", Params.Distribution, "", Params.Deploy);

			// Stage UWP-specific assets (tile, splash, etc.)
			string assetsPath = Path.Combine(Params.ProjectBinariesFolder, "Resources");
			SC.StageFiles(StagedFileType.NonUFS, assetsPath, "*.png", true, null, "Resources");

			SC.StageFile(StagedFileType.NonUFS, Path.Combine(Params.ProjectBinariesFolder, "AppxManifest.xml"), "AppxManifest.xml");
			SC.StageFile(StagedFileType.NonUFS, Path.Combine(Params.ProjectBinariesFolder, "resources.pri"), "resources.pri");

			string SourceNetworkManifestPath = Path.Combine(Params.ProjectBinariesFolder, "NetworkManifest.xml");
			if (File.Exists(SourceNetworkManifestPath))
			{
				SC.StageFile(StagedFileType.NonUFS, SourceNetworkManifestPath, "NetworkManifest.xml");
			}
			string SourceXboxConfigPath = Path.Combine(Params.ProjectBinariesFolder, "xboxservices.config");
			if (File.Exists(SourceXboxConfigPath))
			{
				SC.StageFile(StagedFileType.NonUFS, SourceXboxConfigPath, "xboxservices.config");
			}
		}

		public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
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

			string SigningCertificate = @"Build\UWP\SigningCertificate.pfx";
			ConfigCacheIni PlatformEngineConfig = null;
			if (Params.EngineConfigs.TryGetValue(SC.StageTargetPlatform.PlatformType, out PlatformEngineConfig))
			{
				PlatformEngineConfig.GetString("/Script/UWPTargetPlatform.UWPTargetSettings", "SigningCertificate", out SigningCertificate);
			}

			if (!string.IsNullOrEmpty(SigningCertificate))
			{
				string SigningCertificatePath = Path.Combine(SC.ProjectRoot, SigningCertificate);
				if (!File.Exists(SigningCertificatePath))
				{
					if (!IsBuildMachine && !Params.Unattended)
					{
						// Extract the publisher name from the AppXManifest
						string AppxManifestPath = Path.Combine(SC.StageDirectory, "AppxManifest.xml");
						System.Xml.Linq.XDocument doc = System.Xml.Linq.XDocument.Load(AppxManifestPath);
						System.Xml.Linq.XElement package = doc.Root;
						System.Xml.Linq.XElement identity = package.Element(System.Xml.Linq.XName.Get("Identity", package.Name.NamespaceName));
						string Publisher = identity.Attribute("Publisher").Value;
						if (!string.IsNullOrEmpty(Publisher))
						{
							LogWarning("No certificate found at {0}.  Generating temporary self-signed certificate for {1}.", SigningCertificatePath, Publisher);
							GenerateSigningCertificate(SDKFolder, SigningCertificatePath, Publisher);
						}
						else
						{
							LogWarning("No certificate found at {0} and temporary certificate cannot be generated (missing publisher name).  Check your Company Distinguished Name setting.  Signing will probably fail.", SigningCertificatePath);
						}
					}
					else
					{
						LogWarning("No certificate found at {0} and temporary certificate cannot be generated (running unattended).", SigningCertificatePath);
					}
				}

				string SignToolPath = Path.Combine(SDKFolder, "bin", Environment.Is64BitProcess ? "x64" : "x86", "signtool.exe");
				string SignToolCommandLine = string.Format(@"sign /a /f ""{0}"" /fd SHA256 ""{1}""", Path.Combine(SC.ProjectRoot, SigningCertificate), OutputAppX);
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

		private void GenerateSigningCertificate(string InSDKFolder, string InCertificatePath, string InPublisher)
		{
			// MakeCert.exe -r -h 0 -n "CN=No Publisher, O=No Publisher" -eku 1.3.6.1.5.5.7.3.3 -pe -sv "Signing Certificate.pvk" "Signing Certificate.cer"
			// pvk2pfx -pvk "Signing Certificate.pvk" -spc "Signing Certificate.cer" -pfx "Signing Certificate.pfx"
			string MakeCertPath = Path.Combine(InSDKFolder, "bin", Environment.Is64BitProcess ? "x64" : "x86", "makecert.exe");
			string Pvk2PfxPath = Path.Combine(InSDKFolder, "bin", Environment.Is64BitProcess ? "x64" : "x86", "pvk2pfx.exe");
			string CerFile = Path.ChangeExtension(InCertificatePath, ".cer");
			string PvkFile = Path.ChangeExtension(InCertificatePath, ".pvk");

			string MakeCertCommandLine = string.Format(@"-r -h 0 -n ""{0}"" -eku 1.3.6.1.5.5.7.3.3 -pe -sv ""{1}"" ""{2}""", InPublisher, PvkFile, CerFile);
			RunAndLog(CmdEnv, MakeCertPath, MakeCertCommandLine, null, 0, null, ERunOptions.None);

			string Pvk2PfxCommandLine = string.Format(@"-pvk ""{0}"" -spc ""{1}"" -pfx ""{2}""", PvkFile, CerFile, InCertificatePath);
			RunAndLog(CmdEnv, Pvk2PfxPath, Pvk2PfxCommandLine, null, 0, null, ERunOptions.None);
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
