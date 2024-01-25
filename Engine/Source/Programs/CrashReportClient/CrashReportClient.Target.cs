// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms("Win64", "Mac", "Linux", "LinuxArm64")]
[SupportedConfigurations(UnrealTargetConfiguration.Debug, UnrealTargetConfiguration.Development, UnrealTargetConfiguration.Shipping)]
public class CrashReportClientTarget : TargetRules
{
	[ConfigFile(ConfigHierarchyType.Engine, "CrashReportClientBuildSettings", "DataRouterFallback")]
	public string DataRouterFallback;
		
	[ConfigFile(ConfigHierarchyType.Engine, "CrashReportClientBuildSettings", "CompanyName")]
	public string CompanyName;
	
	[ConfigFile(ConfigHierarchyType.Engine, "CrashReportClientBuildSettings", "TelemetryUrl")]
	public string TelemetryUrl;

	[ConfigFile(ConfigHierarchyType.Engine, "CrashReportClientBuildSettings", "TelemetryKey_Dev")]
	public string TelemetryKey_Dev;

	[ConfigFile(ConfigHierarchyType.Engine, "CrashReportClientBuildSettings", "TelemetryKey_Release")]
	public string TelemetryKey_Release;
	
	public CrashReportClientTarget(TargetInfo Target) : this(Target, true)
	{}

	protected CrashReportClientTarget(TargetInfo Target, bool bSetConfiguredDefinitions) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		UndecoratedConfiguration = UnrealTargetConfiguration.Shipping;

		LaunchModuleName = "CrashReportClient";

		if (bBuildEditor == true && Target.Platform != UnrealTargetPlatform.Linux)
		{
			ExtraModuleNames.Add("EditorStyle");
		}

		bLegalToDistributeBinary = true;

		bBuildDeveloperTools = false;

		// CrashReportClient doesn't ever compile with the engine linked in
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = true;
		bUseLoggingInShipping = true;

		// CrashReportClient.exe has no exports, so no need to verify that a .lib and .exp file was emitted by
		// the linker.
		bHasExports = false;

		bUseChecksInShipping = true;

		// Epic Games Launcher needs to run on OS X 10.9, so CrashReportClient needs this as well
		bEnableOSX109Support = true;

		// Need to disable the bundled version of dbghelp so that CrashDebugHelper can load dbgeng.dll.
		WindowsPlatform.bUseBundledDbgHelp = false;

		// Set the maximum number of cores used
		GlobalDefinitions.Add("UE_TASKGRAPH_THREAD_LIMIT=5");

		GlobalDefinitions.Add("NOINITCRASHREPORTER=1");

		// Since we can't use virtual calls in the constructor allow, any inheriting type to opt out
		// of setting these configured definitions
		if (bSetConfiguredDefinitions)
		{
			GlobalDefinitions.AddRange(SetupConfiguredDefines(
				 DataRouterFallback, CompanyName, TelemetryUrl, TelemetryKey_Dev, TelemetryKey_Release));
		}
	}

	protected static List<string> SetupConfiguredDefines(
		string DataRouterFallback, 
		string CompanyName, 
		string TelemetryUrl, 
		string TelemetryKeyDev,
		string TelemetryKeyRelease)
	{
		var Definitions = new List<string>();
		if (!string.IsNullOrEmpty(DataRouterFallback))
		{
			Definitions.Add($"CRC_DATAROUTER_FALLBACK=\"{DataRouterFallback}\"");
		}

		if (!string.IsNullOrEmpty(CompanyName))
		{
			Definitions.Add($"CRC_COMPANY_NAME_FALLBACK=\"{CompanyName}\"");
		}

		if(!string.IsNullOrWhiteSpace(TelemetryUrl))
		{
			Definitions.Add($"CRC_TELEMETRY_URL=\"{TelemetryUrl}\"");
		}

		if (!string.IsNullOrWhiteSpace(TelemetryKeyDev))
		{
			Definitions.Add($"CRC_TELEMETRY_KEY_DEV=\"{TelemetryKeyDev}\"");
		}

		if (!string.IsNullOrWhiteSpace(TelemetryKeyRelease))
		{
			Definitions.Add($"CRC_TELEMETRY_KEY_RELEASE=\"{TelemetryKeyRelease}\"");
		}

		return Definitions;
	}
}
