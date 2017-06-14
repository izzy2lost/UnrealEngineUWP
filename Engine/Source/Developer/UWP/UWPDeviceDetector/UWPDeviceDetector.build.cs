using UnrealBuildTool;

public class UWPDeviceDetector : ModuleRules
{
	public UWPDeviceDetector(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"TargetPlatform",
				"UWPTargetPlatform",
				"DesktopPlatform",
				"HTTP",
			}
		);

		if (UEBuildConfiguration.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
			PrivateIncludePathModuleNames.Add("TextureCompressor");
		}

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/UWP/UWPTargetPlatform/Private"
			}
		);

		if (WindowsPlatform.bUseWindowsSDK10)
		{
			bEnableWinRTComponentExtensions = true;
			bEnableExceptions = true;
			PCHUsage = PCHUsageMode.NoSharedPCHs;
			PrivatePCHHeaderFile = "Public/IUWPDeviceDetectorModule.h";
		}
		else
		{
			Definitions.Add("UWPTARGETPLATFORM_NULLDEVICEDETECTOR=1");
		}
	}
}