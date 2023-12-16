// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Replicators;

namespace Horde.Server.Replicators
{
	/// <summary>
	/// Interface for a collection of replicators
	/// </summary>
	public interface IReplicatorCollection
	{
		/// <summary>
		/// Finds all replicators
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>All the currently active replicators</returns>
		Task<List<IReplicator>> FindAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets the state of a replicator from the collection
		/// </summary>
		/// <param name="id">Identifier for the replicator</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The state of the requested replicator</returns>
		Task<IReplicator?> GetAsync(ReplicatorId id, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets the state of a replicator or adds a new one to the collection
		/// </summary>
		/// <param name="id">Identifier for the replicator</param>
		/// <param name="replicator">Initial state for the new replicator</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>New replicator instance</returns>
		Task<IReplicator> GetOrAddAsync(ReplicatorId id, CreateReplicatorOptions? replicator = null, CancellationToken cancellationToken = default);
	}

	/// <summary>
	/// Parameters for creating a new <see cref="IReplicator"/>
	/// </summary>
	public record class CreateReplicatorOptions(int? LastChange = null, int? CurrentChange = null, string? Error = null);
}
