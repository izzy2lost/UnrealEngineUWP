// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Compute
{
	/// <summary>
	/// Interface for uploading compute work to remote machines
	/// </summary>
	public interface IComputeClient : IAsyncDisposable
	{
		/// <summary>
		/// Adds a new remote request (deprecated, use method below instead)
		/// </summary>
		/// <param name="clusterId">Cluster to execute the request</param>
		/// <param name="requirements">Requirements for the agent</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public Task<IComputeLease?> TryAssignWorkerAsync(ClusterId clusterId, Requirements? requirements, CancellationToken cancellationToken = default);
		
		/// <summary>
		/// Adds a new remote request
		/// </summary>
		/// <param name="clusterId">Cluster to execute the request</param>
		/// <param name="requirements">Requirements for the agent</param>
		/// <param name="requestId">Optional ID identifying the request over multiple calls, such as retrying the same request</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public Task<IComputeLease?> TryAssignWorkerAsync(ClusterId clusterId, Requirements? requirements, string? requestId, CancellationToken cancellationToken = default);
	}
}
