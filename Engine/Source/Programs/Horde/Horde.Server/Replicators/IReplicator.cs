// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Replicators;

namespace Horde.Server.Replicators
{
	/// <summary>
	/// Snapshot of the state for a replicator; a process which transfers data from an external source control provider into Horde.
	/// </summary>
	public interface IReplicator
	{
		/// <summary>
		/// Identifier for this replicator
		/// </summary>
		ReplicatorId Id { get; }

		/// <summary>
		/// The last change that was replicated
		/// </summary>
		int? LastChange { get; }

		/// <summary>
		/// Time at which the last change was replicated
		/// </summary>
		DateTime? LastChangeFinishTime { get; }

		/// <summary>
		/// The current change being replicated
		/// </summary>
		int? CurrentChange { get; }

		/// <summary>
		/// Time at which the current change was replicated
		/// </summary>
		DateTime? CurrentChangeStartTime { get; }

		/// <summary>
		/// Last error with replication, if there is one.
		/// </summary>
		string? Error { get; }

		/// <summary>
		/// Refreshes the replicator, ensuring it's the latest version
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>New instance of the replicator, or null if it's been deleted</returns>
		Task<IReplicator?> RefreshAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Attempts to delete the replicator.
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation.</param>
		/// <returns>True if the replicator was deleted</returns>
		Task<bool> TryDeleteAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Attempt to update the replicator
		/// </summary>
		/// <param name="options">Options for the update</param>
		/// <param name="cancellationToken">Cancellation token for the operation.</param>
		/// <returns>Updated replicator if the operation succeeded, null otherwise.</returns>
		Task<IReplicator?> TryUpdateAsync(UpdateReplicatorOptions options, CancellationToken cancellationToken = default);
	}

	/// <summary>
	/// Parameters for updating <see cref="IReplicator"/>
	/// </summary>
	public record class UpdateReplicatorOptions(int? NewLastChange = null, int? NewCurrentChange = null, string? NewError = null);
}
