// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Logs;
using Horde.Agent.Driver.Utility;
using Horde.Common.Rpc;
using HordeCommon.Rpc.Messages;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;

namespace Horde.Agent.Driver.Execution
{
	class ConformExecutor
	{
		readonly IHordeClient _hordeClient;
		readonly DirectoryReference _workingDir;
		readonly AgentId _agentId;
		readonly LeaseId _leaseId;
		readonly ConformTask _conformTask;
		readonly DriverSettings _driverSettings;
		readonly ILogger _localLogger;

		public ConformExecutor(IHordeClient hordeClient, DirectoryReference workingDir, AgentId agentId, LeaseId leaseId, ConformTask conformTask, DriverSettings driverSettings, ILogger localLogger)
		{
			_hordeClient = hordeClient;
			_workingDir = workingDir;
			_agentId = agentId;
			_leaseId = leaseId;
			_conformTask = conformTask;
			_driverSettings = driverSettings;
			_localLogger = localLogger;
		}

		public async Task ExecuteAsync(CancellationToken cancellationToken)
		{
			await using IServerLogger serverLogger = _hordeClient.CreateServerLogger(LogId.Parse(_conformTask.LogId)).WithLocalLogger(_localLogger);
			try
			{
				await ExecuteInternalAsync(serverLogger, cancellationToken);
			}
			catch (Exception ex)
			{
				serverLogger.LogError(ex, "Unhandled exception while running conform: {Message}", ex.Message);
				throw;
			}
		}

		async Task ExecuteInternalAsync(ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogInformation("Conforming, lease {LeaseId}", _leaseId);
			await TerminateProcessHelper.TerminateProcessesAsync(TerminateCondition.BeforeConform, _workingDir, _driverSettings.ProcessesToTerminate, logger, cancellationToken);

			bool removeUntrackedFiles = _conformTask.RemoveUntrackedFiles;
			IList<RpcAgentWorkspace> pendingWorkspaces = _conformTask.Workspaces;
			for (; ; )
			{
				bool isPerforceExecutor = _driverSettings.Executor.Equals(PerforceExecutor.Name, StringComparison.OrdinalIgnoreCase);
				bool isWorkspaceExecutor = _driverSettings.Executor.Equals(WorkspaceExecutor.Name, StringComparison.OrdinalIgnoreCase);

				// When using WorkspaceExecutor, only job options can override exact materializer to use
				// It will default to ManagedWorkspaceMaterializer, which is compatible with the conform call below
				// Therefore, compatibility is assumed for now. Exact materializer to use should be changed to a per workspace setting.
				// See WorkspaceExecutorFactory.CreateExecutor
				bool isExecutorConformCompatible = isPerforceExecutor || isWorkspaceExecutor;

				// Run the conform task
				if (isExecutorConformCompatible && _driverSettings.PerforceExecutor.RunConform)
				{
					await PerforceExecutor.ConformAsync(_workingDir, pendingWorkspaces, removeUntrackedFiles, logger, cancellationToken);
				}
				else
				{
					logger.LogInformation("Skipping conform. Executor={Executor} RunConform={RunConform}", _driverSettings.Executor, _driverSettings.PerforceExecutor.RunConform);
				}

				// Update the new set of workspaces
				RpcUpdateAgentWorkspacesRequest request = new RpcUpdateAgentWorkspacesRequest();
				request.AgentId = _agentId.ToString();
				request.Workspaces.AddRange(pendingWorkspaces);
				request.RemoveUntrackedFiles = removeUntrackedFiles;

				JobRpc.JobRpcClient hordeRpc = await _hordeClient.CreateGrpcClientAsync<JobRpc.JobRpcClient>(cancellationToken);

				RpcUpdateAgentWorkspacesResponse response = await hordeRpc.UpdateAgentWorkspacesAsync(request, cancellationToken: cancellationToken);
				if (!response.Retry)
				{
					logger.LogInformation("Conform finished");
					break;
				}

				logger.LogInformation("Pending workspaces have changed - running conform again...");
				pendingWorkspaces = response.PendingWorkspaces;
				removeUntrackedFiles = response.RemoveUntrackedFiles;
			}
		}
	}
}
