// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

namespace Gauntlet.SelfTest
{
	class TestUnrealInstallThenRunMac : TestUnrealInstallAndRunBase<TargetDeviceMac>
	{
		public TestUnrealInstallThenRunMac()
		{
			Platform = UnrealTargetPlatform.Mac;
		}

		protected override bool PerformFullDeviceClean()
		{
			return true;
		}

		protected override bool TestClearSavedDirectory()
		{
			return true;
		}

		protected override bool TestInstallApplication(out IAppInstall Install)
		{
			Install = null;
			return true;
		}

		protected override bool TestCopyFilesToCopyToDevice()
		{
			return true;
		}
	}
}