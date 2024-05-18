// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Agents.Leases;
using Horde.Agent.Driver;
using Horde.Agent.Driver.Execution;
using Horde.Agent.Services;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Agent.Leases.Handlers
{
	class ConformHandler : LeaseHandler<ConformTask>
	{
		readonly DriverSettings _driverSettings;

		/// <summary>
		/// Constructor
		/// </summary>
		public ConformHandler(IOptions<DriverSettings> driverSettings)
		{
			_driverSettings = driverSettings.Value;
		}

		/// <inheritdoc/>
		public override async Task<LeaseResult> ExecuteAsync(ISession session, LeaseId leaseId, ConformTask conformTask, ILogger localLogger, CancellationToken cancellationToken)
		{
			ConformExecutor executor = new ConformExecutor(session.HordeClient, session.WorkingDir, session.AgentId, leaseId, conformTask, _driverSettings, localLogger);
			await executor.ExecuteAsync(cancellationToken);

			return LeaseResult.Success;
		}
	}
}

