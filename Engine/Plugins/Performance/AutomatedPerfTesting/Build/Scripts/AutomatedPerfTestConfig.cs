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
		/// If we're running on the build machine
		/// </summary>
		[AutoParamWithNames(false, "AutomatedPerfTest.IsBuildMachine")]
		public bool IsBuildMachine;
		
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
		public string TraceChannels = "default,screenshot";
		
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
		/// Which platform we're testing on 
		/// </summary>
		[AutoParamWithNames("", "AutomatedPerfTest.TestPlatform")]
		public string TestPlatform;
		
		/// <summary>
		/// Let BuildGraph tell us where we should output the Insights trace after running a test so that we know where to grab it from when we're done
		/// </summary>
		[AutoParamWithNames(false, "AutomatedPerfTest.DoVideoCapture")]
		public bool DoVideoCapture;
	}

	public class AutomatedSequencePerfTestConfig : AutomatedPerfTestConfigBase
	{
		/// <summary>
		/// Which map to run the test on
		/// </summary>
		[AutoParamWithNames("", "AutomatedPerfTest.TestMap")]
		public string TestMap;
		
		/// <summary>
		/// Which sequence to run the test with
		/// </summary>
		[AutoParamWithNames("", "AutomatedPerfTest.SequencePath")]
		public string SequencePath;
	}
}
