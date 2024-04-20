// Copyright Epic Games, Inc. All Rights Reserved.

//#define ENABLE_PUBLIC_DEBUG_CONTROLLER
#define ENABLE_SECURE_DEBUG_CONTROLLER

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Web;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Compute;
using EpicGames.Horde.Jobs;
using EpicGames.Horde.Logs;
using Google.Protobuf;
using Horde.Common.Rpc;
using Horde.Server.Agents.Relay;
using Horde.Server.Configuration;
using Horde.Server.Jobs;
using Horde.Server.Jobs.Graphs;
using Horde.Server.Logs;
using Horde.Server.Projects;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using JetBrains.Profiler.SelfApi;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using MongoDB.Bson;
using MongoDB.Driver;

namespace Horde.Server.Server
{
#if ENABLE_PUBLIC_DEBUG_CONTROLLER
	/// <summary>
	/// Public endpoints for the debug controller
	/// </summary>
	[ApiController]
	public class PublicDebugController : ControllerBase
	{
		/// <summary>
		/// The connection tracker service singleton
		/// </summary>
		RequestTrackerService RequestTrackerService;

		IHostApplicationLifetime ApplicationLifetime;

		IDogStatsd DogStatsd;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="RequestTrackerService"></param>
		/// <param name="ApplicationLifetime"></param>
		/// <param name="DogStatsd"></param>
		public PublicDebugController(RequestTrackerService RequestTrackerService, IHostApplicationLifetime ApplicationLifetime, IDogStatsd DogStatsd)
		{
			RequestTrackerService = RequestTrackerService;
			ApplicationLifetime = ApplicationLifetime;
			DogStatsd = DogStatsd;
		}

		/// <summary>
		/// Prints all the headers for the incoming request
		/// </summary>
		/// <returns>Http result</returns>
		[HttpGet]
		[Route("/api/v1/debug/headers")]
		public IActionResult GetRequestHeaders()
		{
			StringBuilder Content = new StringBuilder();
			Content.AppendLine("<html><body><pre>");
			foreach (KeyValuePair<string, StringValues> Pair in HttpContext.Request.Headers)
			{
				foreach (string Value in Pair.Value)
				{
					Content.AppendLine(HttpUtility.HtmlEncode($"{Pair.Key}: {Value}"));
				}
			}
			Content.Append("</pre></body></html>");
			return new ContentResult { ContentType = "text/html", StatusCode = (int)HttpStatusCode.OK, Content = Content.ToString() };
		}

		/// <summary>
		/// Waits specified number of milliseconds and then returns a response
		/// Used for testing timeouts proxy settings.
		/// </summary>
		/// <returns>Http result</returns>
		[HttpGet]
		[Route("/api/v1/debug/wait")]
		public async Task<ActionResult> GetAndWait([FromQuery] int WaitTimeMs = 1000)
		{
			await Task.Delay(WaitTimeMs);
			string Content = $"Waited {WaitTimeMs} ms. " + new Random().Next(0, 10000000);
			return new ContentResult { ContentType = "text/plain", StatusCode = (int)HttpStatusCode.OK, Content = Content };
		}

		/// <summary>
		/// Waits specified number of milliseconds and then throws an exception
		/// Used for testing graceful shutdown and interruption of outstanding requests.
		/// </summary>
		/// <returns>Http result</returns>
		[HttpGet]
		[Route("/api/v1/debug/exception")]
		public async Task<ActionResult> ThrowException([FromQuery] int WaitTimeMs = 0)
		{
			await Task.Delay(WaitTimeMs);
			throw new Exception("Test exception triggered by debug controller!");
		}

		/// <summary>
		/// Trigger an increment of a DogStatsd metric
		/// </summary>
		/// <returns>Http result</returns>
		[HttpGet]
		[Route("/api/v1/debug/metric")]
		public ActionResult TriggerMetric([FromQuery] int Value = 10)
		{
			DogStatsd.Increment("hordeMetricTest", Value);
			return Ok("Incremented metric 'hordeMetricTest' Type: " + DogStatsd.GetType());
		}

		/// <summary>
		/// Display metrics related to the .NET runtime
		/// </summary>
		/// <returns>Http result</returns>
		[HttpGet]
		[Route("/api/v1/debug/dotnet-metrics")]
		public ActionResult DotNetMetrics()
		{
			ThreadPool.GetMaxThreads(out int MaxWorkerThreads, out int MaxIoThreads);
			ThreadPool.GetAvailableThreads(out int FreeWorkerThreads, out int FreeIoThreads);
			ThreadPool.GetMinThreads(out int MinWorkerThreads, out int MinIoThreads);

			int BusyIoThreads = MaxIoThreads - FreeIoThreads;
			int BusyWorkerThreads = MaxWorkerThreads - FreeWorkerThreads;

			StringBuilder Content = new StringBuilder();
			Content.AppendLine("Threads:");
			Content.AppendLine("-------------------------------------------------------------");
			Content.AppendLine("Worker busy={0,-5} free={1,-5} min={2,-5} max={3,-5}", BusyWorkerThreads, FreeWorkerThreads, MinWorkerThreads, MaxWorkerThreads);
			Content.AppendLine("  IOCP busy={0,-5} free={1,-5} min={2,-5} max={3,-5}", BusyIoThreads, FreeIoThreads, MinIoThreads, MaxWorkerThreads);


			NumberFormatInfo Nfi = (NumberFormatInfo)CultureInfo.InvariantCulture.NumberFormat.Clone();
			Nfi.NumberGroupSeparator = " ";

			string FormatBytes(long Number)
			{
				return (Number / 1024 / 1024).ToString("#,0", Nfi) + " MB";
			}

			GCMemoryInfo GcMemoryInfo = GC.GetGCMemoryInfo();
			Content.AppendLine("");
			Content.AppendLine("");
			Content.AppendLine("Garbage collection (GC):");
			Content.AppendLine("-------------------------------------------------------------");
			Content.AppendLine("              Latency mode: " + GCSettings.LatencyMode);
			Content.AppendLine("              Is server GC: " + GCSettings.IsServerGC);
			Content.AppendLine("              Total memory: " + FormatBytes(GC.GetTotalMemory(false)));
			Content.AppendLine("           Total allocated: " + FormatBytes(GC.GetTotalAllocatedBytes(false)));
			Content.AppendLine("                 Heap size: " + FormatBytes(GcMemoryInfo.HeapSizeBytes));
			Content.AppendLine("                Fragmented: " + FormatBytes(GcMemoryInfo.FragmentedBytes));
			Content.AppendLine("               Memory Load: " + FormatBytes(GcMemoryInfo.MemoryLoadBytes));
			Content.AppendLine("    Total available memory: " + FormatBytes(GcMemoryInfo.TotalAvailableMemoryBytes));
			Content.AppendLine("High memory load threshold: " + FormatBytes(GcMemoryInfo.HighMemoryLoadThresholdBytes));

			return Ok(Content.ToString());
		}

		/// <summary>
		/// Force a full GC of all generations
		/// </summary>
		/// <returns>Prints time taken in ms</returns>
		[HttpGet]
		[Route("/api/v1/debug/force-gc")]
		public ActionResult ForceTriggerGc()
		{
			Stopwatch Timer = new Stopwatch();
			Timer.Start();
			GC.Collect();
			Timer.Stop();
			return Ok($"Time taken: {Timer.Elapsed.TotalMilliseconds} ms");
		}

		/// <summary>
		/// Lists requests in progress
		/// </summary>
		/// <returns>HTML result</returns>
		[HttpGet]
		[Route("/api/v1/debug/requests-in-progress")]
		public ActionResult GetRequestsInProgress()
		{
			StringBuilder Content = new StringBuilder();
			Content.AppendLine("<html><body>");
			Content.AppendLine("<h1>Requests in progress</h1>");
			Content.AppendLine("<table border=\"1\">");
			Content.AppendLine("<tr>");
			Content.AppendLine("<th>Request Trace ID</th>");
			Content.AppendLine("<th>Path</th>");
			Content.AppendLine("<th>Started At</th>");
			Content.AppendLine("<th>Age</th>");
			Content.AppendLine("</tr>");

			List<KeyValuePair<string, TrackedRequest>> Requests = RequestTrackerService.GetRequestsInProgress().ToList();
			Requests.Sort((A, B) => A.Value.StartedAt.CompareTo(B.Value.StartedAt));

			foreach (KeyValuePair<string, TrackedRequest> Entry in Requests)
			{
				Content.Append("<tr>");
				Content.AppendLine($"<td>{Entry.Key}</td>");
				Content.AppendLine($"<td>{Entry.Value.Request.Path}</td>");
				Content.AppendLine($"<td>{Entry.Value.StartedAt}</td>");
				Content.AppendLine($"<td>{Entry.Value.GetTimeSinceStartInMs()} ms</td>");
				Content.Append("</tr>");
			}
			Content.Append("</table>\n</body>\n</html>");

			return new ContentResult { ContentType = "text/html", StatusCode = (int)HttpStatusCode.OK, Content = Content.ToString() };
		}

		/*
		// Used during development only
		[HttpGet]
		[Route("/api/v1/debug/stop")]
		public ActionResult StopApp()
		{
			Task.Run(async () =>
			{
				await Task.Delay(100);
				ApplicationLifetime.StopApplication();
			});
			
			return new ContentResult { ContentType = "text/plain", StatusCode = (int)HttpStatusCode.OK, Content = "App stopping..." };
		}
		/**/
	}
#endif
#if ENABLE_SECURE_DEBUG_CONTROLLER
	/// <summary>
	/// Controller managing account status
	/// </summary>
	[ApiController]
	[Authorize]
	[Tags("Debug")]
	public class SecureDebugController : HordeControllerBase
	{
		private static readonly Random s_random = new();

		private readonly MongoService _mongoService;
		private readonly ConfigService _configService;
		private readonly AgentRelayService _agentRelayService;
		private readonly JobService _jobService;
		private readonly JobTaskSource _jobTaskSource;
		private readonly ILogCollection _logCollection;
		private readonly IOptionsSnapshot<GlobalConfig> _globalConfig;
		private readonly ILogger<SecureDebugController> _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public SecureDebugController(
			MongoService mongoService,
			ConfigService configService,
			AgentRelayService agentRelayService,
			JobService jobService,
			JobTaskSource jobTaskSource,
			ILogCollection logCollection, IOptionsSnapshot<GlobalConfig> globalConfig, ILogger<SecureDebugController> logger)
		{
			_mongoService = mongoService;
			_configService = configService;
			_jobService = jobService;
			_agentRelayService = agentRelayService;
			_jobTaskSource = jobTaskSource;
			_logCollection = logCollection;
			_globalConfig = globalConfig;
			_logger = logger;
		}

		/// <summary>
		/// Prints all the environment variables
		/// </summary>
		/// <returns>Http result</returns>
		[HttpGet]
		[Route("/api/v1/debug/environment")]
		public ActionResult GetServerEnvVars()
		{
			if (!_globalConfig.Value.Authorize(ServerAclAction.Debug, User))
			{
				return Forbid(ServerAclAction.Debug);
			}

			StringBuilder content = new StringBuilder();
			content.AppendLine("<html><body><pre>");
			foreach (System.Collections.DictionaryEntry? pair in System.Environment.GetEnvironmentVariables())
			{
				if (pair != null)
				{
					content.AppendLine(HttpUtility.HtmlEncode($"{pair.Value.Key}={pair.Value.Value}"));
				}
			}
			content.Append("</pre></body></html>");
			return new ContentResult { ContentType = "text/html", StatusCode = (int)HttpStatusCode.OK, Content = content.ToString() };
		}

		/// <summary>
		/// Returns diagnostic information about the current state of the queue
		/// </summary>
		/// <returns>Information about the queue</returns>
		[HttpGet]
		[Route("/api/v1/debug/queue")]
		public ActionResult<object> GetQueueStatus()
		{
			if (!_globalConfig.Value.Authorize(ServerAclAction.Debug, User))
			{
				return Forbid(ServerAclAction.Debug);
			}

			return _jobTaskSource.GetStatus();
		}

		/// <summary>
		/// Converts all legacy pools into config entries
		/// </summary>
		[HttpGet]
		[Route("/api/v1/debug/aclscopes")]
		public ActionResult<object> GetAclScopes()
		{
			if (!_globalConfig.Value.Authorize(ServerAclAction.Debug, User))
			{
				return Forbid(ServerAclAction.Debug);
			}

			return new { scopes = _globalConfig.Value.AclScopes.Keys.ToList() };
		}

		/// <summary>
		/// Returns the fully parsed config object.
		/// </summary>
		[HttpGet]
		[Route("/api/v1/debug/appsettings")]
		public ActionResult<object> GetAppSettings()
		{
			if (!_globalConfig.Value.Authorize(ServerAclAction.Debug, User))
			{
				return Forbid(ServerAclAction.Debug);
			}

			return _globalConfig.Value.ServerSettings;
		}

		/// <summary>
		/// Returns the fully parsed config object.
		/// </summary>
		[HttpGet]
		[Route("/api/v1/debug/config")]
		public ActionResult<object> GetConfig()
		{
			if (!_globalConfig.Value.Authorize(ServerAclAction.Debug, User))
			{
				return Forbid(ServerAclAction.Debug);
			}

			// Duplicate the config, so we can redact stuff that we don't want to return through the browser
			byte[] data = _configService.Serialize(_globalConfig.Value);
			GlobalConfig config = _configService.Deserialize(data)!;

			foreach (ProjectConfig project in config.Projects)
			{
				project.Logo = null;
			}

			return config;
		}

		/// <summary>
		/// Get the network ID for a given IP address
		/// </summary>
		[HttpGet]
		[Route("/api/v1/debug/network-id")]
		public ActionResult<object> GetNetworkId([FromQuery] string? ipAddress = null)
		{
			if (!_globalConfig.Value.Authorize(ServerAclAction.Debug, User))
			{
				return Forbid(ServerAclAction.Debug);
			}

			if (ipAddress == null || !IPAddress.TryParse(ipAddress, out IPAddress? ip))
			{
				return BadRequest("Unable to read or convert query parameter 'ipAddress'");
			}

			_globalConfig.Value.TryGetNetworkConfig(ip, out NetworkConfig? networkConfig);
			return networkConfig == null ? StatusCode(StatusCodes.Status500InternalServerError, "Unable to find a network config for the IP") : Ok(networkConfig);
		}

		/// <summary>
		/// Add a port mapping for agent relay
		/// </summary>
		[HttpGet]
		[Route("/api/v1/debug/relay/add-port")]
		public async Task<ActionResult<object>> AddPortMappingAsync([FromQuery] string? clientIpStr = null, [FromQuery] string? agentIpStr = null, [FromQuery] int? agentPort = null)
		{
			if (!_globalConfig.Value.Authorize(ServerAclAction.Debug, User))
			{
				return Forbid(ServerAclAction.Debug);
			}

			if (clientIpStr == null || !IPAddress.TryParse(clientIpStr, out IPAddress? clientIp))
			{
				return BadRequest("Unable to read or convert query parameter 'clientIp'");
			}

			if (agentIpStr == null || !IPAddress.TryParse(agentIpStr, out IPAddress? agentIp))
			{
				return BadRequest("Unable to read or convert query parameter 'agentIp'");
			}

			if (agentPort == null)
			{
				return BadRequest("Bad query parameter 'agentPort'");
			}

			string bogusLeaseId = ObjectId.GenerateNewId().ToString();
			List<Port> ports = new()
			{
				new Port { RelayPort = -1, AgentPort = agentPort.Value, Protocol = PortProtocol.Tcp }
			};

			PortMapping portMapping = await _agentRelayService.AddPortMappingAsync(new ClusterId("default"), LeaseId.Parse(bogusLeaseId), clientIp, agentIp, ports);
			return JsonFormatter.Default.Format(portMapping);
		}

		/// <summary>
		/// Generate log message of varying size
		/// </summary>
		/// <returns>Information about the log message generated</returns>
		[HttpGet]
		[Route("/api/v1/debug/generate-log-msg")]
		public ActionResult GenerateLogMessage(
			[FromQuery] string? logLevel = null,
			[FromQuery] int messageLen = 0,
			[FromQuery] int exceptionMessageLen = 0,
			[FromQuery] int argCount = 0,
			[FromQuery] int argLen = 10)
		{
			if (!_globalConfig.Value.Authorize(ServerAclAction.Debug, User))
			{
				return Forbid(ServerAclAction.Debug);
			}

			string RandomString(int length)
			{
				const string Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
				return new string(Enumerable.Repeat(Chars, length).Select(s => s[s_random.Next(s.Length)]).ToArray());
			}

			if (!Enum.TryParse(logLevel, out LogLevel logLevelInternal))
			{
				logLevelInternal = LogLevel.Information;
			}

			Exception? exception = null;
			string message = "Message generated by /api/v1/debug/generate-log-msg";
			message += RandomString(messageLen);

			if (exceptionMessageLen > 0)
			{
				exception = new Exception("Exception from /api/v1/debug/generate-log-msg " + RandomString(exceptionMessageLen));
			}

			Dictionary<string, object> args = new();
			if (argCount > 0)
			{
				for (int i = 0; i < argCount; i++)
				{
					args["Arg" + i] = "Arg 1 - " + RandomString(argLen);
				}
			}

			using IDisposable? logScope = _logger.BeginScope(args);

			// Ignore warning as we explicitly want to build this message manually
#pragma warning disable CA2254 // Template should be a static expression
			_logger.Log(logLevelInternal, exception, message);
#pragma warning restore CA2254

			return Ok($"Log message generated logLevel={logLevelInternal} messageLen={messageLen} exceptionMessageLen={exceptionMessageLen} argCount={argCount} argLen={argLen}");
		}

		/// <summary>
		/// Retrieve metadata about a specific log file
		/// </summary>
		/// <param name="logId">Id of the log file to get information about</param>
		/// <param name="filter">Filter for the properties to return</param>
		/// <returns>Information about the requested project</returns>
		[HttpGet]
		[Route("/api/v1/debug/logs/{logId}")]
		public async Task<ActionResult<object>> GetLogAsync(LogId logId, [FromQuery] PropertyFilter? filter = null)
		{
			if (!_globalConfig.Value.Authorize(ServerAclAction.Debug, User))
			{
				return Forbid(ServerAclAction.Debug);
			}

			ILog? log = await _logCollection.GetAsync(logId, CancellationToken.None);
			if (log == null)
			{
				return NotFound();
			}

			return log.ApplyFilter(filter);
		}

		/// <summary>
		/// Display a table listing each template with what job options are enabled
		/// </summary>
		/// <returns>Async task</returns>
		[HttpGet]
		[Route("/api/v1/debug/job-options")]
		public ActionResult GetJobOptions([FromQuery] string? format = "html")
		{
			if (!_globalConfig.Value.Authorize(ServerAclAction.Debug, User))
			{
				return Forbid(ServerAclAction.Debug);
			}

			List<PropertyInfo> joProps = typeof(JobOptions).GetProperties(BindingFlags.Public | BindingFlags.Instance).OrderBy(x => x.Name).ToList();

			if (format == "csv")
			{
				return GetJobOptionsAsCsv(joProps);
			}

			StringBuilder sb = new();

			sb.AppendLine("<style>");
			sb.AppendLine("body { font-family: 'Helvetica Neue', Helvetica, Arial, sans-serif; }");
			sb.AppendLine("table { border-collapse: collapse; width: 100%; font-size: 12px; }");
			sb.AppendLine("th, td { border: 1px solid black; text-align: left; padding: 8px; }");
			sb.AppendLine("th { background-color: #f2f2f2; }");
			sb.AppendLine("</style>");

			sb.AppendLine("<h1>Job options enabled by stream + template</h1>");
			sb.AppendLine("<table>");
			sb.AppendLine("<thead><tr>");
			sb.Append("<th>Stream</th>");
			sb.Append("<th>Template</th>");
			foreach (PropertyInfo prop in joProps)
			{
				sb.Append($"<th>{prop.Name}</th>");
			}
			sb.AppendLine("</tr></thead>");

			foreach (StreamConfig sc in _globalConfig.Value.Streams)
			{
				foreach (TemplateRefConfig tpl in sc.Templates)
				{
					sb.AppendLine("<tr>");
					sb.Append($"<td>{sc.Id}</td>");
					sb.Append($"<td>{tpl.Id}</td>");
					foreach (PropertyInfo prop in joProps)
					{
						sb.Append($"<td>{prop.GetValue(tpl.JobOptions)}</td>");
					}
					sb.AppendLine("</tr>");
				}
			}

			sb.AppendLine("</table>");
			return new ContentResult { ContentType = "text/html", StatusCode = (int)HttpStatusCode.OK, Content = sb.ToString() };
		}

		private ActionResult GetJobOptionsAsCsv(List<PropertyInfo> jobOptionsProps)
		{
			StringBuilder sb = new();

			List<string> headers = new() { "Stream", "Template" };
			headers.AddRange(jobOptionsProps.Select(prop => prop.Name));
			sb.AppendLine(String.Join('\t', headers));

			foreach (StreamConfig sc in _globalConfig.Value.Streams)
			{
				foreach (TemplateRefConfig tpl in sc.Templates)
				{
					List<string> row = new() { sc.Id.ToString(), tpl.Id.ToString() };
					row.AddRange(jobOptionsProps.Select(prop => prop.GetValue(tpl.JobOptions)?.ToString() ?? ""));
					sb.AppendLine(String.Join('\t', row));
				}
			}

			return new ContentResult { ContentType = "text/csv", StatusCode = (int)HttpStatusCode.OK, Content = sb.ToString() };
		}

		record JobTiming(
			string StreamId,
			string TemplateId,
			string Name,
			IReadOnlySet<string> StepNames,
			TimeSpan BatchSetupDuration,
			TimeSpan BatchWorkDuration,
			TimeSpan BatchTeardownDuration)
		{
			public JobTiming Merge(JobTiming jt)
			{
				if (StreamId != jt.StreamId || TemplateId != jt.TemplateId)
				{
					throw new ArgumentException("StreamId or TemplateId do not match");
				}

				HashSet<string> newNames = [..StepNames.Union(jt.StepNames)];
				return new JobTiming(StreamId, TemplateId, Name, newNames, 
					BatchSetupDuration + jt.BatchSetupDuration,
					BatchWorkDuration + jt.BatchWorkDuration,
					BatchTeardownDuration + jt.BatchTeardownDuration);
			}
		}
		
		/// <summary>
		/// Display a table listing each template with job timings (setup, work and teardown durations)
		/// </summary>
		/// <returns>Async task</returns>
		[HttpGet]
		[Route("/api/v1/debug/job-timings")]
		public async Task<ActionResult> GetJobTimingsAsync(
			[FromQuery] DateTimeOffset? minCreateTime = null,
			[FromQuery] DateTimeOffset? maxCreateTime = null,
			[FromQuery] bool onlySetupBuild = false,
			[FromQuery] string? format = "html")
		{
			if (!_globalConfig.Value.Authorize(ServerAclAction.Debug, User))
			{
				return Forbid(ServerAclAction.Debug);
			}

			minCreateTime ??= DateTimeOffset.UtcNow.Subtract(TimeSpan.FromDays(1));
			IReadOnlyList<IJob> jobs = await _jobService.FindJobsAsync(minCreateTime: minCreateTime, maxCreateTime: maxCreateTime);
			List<JobTiming> allJobTimings = await CalculateJobTimingsAsync(jobs);

			if (onlySetupBuild)
			{
				allJobTimings = allJobTimings.Where(x => x.StepNames.SetEquals(["Setup Build"])).ToList();
			}
			
			IReadOnlyList<JobTiming> jobTimingsByTemplate = GroupJobTimings(allJobTimings);
			IEnumerable<JobTiming> sortedJobTimings = jobTimingsByTemplate.OrderBy(x => x.BatchSetupDuration).Reverse();

			if (format == "csv")
			{
				return GetJobTimingsAsCsv(sortedJobTimings);
			}

			StringBuilder sb = new();

			sb.AppendLine("<style>");
			sb.AppendLine("body { font-family: 'Helvetica Neue', Helvetica, Arial, sans-serif; }");
			sb.AppendLine("table { border-collapse: collapse; width: 100%; font-size: 12px; }");
			sb.AppendLine("th, td { border: 1px solid black; text-align: left; padding: 8px; }");
			sb.AppendLine("th { background-color: #f2f2f2; }");
			sb.AppendLine("</style>");

			sb.AppendLine("<h1>Job timings by template</h1>");
			sb.AppendLine("<p>Durations specified in seconds.</p>");
			sb.AppendLine("<table>");
			sb.AppendLine("<thead><tr>");
			sb.Append("<th>Stream</th>");
			sb.Append("<th>Template</th>");
			sb.Append("<th>Name</th>");
			sb.Append("<th>Batch Setup</th>");
			sb.Append("<th>Batch Work</th>");
			sb.Append("<th>Batch Teardown</th>");
			sb.Append("<th>Steps</th>");
			sb.AppendLine("</tr></thead>");

			foreach (JobTiming jt in sortedJobTimings)
			{
				sb.AppendLine("<tr>");
				sb.Append($"<td>{jt.StreamId}</td>");
				sb.Append($"<td>{jt.TemplateId}</td>");
				sb.Append($"<td>{jt.Name}</td>");
				sb.Append($"<td>{(int)jt.BatchSetupDuration.TotalSeconds}</td>");
				sb.Append($"<td>{(int)jt.BatchWorkDuration.TotalSeconds}</td>");
				sb.Append($"<td>{(int)jt.BatchTeardownDuration.TotalSeconds}</td>");
				sb.Append($"<td>{String.Join(", ", jt.StepNames.Order())}</td>");
				sb.AppendLine("</tr>");
			}

			sb.AppendLine("</table>");
			return new ContentResult { ContentType = "text/html", StatusCode = (int)HttpStatusCode.OK, Content = sb.ToString() };
		}
		
		private static ActionResult GetJobTimingsAsCsv(IEnumerable<JobTiming> jobTimings)
		{
			StringBuilder sb = new();
			sb.AppendJoin('\t', ["Stream", "Template", "Name", "Batch Setup", "Batch Work", "Batch Teardown", "Steps"]).AppendLine();

			foreach (JobTiming jt in jobTimings)
			{
				sb.Append($"{jt.StreamId}\t");
				sb.Append($"{jt.TemplateId}\t");
				sb.Append($"{jt.Name}\t");
				sb.Append($"{(int)jt.BatchSetupDuration.TotalSeconds}\t");
				sb.Append($"{(int)jt.BatchWorkDuration.TotalSeconds}\t");
				sb.Append($"{(int)jt.BatchTeardownDuration.TotalSeconds}\t");
				sb.AppendJoin(',', jt.StepNames.Order());
				sb.AppendLine();
			}

			return new ContentResult { ContentType = "text/csv", StatusCode = (int)HttpStatusCode.OK, Content = sb.ToString() };
		}

		private async Task<List<JobTiming>> CalculateJobTimingsAsync(IEnumerable<IJob> jobs)
		{
			List<JobTiming> timings = [];
			foreach (IJob job in jobs)
			{
				IGraph graph = await _jobService.GetGraphAsync(job);
				
				foreach (IJobStepBatch batch in job.Batches)
				{
					if (batch.State != JobStepBatchState.Complete || batch.StartTimeUtc == null || batch.FinishTimeUtc == null)
					{
						continue;
					}
					DateTime firstStepStartTime = DateTime.MaxValue;
					DateTime lastStepFinishTime = DateTime.MinValue;

					HashSet<string> stepNames = [];
					foreach (IJobStep step in batch.Steps)
					{
						if (step.StartTimeUtc == null || step.FinishTimeUtc == null)
						{
							continue;
						}
						
						INode node = graph.GetNode(new NodeRef(batch.GroupIdx, step.NodeIdx));
						firstStepStartTime = step.StartTimeUtc.Value < firstStepStartTime ? step.StartTimeUtc.Value : firstStepStartTime;
						lastStepFinishTime = step.FinishTimeUtc.Value > lastStepFinishTime ? step.FinishTimeUtc.Value : lastStepFinishTime;
						stepNames.Add(node.Name);
					}

					if (firstStepStartTime == DateTime.MaxValue || lastStepFinishTime == DateTime.MinValue)
					{
						continue;
					}

					TimeSpan batchSetupDuration = firstStepStartTime - batch.StartTimeUtc.Value;
					TimeSpan batchWorkDuration = lastStepFinishTime - firstStepStartTime;
					TimeSpan batchTeardownDuration = batch.FinishTimeUtc.Value - lastStepFinishTime;
					timings.Add(new JobTiming(job.StreamId.ToString(), job.TemplateId.ToString(), job.Name, stepNames, batchSetupDuration, batchWorkDuration, batchTeardownDuration));
				}
			}

			return timings;
		}

		private static IReadOnlyList<JobTiming> GroupJobTimings(IEnumerable<JobTiming> jobTimings)
		{
			Dictionary<string, JobTiming> groupedTimings = new();
			foreach (JobTiming timing in jobTimings)
			{
				string key = $"{timing.StreamId}-{timing.TemplateId}";
				if (!groupedTimings.TryGetValue(key, out JobTiming? groupTiming))
				{
					groupedTimings[key] = timing;
				}
				else
				{
					groupedTimings[key] = groupTiming.Merge(timing);
				}
			}

			return groupedTimings.Values.ToList();
		}

		/// <summary>
		/// Populate the database with test data
		/// </summary>
		/// <returns>Async task</returns>
		[HttpGet]
		[Route("/api/v1/debug/collections/{Name}")]
		public async Task<ActionResult<object>> GetDocumentsAsync(string name, [FromQuery] string? filter = null, [FromQuery] int index = 0, [FromQuery] int count = 10)
		{
			if (!_globalConfig.Value.Authorize(ServerAclAction.Debug, User))
			{
				return Forbid(ServerAclAction.Debug);
			}

			IMongoCollection<Dictionary<string, object>> collection = _mongoService.GetCollection<Dictionary<string, object>>(name);
			List<Dictionary<string, object>> documents = await collection.Find(filter ?? "{}").Skip(index).Limit(count).ToListAsync();
			return documents;
		}

		/// <summary>
		/// Start a CPU profiler session using dotTrace
		/// Only one profiling session can run at a time.
		/// </summary>
		/// <returns>Status description</returns>
		[HttpGet]
		[Route("/api/v1/debug/profiler/cpu/start")]
		public async Task<ActionResult> StartCpuProfilerAsync()
		{
			if (!_globalConfig.Value.Authorize(ServerAclAction.Debug, User))
			{
				return Forbid(ServerAclAction.Debug);
			}

			// Downloads dotTrace executable if not available
			Stopwatch sw = Stopwatch.StartNew();
			await DotTrace.EnsurePrerequisiteAsync();
			_logger.LogInformation("dotTrace prerequisites step finished in {SetupTimeMs} ms", sw.ElapsedMilliseconds);

			string snapshotDir = Path.Join(Path.GetTempPath(), "horde-cpu-profiler-snapshots");
			if (!Directory.Exists(snapshotDir))
			{
				Directory.CreateDirectory(snapshotDir);
			}

			DotTrace.Config config = new();
			config.SaveToDir(snapshotDir);
			DotTrace.Attach(config);
			DotTrace.StartCollectingData();

			return new ContentResult { ContentType = "text/plain", StatusCode = (int)HttpStatusCode.OK, Content = "CPU profiling session started. Using dir " + snapshotDir };
		}

		/// <summary>
		/// Stops a CPU profiler session
		/// </summary>
		/// <returns>Text message</returns>
		[HttpGet]
		[Route("/api/v1/debug/profiler/cpu/stop")]
		public ActionResult StopProfiler()
		{
			if (!_globalConfig.Value.Authorize(ServerAclAction.Debug, User))
			{
				return Forbid(ServerAclAction.Debug);
			}

			DotTrace.SaveData();
			DotTrace.Detach();
			return new ContentResult { ContentType = "text/plain", StatusCode = (int)HttpStatusCode.OK, Content = "CPU profiling session stopped" };
		}

		/// <summary>
		/// Downloads the captured CPU profiling snapshots
		/// </summary>
		/// <returns>A .zip file containing the profiling snapshots</returns>
		[HttpGet]
		[Route("/api/v1/debug/profiler/cpu/download")]
		public ActionResult DownloadProfilingData()
		{
			if (!_globalConfig.Value.Authorize(ServerAclAction.Debug, User))
			{
				return Forbid(ServerAclAction.Debug);
			}

			string snapshotZipFile = DotTrace.GetCollectedSnapshotFilesArchive(false);
			if (!System.IO.File.Exists(snapshotZipFile))
			{
				return NotFound("The generated snapshot .zip file was not found");
			}

			return PhysicalFile(snapshotZipFile, "application/zip", Path.GetFileName(snapshotZipFile));
		}
		
		/// <summary>
		/// Take a memory snapshot using dotTrace
		/// </summary>
		/// <returns>A .dmw file containing the memory snapshot</returns>
		[HttpGet]
		[Route("/api/v1/debug/profiler/mem/snapshot")]
		public async Task<ActionResult> TakeMemorySnapshotAsync()
		{
			if (!_globalConfig.Value.Authorize(ServerAclAction.Debug, User))
			{
				return Forbid(ServerAclAction.Debug);
			}

			// Downloads dotMemory executable if not available
			Stopwatch sw = Stopwatch.StartNew();
			await DotMemory.EnsurePrerequisiteAsync();
			_logger.LogInformation("dotMemory prerequisites step finished in {SetupTimeMs} ms", sw.ElapsedMilliseconds);

			string snapshotDir = Path.Join(Path.GetTempPath(), "horde-mem-profiler-snapshots");
			if (!Directory.Exists(snapshotDir))
			{
				Directory.CreateDirectory(snapshotDir);
			}

			sw.Restart();
			DotMemory.Config config = new();
			config.SaveToDir(snapshotDir);
			string workspaceFilePath = DotMemory.GetSnapshotOnce(config);
			_logger.LogInformation("dotMemory snapshot captured in {CaptureTimeMs} ms", sw.ElapsedMilliseconds);
			
			if (!System.IO.File.Exists(workspaceFilePath))
			{
				return NotFound("The generated workspace file was not found");
			}

			return PhysicalFile(workspaceFilePath, "application/octet-stream", Path.GetFileName(workspaceFilePath));
		}

		/// <summary>
		/// Throws an exception to debug error handling
		/// </summary>
		/// <returns></returns>
		[HttpGet]
		[Route("/api/v1/debug/exception")]
		public ActionResult ThrowException()
		{
			if (!_globalConfig.Value.Authorize(ServerAclAction.Debug, User))
			{
				return Forbid(ServerAclAction.Debug);
			}

			int numberArg = 42;
			string stringArg = "hello";
			throw new Exception($"Message: numberArg:{numberArg}, stringArg:{stringArg}");
		}

		/// <summary>
		/// Forces an update of a job's batches to debug issues such as updating dependencies
		/// </summary>
		/// <returns></returns>
		[HttpGet]
		[Route("/api/v1/debug/batchupdate/{JobId}/{BatchId}")]
		public async Task<ActionResult> DebugBatchUpdateAsync(string jobId, string batchId)
		{
			if (!_globalConfig.Value.Authorize(ServerAclAction.Debug, User))
			{
				return Forbid(ServerAclAction.Debug);
			}

			IJob? job = await _jobService.GetJobAsync(JobId.Parse(jobId));

			if (job == null)
			{
				return NotFound();
			}

			await _jobService.TryUpdateBatchAsync(job, JobStepBatchId.Parse(batchId), newError: JobStepBatchError.None);

			return Ok();

		}
	}
}

#endif
