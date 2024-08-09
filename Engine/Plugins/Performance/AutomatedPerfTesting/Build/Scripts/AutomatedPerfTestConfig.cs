// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using EpicGame;
using Gauntlet;

namespace AutomatedPerfTest
{
	public class AutomatedPerfTestConfigBase : UnrealTestConfiguration
	{
		/// <summary>
		/// Name of the project (TODO: parse from the build metadata? What about github builds?)
		/// </summary>
		[AutoParamWithNames("AutomatedPerfTest.DataSourceName")]
		public string DataSourceName = "";

		/// <summary>
		/// Name of the test, useful for identifying it later
		/// </summary>
		[AutoParamWithNames("AutomatedPerfTest.TestName")]
		public string TestName = "";
		
		/// <summary>
		/// If set, will prepend platform name and use this device profile instead of the default
		/// </summary>
		[AutoParamWithNames("AutomatedPerfTest.DeviceProfileOverride")]
		public string DeviceProfileOverride = "";
		
		/// <summary>
		/// If we're running on the build machine
		/// </summary>
		[AutoParamWithNames(false, "AutomatedPerfTest.DoInsightsTrace")]
		public bool DoInsightsTrace;
		
		/// <summary>
		/// Which trace channels to test with
		/// </summary>
		[AutoParamWithNames("AutomatedPerfTest.TraceChannels")]
		public string TraceChannels = "default,screenshot,stats";
		
		/// <summary>
		/// If we're running on the build machine
		/// </summary>
		[AutoParamWithNames(false, "AutomatedPerfTest.DoCSVProfiler")]
		public bool DoCSVProfiler;
		
		/// <summary>
		/// If we're running on the build machine
		/// </summary>
		[AutoParamWithNames(false, "AutomatedPerfTest.DoFPSChart")]
		public bool DoFPSChart;
		
		/// <summary>
		/// Let BuildGraph tell us where we should output the Insights trace after running a test so that we know where to grab it from when we're done
		/// </summary>
		[AutoParamWithNames("", "AutomatedPerfTest.PerfCacheRoot")]
		public string PerfCacheRoot;

		/// <summary>
		/// Path to a JSON file with ignored issues (ensures, warnings, errros). Can be used to suppress hard-to-fix issues, on a per-branch basis
		/// </summary>
		[AutoParamWithNames("", "AutomatedPerfTest.IgnoredIssuesConfigAbsPath")]
		public string IgnoredIssuesConfigAbsPath;
		
		/// <summary>
		/// If we should trigger a video capture
		/// </summary>
		[AutoParamWithNames(false, "AutomatedPerfTest.DoVideoCapture")]
		public bool DoVideoCapture;
		
		/// <summary>
		/// If true, will look for the shipping configuration of Unreal Insights in order to parse Insights trace files
		/// This should be True unless you need to test an issue in parsing the Insights file itself.
		/// </summary>
		[AutoParamWithNames(true, "AutomatedPerfTest.UseShippingInsights")]
		public bool UseShippingInsights;
	}

	public class AutomatedSequencePerfTestConfig : AutomatedPerfTestConfigBase
	{
		/// <summary>
		/// Which map to run the test on
		/// </summary>
		[AutoParamWithNames("", "AutomatedPerfTest.SequencePerfTest.MapSequenceName")]
		public string MapSequenceComboName;
	}
}
