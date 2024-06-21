// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Agents.Leases;
using HordeAgent.Services;
using HordeAgent.Utility;
using HordeCommon.Rpc.Messages;
using HordeCommon.Rpc.Tasks;
using Microsoft.Extensions.Logging;

namespace HordeAgent.Leases.Handlers
{
	class ShutdownHandler : LeaseHandler<ShutdownTask>
	{
		public ShutdownHandler(RpcLease lease)
			: base(lease)
		{ }

		/// <inheritdoc/>
		protected override Task<LeaseResult> ExecuteAsync(ISession session, LeaseId leaseId, ShutdownTask task, ILogger logger, CancellationToken cancellationToken)
		{
			logger.LogInformation("Scheduling shutdown task for agent {AgentId}", session.AgentId);
			SessionResult result = new SessionResult((logger, ctx) => Shutdown.ExecuteAsync(false, logger, ctx));
			return Task.FromResult(new LeaseResult(result));
		}
	}

	class ShutdownHandlerFactory : LeaseHandlerFactory<ShutdownTask>
	{
		public override LeaseHandler<ShutdownTask> CreateHandler(RpcLease lease)
			=> new ShutdownHandler(lease);
	}
}

