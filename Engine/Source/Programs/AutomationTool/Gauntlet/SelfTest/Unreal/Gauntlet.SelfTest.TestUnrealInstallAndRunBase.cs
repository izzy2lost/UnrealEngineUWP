// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Linq;
using UnrealBuildTool;

namespace Gauntlet.SelfTest
{
	/// <summary>
	/// Base class that provides a utility function for running a config and checking that it was plausibly successful
	/// </summary>
	abstract class TestUnrealInstallAndRunBase<TTargetDevice> : TestUnrealBase
		where TTargetDevice : ITargetDevice
	{
		[AutoParam(UnrealTargetRole.Client)]
		public UnrealTargetRole TargetRole { get; set; }

		[AutoParamWithNames(UnrealTargetConfiguration.Test, "TargetConfiguration", "Configuration", "Config")]
		public UnrealTargetConfiguration TargetConfiguration { get; set; }

		[AutoParamWithNames("default", new[] { "Device" })]
		public string DeviceIP { get; set; }

		[AutoParamWithNames(false, "SkipClean", "SkipInstall", "SkipCopy", "SkipDeploy")]
		public bool SkipClean { get; set; }

		protected TTargetDevice TargetDevice;
		protected UnrealTargetPlatform Platform;
		protected UnrealTestConfiguration Options;
		protected UnrealAppConfig AppConfig;

		private IAppInstance AppInstance;
		private DateTime AppStartTime;

		public TestUnrealInstallAndRunBase()
		{
			AutoParam.ApplyParamsAndDefaults(this, Gauntlet.Globals.Params.AllArguments);
			ProjectName = ProjectFile.Directory.GetDirectoryName();
			Options = new UnrealTestConfiguration();
		}

		public override bool StartTest(int Pass, int NumPasses)
		{
			// Construct all test dependencies
			// Build
			UnrealBuildSource Build = new UnrealBuildSource(ProjectName, ProjectFile, UnrealPath, UsesSharedBuildType, BuildPath);
			if (!CheckResult(Build.GetBuildCount(Platform) > 0, "Selected build was invalid"))
			{
				return false;
			}

			// Device
			IDeviceFactory Factory = Utils.InterfaceHelpers.FindImplementations<IDeviceFactory>()
					.Where(F => F.CanSupportPlatform(Platform))
					.FirstOrDefault();
			TargetDevice = (TTargetDevice)Factory.CreateDevice(DeviceIP, GetCleanCacheDirectory());

			// Role
			UnrealSessionRole Role = new UnrealSessionRole(TargetRole, Platform, TargetConfiguration, Options);
			Log.Info("Running test for {Role}", Role);

			// App Config
			AppConfig = Build.CreateConfiguration(Role);
			if (!CheckResult(AppConfig != null, "Could not create config for {Config} {Role} with platform {Platform} from build.", TargetConfiguration, TargetRole, TargetDevice.Platform))
			{
				return false;
			}

			// Wipe the kit
			if (!CheckResult(SkipClean || PerformFullDeviceClean(), "Could not fully clean device {Device}", TargetDevice))
			{
				return false;
			}

			// We now have a blank slate, test the install process!
			if (!CheckResult(TestClearSavedDirectory(), "Failed to clean device cache for device {Device}", TargetDevice))
			{
				return false;
			}

			IAppInstall AppInstall;
			if (!CheckResult(TestInstallApplication(out AppInstall), "Could not create AppInstall for {Config} {Role} with platform {Platform} from build.", TargetConfiguration, TargetRole, TargetDevice.Platform))
			{
				return false;
			}

			// InstallApplication populates the directory mappings, the ensure copy additional files has proper handling for empty mappings, clear them out now
			TargetDevice.GetPlatformDirectoryMappings().Clear();
			if (!CheckResult(TestCopyFilesToCopyToDevice(), "Could not copy {0} additional files to device {1}", AppConfig.FilesToCopy.Count, TargetDevice))
			{
				return false;
			}

			// Run the app and wait for either a timeout or it to exit
			AppStartTime = DateTime.Now;
			AppInstance = AppInstall.Run();
			return base.StartTest(Pass, NumPasses);
		}
		public override void TickTest()
		{
			// Run for 60 seconds
			if ((DateTime.Now - AppStartTime).TotalSeconds < 60)
			{
				return;
			}

			// Check the app didn't exit unexpectedly
			CheckResult(!AppInstance.HasExited, "Failed to run {Config} {Role} with platform {Device}", TargetConfiguration, TargetRole, TargetDevice.Platform);

			// Now kill it
			AppInstance.Kill();

			// Check that it left behind some artifacts (minimum should be a log)
			int ArtifactCount = new DirectoryInfo(AppInstance.ArtifactPath).GetFiles("*", SearchOption.AllDirectories).Length;
			CheckResult(ArtifactCount > 0, "No artifacts on device!");
			MarkComplete();
		}

		// Platforms should completely wipe the kit with this function to ensure a clean environment.
		protected virtual bool PerformFullDeviceClean() { return true; }

		// Should create a file, store the file onto the kit, and clear the device cache.
		protected abstract bool TestClearSavedDirectory();

		// Should install the app and return an IAppInstall
		protected abstract bool TestInstallApplication(out IAppInstall Install);

		// Should add a file to the AppConfig, copy it over, and then verify it exists in the target diretory
		protected abstract bool TestCopyFilesToCopyToDevice();

		protected FileInfo CreateDummyFile()
		{
			FileInfo DummyFile = new FileInfo(Path.GetTempFileName());
			File.WriteAllText(DummyFile.FullName, "Foo");
			return DummyFile;
		}

		protected UnrealFileToCopy CreateDummyUnrealFileToCopy()
		{
			FileInfo DummyFile = CreateDummyFile();
			UnrealFileToCopy FileToCopy = new UnrealFileToCopy(DummyFile.FullName, EIntendedBaseCopyDirectory.Saved, DummyFile.Name);
			return FileToCopy;
		}

		private string GetCleanCacheDirectory()
		{
			DirectoryInfo TempDir = new DirectoryInfo(Gauntlet.Globals.TempDir);
			if (TempDir.Exists)
			{
				TempDir.Delete(true);
			}
			TempDir.Create();

			DirectoryInfo CacheDirectory = new DirectoryInfo(Path.Combine(TempDir.FullName, "DeviceCache", Platform.ToString(), "TestInstallRun"));
			if (CacheDirectory.Exists)
			{
				CacheDirectory.Delete(true);
			}
			CacheDirectory.Create();

			return CacheDirectory.FullName;
		}
	}
}