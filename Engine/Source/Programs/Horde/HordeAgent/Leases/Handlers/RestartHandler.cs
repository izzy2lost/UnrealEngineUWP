// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Agents.Leases;
using HordeAgent.Services;
using HordeAgent.Utility;
using HordeCommon.Rpc.Messages;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;

namespace HordeAgent.Leases.Handlers
{
	class RestartHandler : LeaseHandler<RestartTask>
	{
		public RestartHandler(RpcLease lease)
			: base(lease)
		{ }

		/// <inheritdoc/>
		protected override Task<LeaseResult> ExecuteAsync(ISession session, LeaseId leaseId, RestartTask task, ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogInformation("Scheduling restart task for agent {AgentId}", session.AgentId);
			SessionResult result = new SessionResult((logger, ctx) => Shutdown.ExecuteAsync(true, logger, ctx));
			return Task.FromResult(new LeaseResult(result));
		}
	}

	class RestartHandlerFactory : LeaseHandlerFactory<RestartTask>
	{
		public override LeaseHandler<RestartTask> CreateHandler(RpcLease lease)
			=> new RestartHandler(lease);
	}
}
