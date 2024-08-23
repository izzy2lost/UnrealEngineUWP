// Copyright Epic Games, Inc. All Rights Reserved.

using System.Runtime.CompilerServices;
using EpicGames.Core;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Sessions;
using EpicGames.Redis;
using Google.Protobuf;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages;
using HordeServer.Agents.Sessions;
using HordeServer.Server;
using Microsoft.Extensions.Hosting;
using StackExchange.Redis;

namespace HordeServer.Agents
{
	class AgentScheduler : IAgentScheduler, IHostedService, IAsyncDisposable
	{
#pragma warning disable CA1822 // Make property static
		static class Keys
		{
			public static RedisHashKey<AgentId, SessionId> Agents { get; }
				= new($"compute:agents");

			public static SessionCollectionKeys Sessions { get; }
				= new();

			public static LeaseCollectionKeys Leases { get; }
				= new();
		}

		record struct SessionCollectionKeys
		{
			public RedisHashKey<SessionId, long> ExpiryTimes
				=> new($"compute:sessions:update-ticks");

			public SessionKeys this[SessionId sessionId]
				=> new(sessionId);
		}

		record struct SessionKeys(SessionId SessionId)
		{
			public RedisStringKey<RpcAgentCapabilities> Capabilities
				=> new($"compute:sessions:{SessionId}:caps");

			public RedisStringKey<RpcSession> State
				=> new($"compute:sessions:{SessionId}:state");
		}

		record struct LeaseCollectionKeys
		{
			public RedisSetKey<LeaseId> Current
				=> new($"compute:leases:active");

			public LeaseKeys this[LeaseId leaseId]
				=> new(leaseId);
		}

		record struct LeaseKeys(LeaseId LeaseId)
		{
			public RedisSetKey<LeaseId> Children
				=> new($"compute:leases:{LeaseId}:children");
		}

#pragma warning restore CA1822

		public event Action<SessionId>? SessionUpdated;

		readonly IRedisService _redisService;
		readonly IClock _clock;

		static readonly RedisChannel<SessionId> s_sessionUpdateChannel = new RedisChannel<SessionId>(RedisChannel.Literal("compute:sessions:update"));

		IAsyncDisposable? _updateEventSubscription;

		/// <summary>
		/// Constructor
		/// </summary>
		public AgentScheduler(IRedisService redisService, IClock clock)
		{
			_redisService = redisService;
			_clock = clock;
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			if (_updateEventSubscription != null)
			{
				await _updateEventSubscription.DisposeAsync();
				_updateEventSubscription = null;
			}
		}

		void OnSessionUpdate(SessionId sessionId)
			=> SessionUpdated?.Invoke(sessionId);

		/// <inheritdoc/>
		public async Task StartAsync(CancellationToken cancellationToken)
		{
			_updateEventSubscription = await _redisService.GetDatabase().Multiplexer.SubscribeAsync(s_sessionUpdateChannel, OnSessionUpdate);
		}

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			if (_updateEventSubscription != null)
			{
				await _updateEventSubscription.DisposeAsync();
				_updateEventSubscription = null;
			}
		}

		/// <inheritdoc/>
		public async Task<RpcSession?> TryCreateSessionAsync(AgentId agentId, SessionId sessionId, RpcAgentCapabilities capabilities, CancellationToken cancellationToken = default)
		{
			DateTime utcNow = _clock.UtcNow;

			byte[] capabilitiesData = capabilities.ToByteArray();
			IoHash capabilitiesHash = IoHash.Compute(capabilitiesData);

			RpcSession newSession = new RpcSession();
			newSession.AgentId = agentId;
			newSession.SessionId = sessionId;
			newSession.CapabilitiesHash = capabilitiesHash.ToString();
			newSession.ExpiryTicks = (utcNow + RpcSession.ExpireAfterTime).Ticks;
			newSession.Status = RpcAgentStatus.Ok;

			ITransaction transaction = _redisService.GetDatabase().CreateTransaction();
			transaction.AddCondition(Keys.Sessions.ExpiryTimes.HashNotExists(sessionId));
			_ = transaction.HashSetAsync(Keys.Sessions.ExpiryTimes, sessionId, newSession.ExpiryTicks, flags: CommandFlags.FireAndForget);
			_ = transaction.StringSetAsync(Keys.Sessions[sessionId].Capabilities.Inner, capabilitiesData, flags: CommandFlags.FireAndForget);
			_ = transaction.StringSetAsync(Keys.Sessions[sessionId].State, newSession, flags: CommandFlags.FireAndForget);

			if (!await transaction.ExecuteAsync().WaitAsync(cancellationToken))
			{
				return null;
			}

			return newSession;
		}

		/// <inheritdoc/>
		public async Task<RpcSession?> TryGetSessionAsync(AgentId agentId, CancellationToken cancellationToken = default)
		{
			IDatabase database = _redisService.GetDatabase();

			SessionId sessionId = await database.HashGetAsync(Keys.Agents, agentId).WaitAsync(cancellationToken);
			if (sessionId.Id.IsEmpty)
			{
				return null;
			}

			return await TryGetSessionAsync(sessionId, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<RpcSession?> TryGetSessionAsync(SessionId sessionId, CancellationToken cancellationToken = default)
		{
			IDatabase database = _redisService.GetDatabase();
			return await database.StringGetAsync(Keys.Sessions[sessionId].State);
		}

		/// <inheritdoc/>
		public async Task<RpcAgentCapabilities?> TryGetSessionCapabilitiesAsync(RpcSession session, CancellationToken cancellationToken = default)
		{
			IDatabase database = _redisService.GetDatabase();

			byte[]? data = (byte[]?)await database.StringGetAsync(Keys.Sessions[session.SessionId].Capabilities.Inner);
			if (data == null)
			{
				return null;
			}
			if (!session.CapabilitiesHash.Equals(IoHash.Compute(data).ToString(), StringComparison.Ordinal))
			{
				return null;
			}

			return RpcAgentCapabilities.Parser.ParseFrom(data);
		}

		/// <inheritdoc/>
		public async Task<RpcSession?> TryUpdateSessionAsync(RpcSession session, RpcSession? newSession, RpcAgentCapabilities? newCapabilities, CancellationToken cancellationToken)
		{
			SessionId sessionId = session.SessionId;

			// Copy the session document if a new one was not specified, and update the last modified time
			newSession ??= new RpcSession(session);

			// Start building the update transaction
			ITransaction transaction = _redisService.GetDatabase().CreateTransaction();
			transaction.AddCondition(Keys.Sessions.ExpiryTimes.HashEqual(newSession.SessionId, session.ExpiryTicks));

			// Handle agents transitioning to the stopping state
			if (newSession.Status == RpcAgentStatus.Stopped)
			{
				// TODO: remove all leases
				newSession.ExpiryTicks = Math.Min(newSession.ExpiryTicks, _clock.UtcNow.Ticks);

				_ = transaction.HashDeleteAsync(Keys.Agents, newSession.AgentId, flags: CommandFlags.FireAndForget);
				_ = transaction.HashDeleteAsync(Keys.Sessions.ExpiryTimes, newSession.SessionId, flags: CommandFlags.FireAndForget);
				_ = transaction.KeyDeleteAsync(Keys.Sessions[sessionId].Capabilities, flags: CommandFlags.FireAndForget);
				_ = transaction.KeyDeleteAsync(Keys.Sessions[sessionId].State, flags: CommandFlags.FireAndForget);
			}
			else
			{
				// Extend the expiry time, ensuring it only ever increases
				newSession.ExpiryTicks = Math.Max(session.ExpiryTicks + 1, (_clock.UtcNow + RpcSession.ExpireAfterTime).Ticks);
				_ = transaction.HashSetAsync(Keys.Sessions.ExpiryTimes, session.SessionId, newSession.ExpiryTicks, flags: CommandFlags.FireAndForget);

				// Update the capabilities
				if (newCapabilities != null)
				{
					byte[] capabilitiesData = newCapabilities.ToByteArray();
					IoHash capabilitiesHash = IoHash.Compute(capabilitiesData);

					newSession.CapabilitiesHash = capabilitiesHash.ToString();
					_ = transaction.StringSetAsync(Keys.Sessions[session.SessionId].Capabilities.Inner, capabilitiesData, flags: CommandFlags.FireAndForget);
				}

				// Add any new leases to the global state
				foreach (RpcSessionLease newLease in newSession.Leases)
				{
					if (!session.Leases.Any(x => x.Id == newLease.Id))
					{
						_ = transaction.SetAddAsync(Keys.Leases.Current, newLease.Id, flags: CommandFlags.FireAndForget);
						if (newLease.ParentId != null)
						{
							_ = transaction.SetAddAsync(Keys.Leases[newLease.ParentId.Value].Children, newLease.Id, flags: CommandFlags.FireAndForget);
						}
					}
				}

				// Update the session state
				_ = transaction.StringSetAsync(Keys.Sessions[session.SessionId].State, newSession, flags: CommandFlags.FireAndForget);
			}

			// Remove any complete leases from the global state
			foreach (RpcSessionLease oldLease in session.Leases)
			{
				if (!newSession.Leases.Any(x => x.Id == oldLease.Id))
				{
					_ = transaction.SetRemoveAsync(Keys.Leases.Current, oldLease.Id, flags: CommandFlags.FireAndForget);
					if (oldLease.ParentId != null)
					{
						_ = transaction.SetRemoveAsync(Keys.Leases[oldLease.ParentId.Value].Children, oldLease.Id, flags: CommandFlags.FireAndForget);
					}
				}
			}

			// Execute the transaction
			if (!await transaction.ExecuteAsync().WaitAsync(cancellationToken))
			{
				return null;
			}

			// Notify watchers that the session state has changed
			_ = _redisService.GetDatabase().PublishAsync(s_sessionUpdateChannel, sessionId, CommandFlags.FireAndForget);
			return newSession;
		}

		/// <inheritdoc/>
		public async IAsyncEnumerable<RpcSession> FindExpiredSessionsAsync([EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			DateTime utcNow = _clock.UtcNow;
			IDatabase database = _redisService.GetDatabase();

			HashEntry<SessionId, long>[] entries = await database.HashGetAllAsync(Keys.Sessions.ExpiryTimes);
			foreach ((SessionId sessionId, long expiryTicks) in entries)
			{
				if (expiryTicks < utcNow.Ticks)
				{
					RpcSession? session = await TryGetSessionAsync(sessionId, cancellationToken);
					if (session != null && session.ExpiryTicks == expiryTicks)
					{
						yield return session;
					}
				}
			}
		}

		/// <inheritdoc/>
		public async Task<LeaseId[]> FindActiveLeaseIdsAsync(CancellationToken cancellationToken = default)
		{
			IDatabase database = _redisService.GetDatabase();
			return await database.SetMembersAsync(Keys.Leases.Current).WaitAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<int> GetChildLeaseCountAsync(LeaseId id, CancellationToken cancellationToken = default)
		{
			IDatabase database = _redisService.GetDatabase();
			return (int)await database.SetLengthAsync(Keys.Leases[id].Children).WaitAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<LeaseId[]> GetChildLeaseIdsAsync(LeaseId id, CancellationToken cancellationToken = default)
		{
			IDatabase database = _redisService.GetDatabase();
			return await database.SetMembersAsync(Keys.Leases[id].Children).WaitAsync(cancellationToken);
		}
	}
}
