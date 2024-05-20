// Copyright Epic Games, Inc. All Rights Reserved.

using System.Reflection;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Logs;
using Google.Protobuf;
using Horde.Agent.Services;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;
using OpenTracing.Util;

namespace Horde.Agent.Leases.Handlers
{
	class JobHandler : LeaseHandler<ExecuteJobTask>
	{
		/// <summary>
		/// Current lease ID being executed
		/// </summary>
		public LeaseId? CurrentLeaseId { get; private set; } = null;

		/// <summary>
		/// Current job ID being executed
		/// </summary>
		public string? CurrentJobId { get; private set; } = null;

		/// <summary>
		/// Current job batch ID being executed
		/// </summary>
		public string? CurrentBatchId { get; private set; } = null;

		/// <inheritdoc/>
		public override async Task<LeaseResult> ExecuteAsync(ISession session, LeaseId leaseId, ExecuteJobTask executeTask, ILogger localLogger, CancellationToken cancellationToken)
		{
			try
			{
				CurrentLeaseId = leaseId;
				CurrentJobId = executeTask.JobId;
				CurrentBatchId = executeTask.BatchId;

				GlobalTracer.Instance.ActiveSpan?.SetTag("jobId", executeTask.JobId.ToString());
				GlobalTracer.Instance.ActiveSpan?.SetTag("jobName", executeTask.JobName.ToString());
				GlobalTracer.Instance.ActiveSpan?.SetTag("batchId", executeTask.BatchId.ToString());

				await using IServerLogger logger = session.HordeClient.CreateServerLogger(LogId.Parse(executeTask.LogId)).WithLocalLogger(localLogger);

				executeTask.JobOptions ??= new RpcJobOptions();

				List<string> arguments = new List<string>();
				arguments.Add("execute");
				arguments.Add("job");
				arguments.Add($"-AgentId={session.AgentId}");
				arguments.Add($"-SessionId={session.SessionId}");
				arguments.Add($"-LeaseId={leaseId}");
				arguments.Add($"-WorkingDir={session.WorkingDir}");
				arguments.Add($"-Task={Convert.ToBase64String(executeTask.ToByteArray())}");

				FileReference driverAssembly = FileReference.Combine(new DirectoryReference(AppContext.BaseDirectory), "JobDriver", "JobDriver.dll");

				Dictionary<string, string> environment = ManagedProcess.GetCurrentEnvVars();
				environment[HordeHttpClient.HordeUrlEnvVarName] = session.HordeClient.ServerUrl.ToString();
				environment[HordeHttpClient.HordeTokenEnvVarName] = executeTask.Token;
				environment["UE_LOG_JSON_TO_STDOUT"] = "1";

				int exitCode = await RunDotNetProcessAsync(driverAssembly, arguments, environment, false, logger, cancellationToken);
				logger.LogInformation("Driver finished with exit code {ExitCode}", exitCode);

				return (exitCode == 0) ? LeaseResult.Success : LeaseResult.Failed;
			}
			finally
			{
				CurrentLeaseId = null;
				CurrentJobId = null;
				CurrentBatchId = null;
			}
		}
	}
}

