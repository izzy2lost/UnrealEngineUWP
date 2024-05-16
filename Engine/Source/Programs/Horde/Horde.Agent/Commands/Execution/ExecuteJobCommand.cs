// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Sessions;
using Horde.Agent.Leases.Handlers;
using Horde.Agent.Services;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;

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

		readonly IHordeClientFactory _hordeClientFactory;
		readonly JobHandler _jobHandler;

		public ExecuteJobCommand(IHordeClientFactory hordeClientFactory, JobHandler jobHandler)
		{
			_hordeClientFactory = hordeClientFactory;
			_jobHandler = jobHandler;
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			ExecuteJobTask executeTask = ExecuteJobTask.Parser.ParseFrom(Convert.FromBase64String(Task));

			IHordeClient hordeClient = _hordeClientFactory.Create(executeTask.Token);
			await using Session session = new Session(AgentId, SessionId, WorkingDir, hordeClient);

			await _jobHandler.ExecuteInternalAsync(session, LeaseId, executeTask, logger, CancellationToken.None);
			return 0;
		}
	}
}
