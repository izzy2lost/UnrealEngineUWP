using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Threading.Tasks;
using AutomationTool;
using UnrealBuildTool;
using System.Diagnostics;

namespace UWP.Automation
{
	class UWPLauncherCreatedProcess : IProcessResult
	{
		Process ExternallyLaunchedProcess;
		System.Threading.Thread LogTailThread;
		bool AllowSpew;
		LogEventType SpewVerbosity;

		public UWPLauncherCreatedProcess(Process InExternallyLaunchedProcess, string LogFile, bool InAllowSpew, LogEventType InSpewVerbosity)
		{
			ExternallyLaunchedProcess = InExternallyLaunchedProcess;
			AllowSpew = InAllowSpew;
			SpewVerbosity = InSpewVerbosity;

			// Can't redirect stdout, so tail the log file
			if (AllowSpew)
			{
				LogTailThread = new System.Threading.Thread(() => TailLogFile(LogFile));
				LogTailThread.Start();
			}
			else
			{
				LogTailThread = null;
			}
		}

		~UWPLauncherCreatedProcess()
		{
			if (ExternallyLaunchedProcess != null)
			{
				ExternallyLaunchedProcess.Dispose();
			}
		}

		public int ExitCode
		{
			get
			{
				// Can't access the exit code of a process we didn't start
				return 0;
			}

			set
			{
				throw new NotImplementedException();
			}
		}

		public bool HasExited
		{
			get
			{
				// Avoid potential access of the exit code.
				return ExternallyLaunchedProcess != null && ExternallyLaunchedProcess.HasExited;
			}
		}

		public string Output
		{
			get
			{
				return string.Empty;
			}
		}

		public Process ProcessObject
		{
			get
			{
				return ExternallyLaunchedProcess;
			}
		}

		public void DisposeProcess()
		{
			ExternallyLaunchedProcess.Dispose();
			ExternallyLaunchedProcess = null;
		}

		public string GetProcessName()
		{
			return ExternallyLaunchedProcess.ProcessName;
		}

		public void OnProcessExited()
		{
			ProcessManager.RemoveProcess(this);
			if (LogTailThread != null)
			{
				LogTailThread.Join();
				LogTailThread = null;
			}
		}

		public void StdErr(object sender, DataReceivedEventArgs e)
		{
			throw new NotImplementedException();
		}

		public void StdOut(object sender, DataReceivedEventArgs e)
		{
			throw new NotImplementedException();
		}

		public void StopProcess(bool KillDescendants = true)
		{
			string ProcessNameForLogging = GetProcessName();
			try
			{
				Process ProcessToKill = ExternallyLaunchedProcess;
				ExternallyLaunchedProcess = null;
				if (!ProcessToKill.CloseMainWindow())
				{
					CommandUtils.LogWarning("{0} did not respond to close request.  Killing...", ProcessNameForLogging);
					ProcessToKill.Kill();
					ProcessToKill.WaitForExit(60000);
				}
				if (!ProcessToKill.HasExited)
				{
					CommandUtils.LogLog("Process {0} failed to exit.", ProcessNameForLogging);
				}
				else
				{
					CommandUtils.LogLog("Process {0} successfully exited.", ProcessNameForLogging);
					OnProcessExited();
				}
				ProcessToKill.Close();
			}
			catch (Exception Ex)
			{
				CommandUtils.LogWarning("Exception while trying to kill process {0}:", ProcessNameForLogging);
				CommandUtils.LogWarning(LogUtils.FormatException(Ex));
			}
		}

		public void WaitForExit()
		{
			ExternallyLaunchedProcess.WaitForExit();
			if (LogTailThread != null)
			{
				LogTailThread.Join();
				LogTailThread = null;
			}
		}

		private void TailLogFile(string LogFilePath)
		{
			string LogName = GetProcessName();
			while (!HasExited && !File.Exists(LogFilePath))
			{
				System.Threading.Thread.Sleep(1000);
			}

			if (File.Exists(LogFilePath))
			{
				using (StreamReader LogReader = new StreamReader(new FileStream(LogFilePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite)))
				{
					while (!HasExited)
					{
						System.Threading.Thread.Sleep(5000);
						while (!LogReader.EndOfStream)
						{
							string LogLine = LogReader.ReadLine();
							Log.WriteLine(1, LogName, SpewVerbosity, LogLine);
						}
					}
				}
			}
		}
	}

	class UWPDevicePortalCreatedProcess : IProcessResult
	{
		object StateLock;
		Microsoft.Tools.WindowsDevicePortal.DevicePortal Portal;
		uint ProcId;
		string PackageName;
		string FriendlyName;

		bool ProcessHasExited;

		public UWPDevicePortalCreatedProcess(Microsoft.Tools.WindowsDevicePortal.DevicePortal InPortal, string InPackageName, string InFriendlyName, uint InProcId)
		{
			Portal = InPortal;
			ProcId = InProcId;
			PackageName = InPackageName;
			FriendlyName = InFriendlyName;
			StateLock = new object();
			Portal.RunningProcessesMessageReceived += Portal_RunningProcessesMessageReceived;
			Portal.StartListeningForRunningProcessesAsync().Wait();
			//MonitorThread = new System.Threading.Thread(()=>(MonitorProcess))

			// No ETW on Xbox One, so can't collect trace that way
			if (Portal.Platform != Microsoft.Tools.WindowsDevicePortal.DevicePortal.DevicePortalPlatforms.XboxOne)
			{
				Guid MicrosoftWindowsDiagnoticsLoggingChannelId = new Guid(0x4bd2826e, 0x54a1, 0x4ba9, 0xbf, 0x63, 0x92, 0xb7, 0x3e, 0xa1, 0xac, 0x4a);
				Portal.ToggleEtwProviderAsync(MicrosoftWindowsDiagnoticsLoggingChannelId).Wait();
				Portal.RealtimeEventsMessageReceived += Portal_RealtimeEventsMessageReceived;
				Portal.StartListeningForEtwEventsAsync().Wait();
			}
		}

		private void Portal_RealtimeEventsMessageReceived(Microsoft.Tools.WindowsDevicePortal.DevicePortal sender, Microsoft.Tools.WindowsDevicePortal.WebSocketMessageReceivedEventArgs<Microsoft.Tools.WindowsDevicePortal.DevicePortal.EtwEvents> args)
		{
			foreach (var EtwEvent in args.Message.Events)
			{
				if (EtwEvent.ContainsKey("ProviderName") && EtwEvent.ContainsKey("StringMessage"))
				{
					if (EtwEvent["ProviderName"] == FriendlyName)
					{
						Log.WriteLine(1, FriendlyName, GetLogVerbosityFromEventLevel(EtwEvent.Level), EtwEvent["StringMessage"].Trim('\"'));
					}
				}
			}
		}

		private LogEventType GetLogVerbosityFromEventLevel(uint EtwLevel)
		{
			switch (EtwLevel)
			{
				case 4:
					return LogEventType.Console;
				case 3:
					return LogEventType.Warning;
				case 2:
					return LogEventType.Error;
				case 1:
					return LogEventType.Fatal;
				default:
					return LogEventType.Verbose;
			}
		}

		private void Portal_RunningProcessesMessageReceived(Microsoft.Tools.WindowsDevicePortal.DevicePortal sender, Microsoft.Tools.WindowsDevicePortal.WebSocketMessageReceivedEventArgs<Microsoft.Tools.WindowsDevicePortal.DevicePortal.RunningProcesses> args)
		{
			foreach (var P in args.Message.Processes)
			{
				if (P.ProcessId == ProcId)
				{
					return;
				}
			}
			ProcessHasExited = true;
		}

		public int ExitCode
		{
			get
			{
				return 0;
			}

			set
			{
				throw new NotImplementedException();
			}
		}

		public bool HasExited
		{
			get
			{
				return ProcessHasExited;
			}
		}

		public string Output
		{
			get
			{
				return string.Empty;
			}
		}

		public Process ProcessObject
		{
			get
			{
				return null;
			}
		}

		public void DisposeProcess()
		{
		}

		public string GetProcessName()
		{
			return FriendlyName;
		}

		public void OnProcessExited()
		{
			Portal.StopListeningForRunningProcessesAsync().Wait();
			if (Portal.Platform != Microsoft.Tools.WindowsDevicePortal.DevicePortal.DevicePortalPlatforms.XboxOne)
			{
				Portal.StopListeningForEtwEventsAsync().Wait();
			}
		}

		public void StdErr(object sender, DataReceivedEventArgs e)
		{
			throw new NotImplementedException();
		}

		public void StdOut(object sender, DataReceivedEventArgs e)
		{
			throw new NotImplementedException();
		}

		public void StopProcess(bool KillDescendants = true)
		{
			Portal.TerminateApplicationAsync(PackageName).Wait();
		}

		public void WaitForExit()
		{
			while (!ProcessHasExited)
			{
				System.Threading.Thread.Sleep(1000);
			}
		}
	}

	public abstract class UWPPlatform : Platform
    {
        public UWPPlatform(UnrealTargetPlatform P)
            : base(P)
        {
        }

		public override void PlatformSetupParams(ref ProjectParams ProjParams)
		{
			base.PlatformSetupParams(ref ProjParams);

			if (ProjParams.Deploy && !ProjParams.Package)
			{
				foreach (string DeviceAddress in ProjParams.DeviceNames)
				{
					if (!IsLocalDevice(DeviceAddress))
					{
						LogWarning("Project will be packaged to support deployment to remote UWP device {0}.", DeviceAddress);
						ProjParams.Package = true;
						break;
					}
				}
			}

			ConfigHierarchy PlatformEngineConfig = null;
			if (ProjParams.EngineConfigs.TryGetValue(PlatformType, out PlatformEngineConfig))
			{
				List<string> ThumbprintsFromConfig = new List<string>();
				if (PlatformEngineConfig.GetArray("/Script/UWPPlatformEditor.UWPTargetSettings", "AcceptThumbprints", out ThumbprintsFromConfig))
				{
					AcceptThumbprints.AddRange(ThumbprintsFromConfig);
				}
			}

		}

		public override void Deploy(ProjectParams Params, DeploymentContext SC)
		{
			foreach (string DeviceAddress in Params.DeviceNames)
			{
				if (IsLocalDevice(DeviceAddress))
				{
					// Special case - we can use PackageManager to allow loose apps, plus we need to
					// apply the loopback exemption in case of cook-on-the-fly
					DeployToLocalDevice(Params, SC);
				}
				else
				{
					DeployToRemoteDevice(DeviceAddress, Params, SC);
				}
			}
		}

		public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
		{
			// Stage all the build products
			UWPExports DeployExports = new UWPExports();
			foreach (StageTarget Target in SC.StageTargets)
			{
				SC.StageBuildProductsFromReceipt(Target.Receipt, Target.RequireFilesExist, Params.bTreatNonShippingBinariesAsDebugFiles);
				DeployExports.AddWinMDReferencesFromReceipt(Target.Receipt, Params.RawProjectPath.Directory, SC.LocalRoot);
			}
			List<string> FullExePaths = new List<string>();
			foreach (string ExecutablePath in SC.StageExecutables)
			{
				FullExePaths.Add(Path.Combine(Params.ProjectBinariesFolder, ExecutablePath + Platform.GetExeExtension(SC.StageTargetPlatform.PlatformType)));
			}
			DeployExports.PrepForUATPackageOrDeploy(Params.RawProjectPath, Params.ShortProjectName, SC.ProjectRoot, SC.StageTargetConfigurations, FullExePaths,
				SC.LocalRoot + "/Engine", Params.Distribution, "", Params.Deploy);

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
			return PlatformType.ToString();
		}

		public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
		{
			FileReference MakeAppXPath = UWPExports.GetWindowsSdkToolPath("makeappx.exe");
			string OutputAppX = Path.Combine(SC.StageDirectory, Params.ShortProjectName + ".appx");
			string MakeAppXCommandLine = string.Format(@"pack /o /d ""{0}"" /p ""{1}""", SC.StageDirectory, OutputAppX);
			RunAndLog(CmdEnv, MakeAppXPath.FullName, MakeAppXCommandLine, null, 0, null, ERunOptions.None);

			string SigningCertificate = @"Build\UWP\SigningCertificate.pfx";
			string SigningCertificatePath = Path.Combine(SC.ProjectRoot, SigningCertificate);
			if (!File.Exists(SigningCertificatePath))
			{
				if (!IsBuildMachine && !Params.Unattended)
				{
					// Extract the publisher name from the AppXManifest
					string AppxManifestPath = GetAppxManifestPath(SC);
					string Name;
					string Publisher;
					string PrimaryAppId;
					GetPackageInfo(AppxManifestPath, out Name, out Publisher, out PrimaryAppId);
					if (!string.IsNullOrEmpty(Publisher))
					{
						LogWarning("No certificate found at {0}.  Generating temporary self-signed certificate for {1}.", SigningCertificatePath, Publisher);
						GenerateSigningCertificate(SigningCertificatePath, Publisher);
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

			// Emit a .cer file adjacent to the appx so it can be installed to enable packaged deployment
			System.Security.Cryptography.X509Certificates.X509Certificate2 ActualCert = new System.Security.Cryptography.X509Certificates.X509Certificate2(Path.Combine(SC.ProjectRoot, SigningCertificate));
			File.WriteAllText(Path.Combine(SC.StageDirectory, Params.ShortProjectName + ".cer"), Convert.ToBase64String(ActualCert.Export(System.Security.Cryptography.X509Certificates.X509ContentType.Cert)));

			FileReference SignToolPath = UWPExports.GetWindowsSdkToolPath("signtool.exe");
			string SignToolCommandLine = string.Format(@"sign /a /f ""{0}"" /fd SHA256 ""{1}""", Path.Combine(SC.ProjectRoot, SigningCertificate), OutputAppX);
			RunAndLog(CmdEnv, SignToolPath.FullName, SignToolCommandLine, null, 0, null, ERunOptions.None);

			// If the user indicated that they will distribute this build, then let's also generate an
			// appxupload file suitable for submission to the Windows Store.  This file zips together
			// the appx package and the public symbols (themselves zipped) for the binaries.
			if (Params.Distribution)
			{
				List<FileReference> SymbolFilesToZip = new List<FileReference>();
				DirectoryReference StageDirRef = new DirectoryReference(SC.StageDirectory);
				DirectoryReference PublicSymbols = DirectoryReference.Combine(StageDirRef, "PublicSymbols");
				CreateDirectory_NoExceptions(PublicSymbols.FullName);
				foreach (StageTarget Target in SC.StageTargets)
				{
					foreach (BuildProduct Product in Target.Receipt.BuildProducts)
					{
						if (Product.Type == BuildProductType.SymbolFile)
						{
							FileReference FullSymbolFile = new FileReference(Product.Path);
							FileReference TempStrippedSymbols = FileReference.Combine(PublicSymbols, FullSymbolFile.GetFileName());
							StripSymbols(FullSymbolFile, TempStrippedSymbols);
							SymbolFilesToZip.Add(TempStrippedSymbols);
						}
					}
				}
				FileReference AppxSymFile = FileReference.Combine(StageDirRef, Params.ShortProjectName + ".appxsym");
				ZipFiles(AppxSymFile, PublicSymbols, SymbolFilesToZip);

				FileReference AppxUploadFile = FileReference.Combine(StageDirRef, Params.ShortProjectName + ".appxupload");
				ZipFiles(AppxUploadFile, StageDirRef,
					new FileReference[]
					{
							new FileReference(OutputAppX),
							AppxSymFile
					});
			}
		}

		public override IProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
		{
			IProcessResult ProcResult = null;
			foreach (string DeviceAddress in Params.DeviceNames)
			{
				// Prefer launcher tool for local device since it avoids having to deal with certificate issues on the
				// device portal connection.
				if (IsLocalDevice(DeviceAddress))
				{
					ProcResult = RunUsingLauncherTool(DeviceAddress, ClientRunFlags, ClientApp, ClientCmdLine, Params);
				}
				else
				{
					ProcResult = RunUsingDevicePortal(DeviceAddress, ClientRunFlags, ClientApp, ClientCmdLine, Params);
				}
			}
			return ProcResult;
		}

		public override List<string> GetExecutableNames(DeploymentContext SC, bool bIsRun = false)
		{
			if (bIsRun)
			{
				// If we're calling this for the purpose of running the app then the string we really
				// need is the AUMID.  We can't form a full AUMID here without making assumptions about 
				// how the PFN is built, which (while straightforward) does not appear to be officially
				// documented.  So we'll save off the path to the manifest, which the launch process can
				// parse later for information that, in conjunction with the target device, will allow
				// for looking up the true AUMID.
				List<string> Exes = new List<string>();
				Exes.Add(GetAppxManifestPath(SC));
				return Exes;
			}
			else
			{
				return base.GetExecutableNames(SC, bIsRun);
			}
		}

		public override bool IsSupported { get { return true; } }
		public override bool UseAbsLog { get { return false; } }
		public override bool LaunchViaUFE { get { return false; } }

		public override List<string> GetDebugFileExtentions()
		{
			return new List<string> { ".pdb", ".map" };
		}

		public override void StripSymbols(FileReference SourceFile, FileReference TargetFile)
		{
			bool bStripInPlace = false;

			if (SourceFile == TargetFile)
			{
				// PDBCopy only supports creation of a brand new stripped file so we have to create a temporary filename
				TargetFile = new FileReference(Path.Combine(TargetFile.Directory.FullName, Guid.NewGuid().ToString() + TargetFile.GetExtension()));
				bStripInPlace = true;
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			FileReference PDBCopyPath = null;

			// VS 2017 puts MSBuild stuff (where PDBCopy lives) under the Visual Studio Installation directory
			DirectoryReference VSInstallDir;
			if (WindowsExports.TryGetVSInstallDir(WindowsCompiler.VisualStudio2017, out VSInstallDir))
			{
				PDBCopyPath = FileReference.Combine(VSInstallDir, "MSBuild", "Microsoft", "VisualStudio", "v15.0", "AppxPackage", "PDBCopy.exe");
			}

			// Earlier versions use a separate MSBuild install location
			if (PDBCopyPath == null || !FileReference.Exists(PDBCopyPath))
			{
				DirectoryReference MSBuildInstallDir = new DirectoryReference(Path.Combine(WindowsExports.GetMSBuildToolPath(), "..", "..", ".."));
				PDBCopyPath = FileReference.Combine(MSBuildInstallDir, "Microsoft", "VisualStudio", "v14.0", "AppxPackage", "PDBCopy.exe");
				if (PDBCopyPath == null || !FileReference.Exists(PDBCopyPath))
				{
					PDBCopyPath = FileReference.Combine(MSBuildInstallDir, "Microsoft", "VisualStudio", "v12.0", "AppxPackage", "PDBCopy.exe");
				}
			}

			StartInfo.FileName = PDBCopyPath.FullName;
			StartInfo.Arguments = String.Format("\"{0}\" \"{1}\" -p", SourceFile.FullName, TargetFile.FullName);
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo);

			if (bStripInPlace)
			{
				// Copy stripped file to original location and delete the temporary file
				File.Copy(TargetFile.FullName, SourceFile.FullName, true);
				FileReference.Delete(TargetFile);
			}
		}

		private void GenerateSigningCertificate(string InCertificatePath, string InPublisher)
		{
			// MakeCert.exe -r -h 0 -n "CN=No Publisher, O=No Publisher" -eku 1.3.6.1.5.5.7.3.3 -pe -sv "Signing Certificate.pvk" "Signing Certificate.cer"
			// pvk2pfx -pvk "Signing Certificate.pvk" -spc "Signing Certificate.cer" -pfx "Signing Certificate.pfx"
			FileReference MakeCertPath = UWPExports.GetWindowsSdkToolPath("makecert.exe");
			FileReference Pvk2PfxPath = UWPExports.GetWindowsSdkToolPath("pvk2pfx.exe");
			string CerFile = Path.ChangeExtension(InCertificatePath, ".cer");
			string PvkFile = Path.ChangeExtension(InCertificatePath, ".pvk");

			string MakeCertCommandLine = string.Format(@"-r -h 0 -n ""{0}"" -eku 1.3.6.1.5.5.7.3.3 -pe -sv ""{1}"" ""{2}""", InPublisher, PvkFile, CerFile);
			RunAndLog(CmdEnv, MakeCertPath.FullName, MakeCertCommandLine, null, 0, null, ERunOptions.None);

			string Pvk2PfxCommandLine = string.Format(@"-pvk ""{0}"" -spc ""{1}"" -pfx ""{2}""", PvkFile, CerFile, InCertificatePath);
			RunAndLog(CmdEnv, Pvk2PfxPath.FullName, Pvk2PfxCommandLine, null, 0, null, ERunOptions.None);
		}

		private bool IsLocalDevice(string DeviceAddress)
		{
			try
			{
				return new Uri(DeviceAddress).IsLoopback;
			}
			catch
			{
				// If we can't parse the address as a Uri we default to local
				return true;
			}
		}

		private void DeployToLocalDevice(ProjectParams Params, DeploymentContext SC)
		{
			string AppxManifestPath = GetAppxManifestPath(SC);
			string Name;
			string Publisher;
			string PrimaryAppId;
			GetPackageInfo(AppxManifestPath, out Name, out Publisher, out PrimaryAppId);
			Windows.Management.Deployment.PackageManager PackMgr = new Windows.Management.Deployment.PackageManager();
			try
			{
				var ExistingPackage = PackMgr.FindPackagesForUser("", Name, Publisher).FirstOrDefault();

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
					// Register appears to expect dependency packages to be loose too, which the VC runtime is not, so skip it and hope 
					// that it's already installed on the local machine (almost certainly true given that we aren't currently picky about exact version)
					PackMgr.RegisterPackageAsync(new Uri(AppxManifestPath), null, Windows.Management.Deployment.DeploymentOptions.DevelopmentMode).AsTask().Wait();
				}
				else
				{
					string PackagePath = Path.Combine(SC.StageDirectory, Params.ShortProjectName + ".appx");

					List<Uri> Dependencies = new List<Uri>();
					TargetRules Rules = Params.ProjectTargets[TargetType.Game].Rules;
					bool UseDebugCrt = Params.ClientConfigsToBuild.Contains(UnrealTargetConfiguration.Debug) && Rules.bDebugBuildsActuallyUseDebugCRT;
					Dependencies.Add(new Uri(GetPathToVCLibsPackage(UseDebugCrt, Rules.WindowsPlatform.Compiler)));

					PackMgr.AddPackageAsync(new Uri(PackagePath), Dependencies, Windows.Management.Deployment.DeploymentOptions.None).AsTask().Wait();
				}
			}
			catch (AggregateException agg)
			{
				throw new AutomationException(ExitCode.Error_AppInstallFailed, agg.InnerException, "");
			}

			// Package should now be installed.  Locate it and make sure it's permitted to connect over loopback.
			try
			{
				var InstalledPackage = PackMgr.FindPackagesForUser("", Name, Publisher).FirstOrDefault();
				string LoopbackExemptCmdLine = string.Format("loopbackexempt -a -n={0}", InstalledPackage.Id.FamilyName);
				RunAndLog(CmdEnv, "checknetisolation.exe", LoopbackExemptCmdLine, null, 0, null, ERunOptions.None);
			}
			catch
			{
				LogWarning("Failed to apply a loopback exemption to the deployed app.  Connection to a local cook server will fail.");
			}
		}

		private void DeployToRemoteDevice(string DeviceAddress, ProjectParams Params, DeploymentContext SC)
		{
			if (Params.Package)
			{
				Microsoft.Tools.WindowsDevicePortal.DefaultDevicePortalConnection conn = new Microsoft.Tools.WindowsDevicePortal.DefaultDevicePortalConnection(DeviceAddress, Params.DeviceUsername, Params.DevicePassword);
				Microsoft.Tools.WindowsDevicePortal.DevicePortal portal = new Microsoft.Tools.WindowsDevicePortal.DevicePortal(conn);
				portal.UnvalidatedCert += (sender, certificate, chain, sslPolicyErrors) =>
				{
					return ShouldAcceptCertificate(new System.Security.Cryptography.X509Certificates.X509Certificate2(certificate), Params.Unattended);
				};
				portal.ConnectionStatus += Portal_ConnectionStatus;
				try
				{
					portal.ConnectAsync().Wait();

					string PackagePath = Path.Combine(SC.StageDirectory, Params.ShortProjectName + ".appx");
					string CertPath = Path.Combine(SC.StageDirectory, Params.ShortProjectName + ".cer");

					List<string> Dependencies = new List<string>();
					TargetRules Rules = Params.ProjectTargets[TargetType.Game].Rules;
					bool UseDebugCrt = Params.ClientConfigsToBuild.Contains(UnrealTargetConfiguration.Debug) && Rules.bDebugBuildsActuallyUseDebugCRT;
					Dependencies.Add(GetPathToVCLibsPackage(UseDebugCrt, Rules.WindowsPlatform.Compiler));

					portal.AppInstallStatus += Portal_AppInstallStatus;
					portal.InstallApplicationAsync(string.Empty, PackagePath, Dependencies, CertPath).Wait();
				}
				catch (AggregateException e)
				{
					if (e.InnerException is AutomationException)
					{
						throw e.InnerException;
					}
					else
					{
						throw new AutomationException(ExitCode.Error_AppInstallFailed, e.InnerException, e.InnerException.Message);
					}
				}
				catch (Exception e)
				{
					throw new AutomationException(ExitCode.Error_AppInstallFailed, e, e.Message);
				}
			}
			else
			{
				throw new AutomationException(ExitCode.Error_AppInstallFailed, "Remote deployment of unpackaged apps is not supported.");
			}
		}

		private IProcessResult RunUsingLauncherTool(string DeviceAddress, ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
		{
			string Name;
			string Publisher;
			string PrimaryAppId;
			GetPackageInfo(ClientApp, out Name, out Publisher, out PrimaryAppId);

			Windows.Management.Deployment.PackageManager PackMgr = new Windows.Management.Deployment.PackageManager();
			Windows.ApplicationModel.Package InstalledPackage = PackMgr.FindPackagesForUser("", Name, Publisher).FirstOrDefault();

			if (InstalledPackage == null)
			{
				throw new AutomationException(ExitCode.Error_LauncherFailed, "Could not find installed app (Name: {0}, Publisher: {1}", Name, Publisher);
			}

			string Aumid = string.Format("{0}!{1}", InstalledPackage.Id.FamilyName, PrimaryAppId);
			string SDKFolder = UWPExports.FindWindowsSDKInstallationFolder();
			string LauncherPath = Path.Combine(SDKFolder, "App Certification Kit", "microsoft.windows.softwarelogo.appxlauncher.exe");
			IProcessResult LauncherProc = Run(LauncherPath, Aumid);
			LauncherProc.WaitForExit();
			string LogFile;
			if (Params.CookOnTheFly)
			{
				LogFile = Path.Combine(Params.RawProjectPath.Directory.FullName, "Saved", "Cooked", PlatformType.ToString(), Params.ShortProjectName, "UWPLocalAppData", Params.ShortProjectName, "Saved", "Logs", Params.ShortProjectName + ".log");
			}
			else
			{
				LogFile = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Packages", InstalledPackage.Id.FamilyName, "LocalState", Params.ShortProjectName, "Saved", "Logs", Params.ShortProjectName + ".log");
			}
			System.Diagnostics.Process Proc = System.Diagnostics.Process.GetProcessById(LauncherProc.ExitCode);
			bool AllowSpew = ClientRunFlags.HasFlag(ERunOptions.AllowSpew);
			UnrealBuildTool.LogEventType SpewVerbosity = ClientRunFlags.HasFlag(ERunOptions.SpewIsVerbose) ? UnrealBuildTool.LogEventType.Verbose : UnrealBuildTool.LogEventType.Console;
			UWPLauncherCreatedProcess UwpProcessResult = new UWPLauncherCreatedProcess(Proc, LogFile, AllowSpew, SpewVerbosity);
			ProcessManager.AddProcess(UwpProcessResult);
			if (!ClientRunFlags.HasFlag(ERunOptions.NoWaitForExit))
			{
				UwpProcessResult.WaitForExit();
				UwpProcessResult.OnProcessExited();
				UwpProcessResult.DisposeProcess();
			}
			return UwpProcessResult;
		}

		private IProcessResult RunUsingDevicePortal(string DeviceAddress, ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
		{
			string Name;
			string Publisher;
			string PrimaryAppId;
			GetPackageInfo(ClientApp, out Name, out Publisher, out PrimaryAppId);

			Microsoft.Tools.WindowsDevicePortal.DefaultDevicePortalConnection conn = new Microsoft.Tools.WindowsDevicePortal.DefaultDevicePortalConnection(DeviceAddress, Params.DeviceUsername, Params.DevicePassword);
			Microsoft.Tools.WindowsDevicePortal.DevicePortal portal = new Microsoft.Tools.WindowsDevicePortal.DevicePortal(conn);
			portal.UnvalidatedCert += (sender, certificate, chain, sslPolicyErrors) =>
			{
				return ShouldAcceptCertificate(new System.Security.Cryptography.X509Certificates.X509Certificate2(certificate), Params.Unattended);
			};

			try
			{
				portal.ConnectAsync().Wait();
				string Aumid = string.Empty;
				string FullName = string.Empty;
				var AllAppsTask = portal.GetInstalledAppPackagesAsync();
				AllAppsTask.Wait();
				foreach (var App in AllAppsTask.Result.Packages)
				{
					// App.Name seems to report the localized name.
					if (App.FamilyName.StartsWith(Name) && App.Publisher == Publisher)
					{
						Aumid = App.AppId;
						FullName = App.FullName;
						break;
					}
				}

				var LaunchTask = portal.LaunchApplicationAsync(Aumid, FullName);
				LaunchTask.Wait();
				IProcessResult Result = new UWPDevicePortalCreatedProcess(portal, FullName, Params.ShortProjectName, LaunchTask.Result);

				ProcessManager.AddProcess(Result);
				if (!ClientRunFlags.HasFlag(ERunOptions.NoWaitForExit))
				{
					Result.WaitForExit();
					Result.OnProcessExited();
					Result.DisposeProcess();
				}
				return Result;
			}
			catch (AggregateException e)
			{
				if (e.InnerException is AutomationException)
				{
					throw e.InnerException;
				}
				else
				{
					throw new AutomationException(ExitCode.Error_LauncherFailed, e.InnerException, e.InnerException.Message);
				}
			}
			catch (Exception e)
			{
				throw new AutomationException(ExitCode.Error_LauncherFailed, e, e.Message);
			}
		}

		private string GetAppxManifestPath(DeploymentContext SC)
		{
			return Path.Combine(SC.Stage ? SC.StageDirectory : SC.ProjectBinariesFolder, "AppxManifest.xml");
		}

		private void GetPackageInfo(string AppxManifestPath, out string Name, out string Publisher, out string PrimaryAppId)
		{
			System.Xml.Linq.XDocument Doc = System.Xml.Linq.XDocument.Load(AppxManifestPath);
			System.Xml.Linq.XElement Package = Doc.Root;
			System.Xml.Linq.XElement Identity = Package.Element(System.Xml.Linq.XName.Get("Identity", Package.Name.NamespaceName));
			Name = Identity.Attribute("Name").Value;
			Publisher = Identity.Attribute("Publisher").Value;
			System.Xml.Linq.XElement Applications = Package.Element(System.Xml.Linq.XName.Get("Applications", Package.Name.NamespaceName));
			System.Xml.Linq.XElement PrimaryApp = Applications.Element(System.Xml.Linq.XName.Get("Application", Package.Name.NamespaceName));
			PrimaryAppId = PrimaryApp.Attribute("Id").Value;
		}

		private bool ShouldAcceptCertificate(System.Security.Cryptography.X509Certificates.X509Certificate2 Certificate, bool Unattended)
		{
			if (AcceptThumbprints.Contains(Certificate.Thumbprint))
			{
				return true;
			}

			if (Unattended)
			{
				throw new AutomationException(ExitCode.Error_CertificateNotFound, "Cannot connect to remote device: certificate is untrusted and cannot prompt for consent (running unattended).");
			}

			System.Windows.Forms.DialogResult AcceptResult = System.Windows.Forms.MessageBox.Show(
				string.Format("Do you want to accept the following certificate?\n\nThumbprint:\n\t{0}\nIssues:\n\t{1}", Certificate.Thumbprint, Certificate.Issuer),
				"Untrusted Certificate Detected",
				System.Windows.Forms.MessageBoxButtons.YesNo,
				System.Windows.Forms.MessageBoxIcon.Question,
				System.Windows.Forms.MessageBoxDefaultButton.Button2);
			if (AcceptResult == System.Windows.Forms.DialogResult.Yes)
			{
				AcceptThumbprints.Add(Certificate.Thumbprint);
				return true;
			}

			throw new AutomationException(ExitCode.Error_CertificateNotFound, "Cannot connect to remote device: certificate is untrusted and user declined to accept.");
		}

		private void Portal_AppInstallStatus(Microsoft.Tools.WindowsDevicePortal.DevicePortal sender, Microsoft.Tools.WindowsDevicePortal.ApplicationInstallStatusEventArgs args)
		{
			if (args.Status == Microsoft.Tools.WindowsDevicePortal.ApplicationInstallStatus.Failed)
			{
				throw new AutomationException(ExitCode.Error_AppInstallFailed, args.Message);
			}
			else
			{
				LogLog(args.Message);
			}
		}

		private void Portal_ConnectionStatus(Microsoft.Tools.WindowsDevicePortal.DevicePortal sender, Microsoft.Tools.WindowsDevicePortal.DeviceConnectionStatusEventArgs args)
		{
			if (args.Status == Microsoft.Tools.WindowsDevicePortal.DeviceConnectionStatus.Failed)
			{
				throw new AutomationException(args.Message);
			}
			else
			{
				LogLog(args.Message);
			}
		}

        private string GetPathToVCLibsPackage(bool UseDebugCrt, WindowsCompiler Compiler)
		{
			string VCVersionFragment;
            switch (Compiler)
			{
				case WindowsCompiler.VisualStudio2017:
				//Compiler version is still 14 for 2017
				case WindowsCompiler.VisualStudio2015:
				case WindowsCompiler.Default:
					VCVersionFragment = "14";
					break;

				default:
					VCVersionFragment = "Unsupported_VC_Version";
					break;
			}

			string ArchitectureFragment;
			switch (PlatformType)
			{
				case UnrealTargetPlatform.UWP64:
					ArchitectureFragment = "x64";
					break;
				case UnrealTargetPlatform.UWP32:
					ArchitectureFragment = "x86";
					break;
				default:
					ArchitectureFragment = "Unknown_Architecture";
					break;
			}

			return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
				"Microsoft SDKs",
				"Windows Kits",
				"10",
				"ExtensionSDKs",
				"Microsoft.VCLibs",
				string.Format("{0}.0", VCVersionFragment),
				"Appx",
				UseDebugCrt ? "Debug" : "Retail",
				ArchitectureFragment,
				string.Format("Microsoft.VCLibs.{0}.{1}.00.appx", ArchitectureFragment, VCVersionFragment));
		}

		private static List<string> AcceptThumbprints = new List<string>();
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
