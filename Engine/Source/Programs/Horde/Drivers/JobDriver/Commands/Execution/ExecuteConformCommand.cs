// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Sessions;
using JobDriver.Execution;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace JobDriver.Commands.Execution
{
	[Command("Execute", "Conform", "Executes a conform")]
	class ExecuteConformCommand : Command
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
		readonly IOptions<DriverSettings> _driverSettings;

		public ExecuteConformCommand(IHordeClientFactory hordeClientFactory, IOptions<DriverSettings> driverSettings)
		{
			_hordeClientFactory = hordeClientFactory;
			_driverSettings = driverSettings;
		}

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			logger.LogInformation("Running conform command in driver");

			ConformTask conformTask = ConformTask.Parser.ParseFrom(Convert.FromBase64String(Task));

			IHordeClient hordeClient = _hordeClientFactory.Create();

			ConformExecutor conformExecutor = new ConformExecutor(hordeClient, WorkingDir, AgentId, LeaseId, conformTask, _driverSettings.Value, logger);
			await conformExecutor.ExecuteAsync(CancellationToken.None);

			return 0;
		}
	}
}
