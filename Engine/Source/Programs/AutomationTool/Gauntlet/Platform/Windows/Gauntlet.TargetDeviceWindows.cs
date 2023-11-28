// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Text.RegularExpressions;
using EpicGames.Core;
using static AutomationTool.ProcessResult;

namespace Gauntlet
{
	/// <summary>
	/// Win32/64 implementation of a device to run applications
	/// </summary>
	public class TargetDeviceWindows : TargetDeviceDesktopCommon
	{
		public TargetDeviceWindows(string InName, string InCacheDir)
			: base(InName, InCacheDir)
		{
			Platform = UnrealTargetPlatform.Win64;
			RunOptions = CommandUtils.ERunOptions.NoWaitForExit | CommandUtils.ERunOptions.NoLoggingOfRunCommand;
		}

		public override IAppInstall InstallApplication(UnrealAppConfig AppConfig)
		{
			switch (AppConfig.Build)
			{
				case NativeStagedBuild:
					return InstallNativeStagedBuild(AppConfig, AppConfig.Build as NativeStagedBuild);

				case StagedBuild:
					return InstallStagedBuild(AppConfig, AppConfig.Build as StagedBuild);

				case EditorBuild:
					return InstallEditorBuild(AppConfig, AppConfig.Build as EditorBuild);

				case IWindowsSelfInstallingBuild:
					return InstallSelfInstallingBuild(AppConfig, AppConfig.Build as IWindowsSelfInstallingBuild);

				default:
					throw new AutomationException("{0} is an invalid build type!", AppConfig.Build.ToString());
			}
		}

		public override IAppInstance Run(IAppInstall App)
		{
			WindowsAppInstall WinApp = App as WindowsAppInstall;

			if (WinApp == null)
			{
				throw new DeviceException("AppInstance is of incorrect type!");
			}

			if (File.Exists(WinApp.ExecutablePath) == false)
			{
				throw new DeviceException("Specified path {0} not found!", WinApp.ExecutablePath);
			}

			IProcessResult Result = null;
			string ProcessLogFile = null;

			lock (Globals.MainLock)
			{
				string ExePath = Path.GetDirectoryName(WinApp.ExecutablePath);
				string NewWorkingDir = string.IsNullOrEmpty(WinApp.WorkingDirectory) ? ExePath : WinApp.WorkingDirectory;
				string OldWD = Environment.CurrentDirectory;
				Environment.CurrentDirectory = NewWorkingDir;

				Log.Info("Launching {0} on {1}", App.Name, ToString());

				string CmdLine = WinApp.CommandArguments;

				if (WinApp.CanAlterCommandArgs)
				{
					// Look in app parameters if abslog is specified, if so use it
					Regex CLRegex = new Regex(@"(--?[a-zA-Z]+)[:\s=]?([A-Z]:(?:\\[\w\s-]+)+\\?(?=\s-)|\""[^\""]*\""|[^-][^\s]*)?");
					foreach (Match M in CLRegex.Matches(CmdLine))
					{
						if (M.Groups.Count == 3 && M.Groups[1].Value == "-abslog")
						{
							ProcessLogFile = M.Groups[2].Value;
						}
					}
				}

				// Explicitly set log file when not already defined if not build machine
				// -abslog makes sure Unreal dymanicaly update the log window when using -log
				if (WinApp.CanAlterCommandArgs && !AutomationTool.Automation.IsBuildMachine && string.IsNullOrEmpty(ProcessLogFile))
				{
					string LogFolder = string.Format(@"{0}\Logs", WinApp.ArtifactPath);

					if (!Directory.Exists(LogFolder))
					{
						Directory.CreateDirectory(LogFolder);
					}

					ProcessLogFile = string.Format("{0}\\{1}.log", LogFolder, WinApp.ProjectName);
					CmdLine = string.Format("{0} -abslog=\"{1}\"", CmdLine, ProcessLogFile);
				}

				// cleanup any existing log file
				try
				{
					if (ProcessLogFile != null && File.Exists(ProcessLogFile))
					{
						EpicGames.Core.FileUtils.ForceDeleteFile(ProcessLogFile);
					}
				}
				catch (Exception Ex)
				{
					//throw new AutomationException("Unable to delete existing log file {0} {1}", ProcessLogFile, Ex.Message);
					Log.Warning(KnownLogEvents.Gauntlet_DeviceEvent, "Unable to delete existing log file {File}. {Exception}", ProcessLogFile, Ex.Message);
				}

				Log.Verbose("\t{0}", CmdLine);

				CommandUtils.ERunOptions FinalRunOptions = WinApp.RunOptions;
				if (!WinApp.CanAlterCommandArgs)
				{
					CmdLine = WinApp.CommandArguments;
				}

				FinalRunOptions = WinApp.RunOptions | (ProcessLogFile != null ? CommandUtils.ERunOptions.NoStdOutRedirect : 0);

				Result = CommandUtils.Run(WinApp.ExecutablePath,
					CmdLine,
					Options: FinalRunOptions,
					SpewFilterCallback: new SpewFilterCallbackType(M => { return ProcessLogFile == null ? M : null; }) /* make sure stderr does not spew in the stdout */,
					WorkingDir: WinApp.WorkingDirectory);

				if (Result.HasExited && Result.ExitCode != 0)
				{
					throw new AutomationException("Failed to launch {0}. Error {1}", WinApp.ExecutablePath, Result.ExitCode);
				}

				Environment.CurrentDirectory = OldWD;
			}

			return new WindowsAppInstance(WinApp, Result, ProcessLogFile);
		}

		protected override IAppInstall InstallNativeStagedBuild(UnrealAppConfig AppConfig, NativeStagedBuild InBuild)
		{
			WindowsAppInstall WinApp = new WindowsAppInstall(AppConfig.Name, AppConfig.ProjectName, this);
			WinApp.CanAlterCommandArgs = AppConfig.CanAlterCommandArgs;

			WinApp.RunOptions = RunOptions;
			if (Log.IsVeryVerbose)
			{
				WinApp.RunOptions |= CommandUtils.ERunOptions.AllowSpew;
			}

			WinApp.WorkingDirectory = InBuild.BuildPath;
			WinApp.ExecutablePath = Path.Combine(InBuild.BuildPath, InBuild.ExecutablePath);
			WinApp.CommandArguments = AppConfig.CommandLine;

			WinApp.ArtifactPath = Path.Combine(InBuild.BuildPath, AppConfig.ProjectName, @"Saved");
			WinApp.CleanDeviceArtifacts();

			CopyAdditionalFiles(AppConfig);

			return WinApp;
		}

		protected override IAppInstall InstallStagedBuild(UnrealAppConfig AppConfig, StagedBuild InBuild)
		{
			string BuildPath = InBuild.BuildPath;

			if (Utils.SystemHelpers.IsNetworkPath(BuildPath))
			{
				string SubDir = string.IsNullOrEmpty(AppConfig.Sandbox) ? AppConfig.ProjectName : AppConfig.Sandbox;
				string BasePath = string.IsNullOrEmpty(AppConfig.DestLocalInstallDir) ? this.LocalCachePath : AppConfig.DestLocalInstallDir;
				string DestPath = Path.Combine(BasePath, SubDir, AppConfig.ProcessType.ToString());

				if (!AppConfig.SkipInstall)
				{
					DestPath = StagedBuild.InstallBuildParallel(AppConfig, InBuild, BuildPath, DestPath, ToString());
				}
				else
				{
					Log.Info("Skipping install of {0} (-skipdeploy)", BuildPath);
				}

				Utils.SystemHelpers.MarkDirectoryForCleanup(DestPath);

				BuildPath = DestPath;
			}

			WindowsAppInstall WinApp = new WindowsAppInstall(AppConfig.Name, AppConfig.ProjectName, this);
			WinApp.RunOptions = RunOptions;
			WinApp.CanAlterCommandArgs = AppConfig.CanAlterCommandArgs;

			// Set commandline replace any InstallPath arguments with the path we use
			WinApp.CommandArguments = Regex.Replace(AppConfig.CommandLine, @"\$\(InstallPath\)", BuildPath, RegexOptions.IgnoreCase);

			if (string.IsNullOrEmpty(UserDir) == false)
			{
				WinApp.CommandArguments += string.Format(" -userdir=\"{0}\"", UserDir);
				WinApp.ArtifactPath = Path.Combine(UserDir, @"Saved");

				Utils.SystemHelpers.MarkDirectoryForCleanup(UserDir);
			}
			else
			{
				// e.g d:\Unreal\GameName\Saved
				WinApp.ArtifactPath = Path.Combine(BuildPath, AppConfig.ProjectName, @"Saved");

			}

			// clear artifact path
			WinApp.CleanDeviceArtifacts();

			if (LocalDirectoryMappings.Count == 0)
			{
				PopulateDirectoryMappings(Path.Combine(BuildPath, AppConfig.ProjectName));
			}

			CopyAdditionalFiles(AppConfig);

			if (Path.IsPathRooted(InBuild.ExecutablePath))
			{
				WinApp.ExecutablePath = InBuild.ExecutablePath;
			}
			else
			{
				// TODO - this check should be at a higher level....
				string BinaryPath = Path.Combine(BuildPath, InBuild.ExecutablePath);

				// check for a local newer executable
				if (Globals.Params.ParseParam("dev") && AppConfig.ProcessType.UsesEditor() == false)
				{
					string LocalBinary = Path.Combine(Environment.CurrentDirectory, InBuild.ExecutablePath);

					bool LocalFileExists = File.Exists(LocalBinary);
					bool LocalFileNewer = LocalFileExists && File.GetLastWriteTime(LocalBinary) > File.GetLastWriteTime(BinaryPath);

					Log.Verbose("Checking for newer binary at {0}", LocalBinary);
					Log.Verbose("LocalFile exists: {0}. Newer: {1}", LocalFileExists, LocalFileNewer);

					if (LocalFileExists && LocalFileNewer)
					{
						// need to -basedir to have our exe load content from the path
						WinApp.CommandArguments += string.Format(" -basedir={0}", Path.GetDirectoryName(BinaryPath));

						BinaryPath = LocalBinary;
					}
				}

				WinApp.ExecutablePath = BinaryPath;
			}

			return WinApp;
		}

		protected override IAppInstall InstallEditorBuild(UnrealAppConfig AppConfig, EditorBuild Build)
		{
			WindowsAppInstall WinApp = new WindowsAppInstall(AppConfig.Name, AppConfig.ProjectName, this);

			WinApp.WorkingDirectory = Path.GetDirectoryName(Build.ExecutablePath);
			WinApp.RunOptions = RunOptions;

			// Force this to stop logs and other artifacts going to different places
			WinApp.CommandArguments = AppConfig.CommandLine + string.Format(" -userdir=\"{0}\"", UserDir);
			WinApp.ArtifactPath = Path.Combine(UserDir, @"Saved");
			WinApp.ExecutablePath = Build.ExecutablePath;

			if (LocalDirectoryMappings.Count == 0)
			{
				PopulateDirectoryMappings(AppConfig.ProjectFile.Directory.FullName);
			}

			CopyAdditionalFiles(AppConfig);

			return WinApp;
		}

		protected IAppInstall InstallSelfInstallingBuild(UnrealAppConfig AppConfig, IWindowsSelfInstallingBuild Build)
		{
			WindowsAppInstall WinApp = Build.Install(this, AppConfig, out string BasePath);

			if (Log.IsVeryVerbose)
			{
				WinApp.RunOptions |= CommandUtils.ERunOptions.AllowSpew;
			}

			if (string.IsNullOrEmpty(UserDir) == false)
			{
				WinApp.CommandArguments += string.Format(" -userdir=\"{0}\"", UserDir);
				WinApp.ArtifactPath = Path.Combine(UserDir, @"Saved");
				Utils.SystemHelpers.MarkDirectoryForCleanup(UserDir);
			}
			else
			{
				WinApp.ArtifactPath = Path.Combine(BasePath, AppConfig.ProjectName, @"Saved");
			}
			WinApp.CleanDeviceArtifacts();

			if (LocalDirectoryMappings.Count == 0)
			{
				PopulateDirectoryMappings(Path.Combine(BasePath, AppConfig.ProjectName));
			}

			CopyAdditionalFiles(AppConfig);
			return WinApp;
		}
	}

	public class WindowsAppInstall : DesktopCommonAppInstall<TargetDeviceWindows>, IAppInstall.IDynamicCommandLine
	{
		public bool CanAlterCommandArgs;

		[Obsolete("Will be removed in a future release. Use 'DesktopDevice' instead.")]
		public TargetDeviceWindows WinDevice => DesktopDevice;

		public override string CommandArguments
		{
			get { return CommandArgumentsPrivate; }
			set
			{
				if (CanAlterCommandArgs || string.IsNullOrEmpty(CommandArgumentsPrivate))
				{
					CommandArgumentsPrivate = value;
				}
				else
				{
					Log.Info("Skipped setting command AppInstall line when CanAlterCommandArgs = false");
				}
			}
		}

		private string CommandArgumentsPrivate;

		public WindowsAppInstall(string InName, string InProjectName, TargetDeviceWindows InDevice)
			: base(InName, InProjectName, InDevice)
		{
			CanAlterCommandArgs = true;
		}

		public void AppendCommandline(string AdditionalCommandline)
		{
			CommandArguments += AdditionalCommandline;
		}
	}

	public class WindowsAppInstance : DesktopCommonAppInstance<WindowsAppInstall, TargetDeviceWindows>
	{
		public WindowsAppInstance(WindowsAppInstall InInstall, IProcessResult InProcess, string ProcessLogFile = null)
			: base(InInstall, InProcess, ProcessLogFile)
		{ }
	}

	public class Win64DeviceFactory : IDeviceFactory
	{
		public bool CanSupportPlatform(UnrealTargetPlatform? Platform)
		{
			return Platform == UnrealTargetPlatform.Win64;
		}

		public ITargetDevice CreateDevice(string InRef, string InCachePath, string InParam = null)
		{
			return new TargetDeviceWindows(InRef, InCachePath);
		}
	}
}