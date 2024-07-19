// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Pools;
using HordeServer.Auditing;

#pragma warning disable CA1716 // rename parameter property so that it no longer conflicts with the reserved language keyword 'Property'

namespace HordeServer.Agents
{
	/// <summary>
	/// Interface for a collection of agent documents
	/// </summary>
	public interface IAgentCollection
	{
		/// <summary>
		/// Adds a new agent with the given properties
		/// </summary>
		/// <param name="id">Id for the new agent</param>
		/// <param name="ephemeral">Whether the agent is ephemeral or not</param>
		/// <param name="enrollmentKey">Key used to identify a unique enrollment for the agent with this id</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task<IAgent> AddAsync(AgentId id, bool ephemeral, string enrollmentKey, CancellationToken cancellationToken = default);

		/// <summary>
		/// Deletes an agent
		/// </summary>
		/// <param name="agentId">Unique id of the agent</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task ForceDeleteAsync(AgentId agentId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets an agent by ID
		/// </summary>
		/// <param name="agentId">Unique id of the agent</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The agent document</returns>
		Task<IAgent?> GetAsync(AgentId agentId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets multiple agents by ID
		/// </summary>
		/// <param name="agentIds">List of unique IDs of the agents</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The agent documents</returns>
		Task<IReadOnlyList<IAgent>> GetManyAsync(List<AgentId> agentIds, CancellationToken cancellationToken = default);

		/// <summary>
		/// Finds all agents matching certain criteria
		/// </summary>
		/// <param name="poolId">The pool ID in string form containing the agent</param>
		/// <param name="modifiedAfter">If set, only returns agents modified after this time</param>
		/// <param name="property">Property to look for</param>
		/// <param name="status">Status to look for</param>
		/// <param name="enabled">Enabled/disabled status to look for</param>
		/// <param name="includeDeleted">Whether agents marked as deleted should be included</param>
		/// <param name="index">Index of the first result</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="consistentRead">If the database read should be made to the replica server</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of agents matching the given criteria</returns>
		Task<IReadOnlyList<IAgent>> FindAsync(PoolId? poolId = null, DateTime? modifiedAfter = null, string? property = null, AgentStatus? status = null, bool? enabled = null, bool includeDeleted = false, int? index = null, int? count = null, bool consistentRead = true, CancellationToken cancellationToken = default);

		/// <summary>
		/// Finds all agents with an expired session
		/// </summary>
		/// <param name="utcNow">The current time</param>
		/// <param name="maxAgents">Maximum number of agents to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of agents</returns>
		Task<IReadOnlyList<IAgent>> FindExpiredAsync(DateTime utcNow, int maxAgents, CancellationToken cancellationToken = default);

		/// <summary>
		/// Finds all agents marked as deleted
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of agents</returns>
		Task<IReadOnlyList<IAgent>> FindDeletedAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Finds all active agent lease IDs
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of agent lease IDs</returns>
		Task<List<LeaseId>> FindActiveLeaseIdsAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Get all child lease IDs
		/// </summary>
		/// <param name="id">Lease ID</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of agent lease IDs</returns>
		Task<List<LeaseId>> GetChildLeaseIdsAsync(LeaseId id, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets the log channel for an agent
		/// </summary>
		/// <param name="agentId"></param>
		/// <returns></returns>
		IAuditLogChannel<AgentId> GetLogger(AgentId agentId);

		/// <summary>
		/// Sends a notification that an update event has ocurred
		/// </summary>
		/// <param name="agentId">Agent that has been updated</param>
		Task PublishUpdateEventAsync(AgentId agentId);

		/// <summary>
		/// Subscribe to notifications on agent states being updated
		/// </summary>
		/// <param name="onUpdate">Callback for updates</param>
		/// <returns>Disposable subscription object</returns>
		Task<IAsyncDisposable> SubscribeToUpdateEventsAsync(Action<AgentId> onUpdate);
	}
}
