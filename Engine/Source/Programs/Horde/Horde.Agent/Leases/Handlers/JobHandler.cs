// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using System.Reflection;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Agents.Leases;
using Google.Protobuf;
using Horde.Agent.Driver;
using Horde.Agent.Driver.Execution;
using Horde.Agent.Services;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
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

		readonly IEnumerable<IJobExecutorFactory> _executorFactories;
		readonly AgentSettings _agentSettings;
		readonly DriverSettings _driverSettings;

		/// <summary>
		/// Constructor
		/// </summary>
		public JobHandler(IEnumerable<IJobExecutorFactory> executorFactories, IOptions<AgentSettings> agentSettings, IOptions<DriverSettings> driverSettings)
		{
			_executorFactories = executorFactories;
			_agentSettings = agentSettings.Value;
			_driverSettings = driverSettings.Value;
		}

		/// <inheritdoc/>
		public override async Task<LeaseResult> ExecuteAsync(ISession session, LeaseId leaseId, ExecuteJobTask executeTask, ILogger localLogger, CancellationToken cancellationToken)
		{
			try
			{
				CurrentLeaseId = leaseId;
				CurrentJobId = executeTask.JobId;
				CurrentBatchId = executeTask.BatchId;

				executeTask.JobOptions ??= new RpcJobOptions();

				if (executeTask.JobOptions.RunInSeparateProcess ?? false)
				{
					if (AgentApp.IsSelfContained)
					{
						// TODO: Implement handling for invoking a self-contained agent process (i.e handle "dotnet" below)
						throw new NotSupportedException("Running job in a separate process not supported for self-contained agents");
					}

					using (ManagedProcessGroup processGroup = new ManagedProcessGroup())
					{
						List<string> arguments = new List<string>();
#pragma warning disable IL3000 // Avoid accessing Assembly file path when publishing as a single file
						arguments.Add(Assembly.GetExecutingAssembly().Location);
#pragma warning restore IL3000 // Avoid accessing Assembly file path when publishing as a single file						
						arguments.Add("execute");
						arguments.Add("job");
						arguments.Add($"-Server={_agentSettings.GetCurrentServerProfile().Name}");
						arguments.Add($"-AgentId={session.AgentId}");
						arguments.Add($"-SessionId={session.SessionId}");
						arguments.Add($"-LeaseId={leaseId}");
						arguments.Add($"-WorkingDir={session.WorkingDir}");
						arguments.Add($"-Task={Convert.ToBase64String(executeTask.ToByteArray())}");

						string commandLine = CommandLineArguments.Join(arguments);
						localLogger.LogInformation("Running child process with arguments: {CommandLine}", commandLine);

						using (ManagedProcess process = new ManagedProcess(processGroup, "dotnet", commandLine, null,
								   null, ProcessPriorityClass.Normal))
						{
							using (LogEventParser parser = new LogEventParser(localLogger))
							{
								for (; ; )
								{
									string? line = await process.ReadLineAsync(cancellationToken);
									if (line == null)
									{
										break;
									}

									parser.WriteLine(line);
								}
							}

							await process.WaitForExitAsync(CancellationToken.None);

							if (process.ExitCode != 0)
							{
								return LeaseResult.Failed;
							}
						}
					}

					return LeaseResult.Success;
				}

				return await ExecuteInternalAsync(session.HordeClient, session.WorkingDir, leaseId, executeTask, localLogger, cancellationToken);
			}
			finally
			{
				CurrentLeaseId = null;
				CurrentJobId = null;
				CurrentBatchId = null;
			}
		}

		internal async Task<LeaseResult> ExecuteInternalAsync(IHordeClient hordeClient, DirectoryReference workingDir, LeaseId leaseId, ExecuteJobTask executeTask, ILogger localLogger, CancellationToken cancellationToken)
		{
			GlobalTracer.Instance.ActiveSpan?.SetTag("jobId", executeTask.JobId.ToString());
			GlobalTracer.Instance.ActiveSpan?.SetTag("jobName", executeTask.JobName.ToString());
			GlobalTracer.Instance.ActiveSpan?.SetTag("batchId", executeTask.BatchId.ToString());

			await JobExecutorHelpers.ExecuteAsync(hordeClient, workingDir, leaseId, executeTask, _executorFactories, _driverSettings, localLogger, cancellationToken);
			return LeaseResult.Success;
		}
	}
}

