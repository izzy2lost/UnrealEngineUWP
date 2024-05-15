// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Sessions;
using Horde.Agent.Driver;
using Horde.Agent.Leases.Handlers;
using Horde.Agent.Services;
using Horde.Agent.Utility;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Agent.Commands.Execution
{
	[Command("Execute", "Job", "Executes a job", Advertise = false)]
	class ExecuteJobCommand : Command
	{
		[CommandLine("-AgentId=", Required = true)]
		public AgentId AgentId { get; set; }

		[CommandLine("-SessionId=", Required = true)]
		public SessionId SessionId { get; set; }

		[CommandLine("-LeaseId", Required = true)]
		public LeaseId LeaseId { get; set; }

		[CommandLine("-Task=", Required = true)]
		public string Task { get; set; } = null!;

		[CommandLine("-WorkingDir=", Required = true)]
		public DirectoryReference WorkingDir { get; set; } = null!;

		readonly GrpcService _grpcService;
		readonly JobHandler _jobHandler;
		readonly AgentSettings _agentSettings;
		readonly DriverSettings _driverSettings;

		public ExecuteJobCommand(GrpcService grpcService, JobHandler jobHandler, IOptions<AgentSettings> agentSettings, IOptions<DriverSettings> driverSettings)
		{
			_grpcService = grpcService;
			_jobHandler = jobHandler;
			_agentSettings = agentSettings.Value;
			_driverSettings = driverSettings.Value;
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			ServerProfile serverProfile = _agentSettings.GetCurrentServerProfile();
			ExecuteJobTask executeTask = ExecuteJobTask.Parser.ParseFrom(Convert.FromBase64String(Task));

			await using RpcConnection rpcConnection = new RpcConnection(ctx => _grpcService.CreateGrpcChannelAsync(executeTask.Token, ctx), logger);
			await using Session session = new Session(serverProfile.Url, AgentId, SessionId, executeTask.Token, rpcConnection, WorkingDir);

			await _jobHandler.ExecuteInternalAsync(session, LeaseId, executeTask, logger, CancellationToken.None);
			return 0;
		}
	}
}
