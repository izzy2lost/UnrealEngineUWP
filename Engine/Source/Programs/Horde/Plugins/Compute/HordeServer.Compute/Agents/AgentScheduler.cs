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
using HordeServer.Server;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using StackExchange.Redis;

namespace HordeServer.Agents
{
	class AgentScheduler : IAgentScheduler, IHostedService, IAsyncDisposable
	{
		/// <summary>
		/// Time after which we expire filters
		/// </summary>
		public static TimeSpan ExpireFiltersTime { get; } = TimeSpan.FromMinutes(30.0);

#pragma warning disable CA1822 // Make property static
		static class Keys
		{
			public static FilterCollectionKeys Filters { get; }
				= new();

			public static SessionCollectionKeys Sessions { get; }
				= new();

			public static LeaseCollectionKeys Leases { get; }
				= new();
		}

		record struct FilterCollectionKeys
		{
			public RedisSortedSetKey<IoHash> Created
				=> new($"compute:filters:created");

			public RedisSortedSetKey<IoHash> Deleted
				=> new($"compute:filters:deleted");

			public RedisHashKey<IoHash, long> Touched
				=> new($"compute:filters:touched");

			public FilterKeys this[IoHash requirementsHash]
				=> new(requirementsHash);
		}

		record struct FilterKeys(IoHash RequirementsHash)
		{
			public RedisStringKey<RpcAgentRequirements> Requirements
				=> new($"compute:filters:{RequirementsHash}:reqs");

			public RedisSetKey<SessionId> Sessions
				=> new($"compute:filters:{RequirementsHash}:sessions");
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

			public RedisSetKey<IoHash> Filters
				=> new($"compute:sessions:{SessionId}:filters");

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
		readonly ITicker _updateCachedFiltersTicker;
		readonly ITicker _expireFiltersTicker;
		readonly ILogger _logger;
		readonly MemoryCache _memoryCache;

		static readonly RedisChannel<SessionId> s_sessionUpdateChannel = new RedisChannel<SessionId>(RedisChannel.Literal("compute:sessions:update"));

		record class CachedFilters(long Ticks, SortedSetEntry<IoHash>[] Created, SortedSetEntry<IoHash>[] Deleted);
		CachedFilters _cachedFilters = new CachedFilters(0, Array.Empty<SortedSetEntry<IoHash>>(), Array.Empty<SortedSetEntry<IoHash>>());

		IAsyncDisposable? _updateEventSubscription;

		/// <summary>
		/// Constructor
		/// </summary>
		public AgentScheduler(IRedisService redisService, IClock clock, ILogger<AgentScheduler> logger)
		{
			_redisService = redisService;
			_clock = clock;
			_updateCachedFiltersTicker = clock.AddTicker<AgentScheduler>(TimeSpan.FromSeconds(30.0), UpdateCachedFiltersTickAsync, logger);
			_expireFiltersTicker = clock.AddSharedTicker($"{nameof(AgentScheduler)}.{nameof(ExpireFiltersAsync)}", TimeSpan.FromMinutes(5.0), ExpireFiltersAsync, logger);
			_memoryCache = new MemoryCache(new MemoryCacheOptions());
			_logger = logger;
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			_memoryCache.Dispose();

			await _updateCachedFiltersTicker.DisposeAsync();
			await _expireFiltersTicker.DisposeAsync();

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

			await _updateCachedFiltersTicker.StartAsync();
			await _expireFiltersTicker.StartAsync();
		}

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			await _expireFiltersTicker.StopAsync();
			await _updateCachedFiltersTicker.StopAsync();

			if (_updateEventSubscription != null)
			{
				await _updateEventSubscription.DisposeAsync();
				_updateEventSubscription = null;
			}
		}

		#region Sessions

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

			if (!await UpdateFiltersAsync(_cachedFilters, transaction, newSession, capabilities, cancellationToken))
			{
				return null;
			}
			if (!await transaction.ExecuteAsync().WaitAsync(cancellationToken))
			{
				return null;
			}

			return newSession;
		}

		/// <inheritdoc/>
		public async Task<RpcSession?> TryGetSessionAsync(SessionId sessionId, CancellationToken cancellationToken = default)
		{
			IDatabase database = _redisService.GetDatabase();
			return await database.StringGetAsync(Keys.Sessions[sessionId].State);
		}

		record class CapabilitiesLookup(IReadOnlySet<string> Properties, IReadOnlyDictionary<string, int> Resources)
		{
			public CapabilitiesLookup(RpcAgentCapabilities capabilities)
				: this(new HashSet<string>(capabilities.Properties, StringComparer.Ordinal), new Dictionary<string, int>(capabilities.Resources, StringComparer.Ordinal)) { }
		}

		record class CapabilitiesCacheEntry(string Hash, CapabilitiesLookup Capabilities, RpcAgentCapabilities Message);

		async ValueTask<CapabilitiesCacheEntry?> TryGetCachedCapabilitiesAsync(RpcSession session, CancellationToken cancellationToken = default)
		{
			CapabilitiesCacheEntry? cachedCapabilities;
			if (!_memoryCache.TryGetValue(session.SessionId, out cachedCapabilities) || cachedCapabilities == null || cachedCapabilities.Hash != session.CapabilitiesHash)
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

				RpcAgentCapabilities capabilities = RpcAgentCapabilities.Parser.ParseFrom(data);
				cachedCapabilities = new CapabilitiesCacheEntry(session.CapabilitiesHash, new CapabilitiesLookup(capabilities), capabilities);

				using (ICacheEntry entry = _memoryCache.CreateEntry(session.SessionId))
				{
					entry.SetSlidingExpiration(TimeSpan.FromMinutes(30.0));
					entry.Value = cachedCapabilities;
				}
			}
			return cachedCapabilities;
		}

		/// <inheritdoc/>
		public async Task<RpcAgentCapabilities?> TryGetCapabilitiesAsync(RpcSession session, CancellationToken cancellationToken = default)
		{
			CapabilitiesCacheEntry? cachedCapabilities = await TryGetCachedCapabilitiesAsync(session, cancellationToken);
			return cachedCapabilities?.Message;
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

				_ = transaction.HashDeleteAsync(Keys.Sessions.ExpiryTimes, newSession.SessionId, flags: CommandFlags.FireAndForget);
				_ = transaction.KeyDeleteAsync(Keys.Sessions[sessionId].Capabilities, flags: CommandFlags.FireAndForget);
				_ = transaction.KeyDeleteAsync(Keys.Sessions[sessionId].State, flags: CommandFlags.FireAndForget);
			}
			else
			{
				DateTime utcNow = _clock.UtcNow;

				// Extend the expiry time, ensuring it only ever increases
				newSession.ExpiryTicks = Math.Max(session.ExpiryTicks + 1, (utcNow + RpcSession.ExpireAfterTime).Ticks);
				_ = transaction.HashSetAsync(Keys.Sessions.ExpiryTimes, session.SessionId, newSession.ExpiryTicks, flags: CommandFlags.FireAndForget);

				// Update the capabilities
				if (newCapabilities != null)
				{
					byte[] capabilitiesData = newCapabilities.ToByteArray();
					string capabilitiesHash = IoHash.Compute(capabilitiesData).ToString();

					if (capabilitiesHash != newSession.CapabilitiesHash)
					{
						newSession.CapabilitiesHash = capabilitiesHash;
						_ = transaction.StringSetAsync(Keys.Sessions[session.SessionId].Capabilities.Inner, capabilitiesData, flags: CommandFlags.FireAndForget);
					}
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

				// Update the filters that this session belongs to
				if (!await UpdateFiltersAsync(_cachedFilters, transaction, newSession, newCapabilities, cancellationToken))
				{
					return null;
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

			// Trace the new expiry time for debugging
			_logger.LogDebug("Updated session {SessionId} expiry time to {ExpiryTime}", sessionId, newSession.ExpiryTime);

			// Notify watchers that the session state has changed
			if (newSession.Status != session.Status || !newSession.CapabilitiesHash.Equals(session.CapabilitiesHash, StringComparison.Ordinal) || !newSession.Leases.Equals(session.Leases))
			{
				_ = _redisService.GetDatabase().PublishAsync(s_sessionUpdateChannel, sessionId, CommandFlags.FireAndForget);
			}
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

		#endregion
		#region Filters

		/// <inheritdoc/>
		public async Task<IoHash> CreateFilterAsync(RpcAgentRequirements requirements, CancellationToken cancellationToken = default)
		{
			byte[] requirementsData = requirements.ToByteArray();
			IoHash requirementsHash = IoHash.Compute(requirementsData);

			for (; ; )
			{
				IDatabase database = _redisService.GetDatabase();
				DateTime utcNow = _clock.UtcNow;

				ITransaction transaction = database.CreateTransaction();
				transaction.AddCondition(Keys.Filters.Touched.HashNotExists(requirementsHash));
				_ = transaction.HashSetAsync(Keys.Filters.Touched, requirementsHash, utcNow.Ticks);
				_ = transaction.StringSetAsync(Keys.Filters[requirementsHash].Requirements.Inner, requirementsData, flags: CommandFlags.FireAndForget);
				_ = transaction.SortedSetAddAsync(Keys.Filters.Created, requirementsHash, utcNow.Ticks, flags: CommandFlags.FireAndForget);
				_ = transaction.SortedSetRemoveAsync(Keys.Filters.Deleted, requirementsHash, flags: CommandFlags.FireAndForget);

				if (await transaction.ExecuteAsync().WaitAsync(cancellationToken))
				{
					break;
				}

				transaction = database.CreateTransaction();
				transaction.AddCondition(Keys.Filters.Touched.HashNotExists(requirementsHash));
				_ = transaction.HashSetAsync(Keys.Filters.Touched, requirementsHash, utcNow.Ticks, flags: CommandFlags.FireAndForget);

				if (await transaction.ExecuteAsync().WaitAsync(cancellationToken))
				{
					break;
				}
			}

			return requirementsHash;
		}

		/// <summary>
		/// Update all the cached filters
		/// </summary>
		async ValueTask UpdateCachedFiltersTickAsync(CancellationToken cancellationToken)
		{
			IDatabase database = _redisService.GetDatabase();
			DateTime utcNow = _clock.UtcNow;

			ITransaction transaction = database.CreateTransaction();
			Task<SortedSetEntry<IoHash>[]> created = transaction.SortedSetRangeByRankWithScoresAsync(Keys.Filters.Created);
			Task<SortedSetEntry<IoHash>[]> deleted = transaction.SortedSetRangeByRankWithScoresAsync(Keys.Filters.Deleted);

			await transaction.ExecuteAsync().WaitAsync(cancellationToken);
			_cachedFilters = new CachedFilters(utcNow.Ticks, await created, await deleted);
		}

		/// <summary>
		/// Expire all the filters older than the standard expiry time
		/// </summary>
		async ValueTask ExpireFiltersAsync(CancellationToken cancellationToken)
		{
			IDatabase database = _redisService.GetDatabase();

			DateTime expiryTime = _clock.UtcNow - ExpireFiltersTime;

			HashEntry<IoHash, long>[] entries = await database.HashGetAllAsync(Keys.Filters.Touched).WaitAsync(cancellationToken);
			foreach ((IoHash requirementsHash, long ticks) in entries)
			{
				cancellationToken.ThrowIfCancellationRequested();
				if (ticks < expiryTime.Ticks)
				{
					ITransaction transaction = database.CreateTransaction();
					transaction.AddCondition(Keys.Filters.Touched.HashEqual(requirementsHash, ticks));

					_ = transaction.HashDeleteAsync(Keys.Filters.Touched, requirementsHash, flags: CommandFlags.FireAndForget);
					_ = transaction.SortedSetRemoveAsync(Keys.Filters.Created, requirementsHash, flags: CommandFlags.FireAndForget);
					_ = transaction.SortedSetAddAsync(Keys.Filters.Deleted, requirementsHash, _clock.UtcNow.Ticks, flags: CommandFlags.FireAndForget);
					_ = transaction.KeyDeleteAsync(Keys.Filters[requirementsHash].Requirements, flags: CommandFlags.FireAndForget);
					_ = transaction.KeyDeleteAsync(Keys.Filters[requirementsHash].Sessions, flags: CommandFlags.FireAndForget);

					_ = transaction.ExecuteAsync(CommandFlags.FireAndForget);
				}
			}
		}

		/// <inheritdoc/>
		public async Task TouchFilterAsync(IoHash requirementsHash, CancellationToken cancellationToken = default)
		{
			IDatabase database = _redisService.GetDatabase();
			DateTime utcNow = _clock.UtcNow;
			await database.HashSetAsync(Keys.Filters.Touched, requirementsHash, utcNow.Ticks);
		}

		/// <inheritdoc/>
		public async Task<IoHash[]> GetFiltersAsync(CancellationToken cancellationToken = default)
		{
			IDatabase database = _redisService.GetDatabase();
			return await database.HashKeysAsync(Keys.Filters.Touched);
		}

		public async Task ForceUpdateFiltersAsync(CancellationToken cancellationToken)
		{
			IDatabase database = _redisService.GetDatabase();

			await UpdateCachedFiltersTickAsync(cancellationToken);
			CachedFilters cachedFilters = _cachedFilters;

			SessionId[] sessionIds = await database.HashKeysAsync(Keys.Sessions.ExpiryTimes);
			foreach (SessionId sessionId in sessionIds)
			{
				RpcSession? session = await TryGetSessionAsync(sessionId, cancellationToken);
				if (session != null)
				{
					ITransaction transaction = database.CreateTransaction();
					await UpdateFiltersAsync(cachedFilters, transaction, session, null, cancellationToken);
					await transaction.ExecuteAsync().WaitAsync(cancellationToken);
				}
			}
		}

		async Task<bool> UpdateFiltersAsync(CachedFilters cachedFilters, ITransaction transaction, RpcSession newSession, RpcAgentCapabilities? newCapabilities, CancellationToken cancellationToken)
		{
			long minFilterTime = Math.Max(0, newSession.LastFilterUpdateTicks - TimeSpan.FromSeconds(30.0).Ticks);

			// Add the session to any new matching filters
			SortedSetEntry<IoHash>[] createdFilters = cachedFilters.Created;

			int createdIdx = createdFilters.BinarySearch(new SortedSetEntry<IoHash>(IoHash.Zero, minFilterTime));
			if (createdIdx < 0)
			{
				createdIdx = ~createdIdx;
			}

			if (createdIdx < createdFilters.Length)
			{
				CapabilitiesLookup? capabilities;
				if (newCapabilities != null)
				{
					capabilities = new CapabilitiesLookup(newCapabilities);
				}
				else
				{
					capabilities = (await TryGetCachedCapabilitiesAsync(newSession, cancellationToken))?.Capabilities;
				}

				if (capabilities == null)
				{
					return false;
				}

				for (; createdIdx < createdFilters.Length; createdIdx++)
				{
					IoHash requirementsHash = createdFilters[createdIdx].Element;

					RpcAgentRequirements? requirements = await TryGetFilterRequirementsAsync(requirementsHash, cancellationToken);
					if (requirements != null && MeetsRequirements(capabilities, requirements))
					{
						transaction.AddCondition(Keys.Filters.Touched.HashExists(requirementsHash));
						_ = transaction.SetAddAsync(Keys.Filters[requirementsHash].Sessions, newSession.SessionId, flags: CommandFlags.FireAndForget);
						_ = transaction.SetAddAsync(Keys.Sessions[newSession.SessionId].Filters, requirementsHash, flags: CommandFlags.FireAndForget);
					}
				}
			}

			// Remove the session from any removed filters
			SortedSetEntry<IoHash>[] deletedFilters = cachedFilters.Deleted;

			int deletedIdx = deletedFilters.BinarySearch(new SortedSetEntry<IoHash>(IoHash.Zero, minFilterTime));
			if (deletedIdx < 0)
			{
				deletedIdx = ~deletedIdx;
			}

			for (; deletedIdx < deletedFilters.Length; deletedIdx++)
			{
				IoHash queueHash = deletedFilters[deletedIdx].Element;
				transaction.AddCondition(Keys.Filters.Touched.HashNotExists(queueHash));
				_ = transaction.SetRemoveAsync(Keys.Sessions[newSession.SessionId].Filters, queueHash, flags: CommandFlags.FireAndForget);
			}

			// Update the last queue time
			newSession.LastFilterUpdateTicks = cachedFilters.Ticks;
			return true;
		}

		static bool MeetsRequirements(CapabilitiesLookup capabilities, RpcAgentRequirements requirements)
		{
			foreach (string property in requirements.Properties)
			{
				if (!capabilities.Properties.Contains(property))
				{
					return false;
				}
			}
			return true;
		}

		void AddRequirementsToCache(IoHash requirementsHash, RpcAgentRequirements requirements)
		{
			using (ICacheEntry entry = _memoryCache.CreateEntry(requirementsHash))
			{
				entry.SetSlidingExpiration(TimeSpan.FromMinutes(30.0));
				entry.SetValue(requirements);
			}
		}

		/// <inheritdoc/>
		public async ValueTask<RpcAgentRequirements?> TryGetFilterRequirementsAsync(IoHash requirementsHash, CancellationToken cancellationToken)
		{
			RpcAgentRequirements? requirements;
			if (!_memoryCache.TryGetValue(requirementsHash, out requirements))
			{
				IDatabase database = _redisService.GetDatabase();
				requirements = await database.StringGetAsync(Keys.Filters[requirementsHash].Requirements);

				if (requirements != null)
				{
					AddRequirementsToCache(requirementsHash, requirements);
				}
			}
			return requirements;
		}

		/// <inheritdoc/>
		public async Task<SessionId[]> GetAllFilteredSessionsAsync(IoHash requirementsHash, CancellationToken cancellationToken = default)
		{
			IDatabase database = _redisService.GetDatabase();
			_ = database.HashSetAsync(Keys.Filters.Touched, requirementsHash, _clock.UtcNow.Ticks, flags: CommandFlags.FireAndForget);
			return await database.SetMembersAsync(Keys.Filters[requirementsHash].Sessions).WaitAsync(cancellationToken);
		}

		#endregion
		#region Stats

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

		#endregion
	}
}
