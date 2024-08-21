// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.ObjectModel;
using System.Collections.Concurrent;
using System.Diagnostics.CodeAnalysis;
using System.Linq.Expressions;
using EpicGames.Core;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Agents.Sessions;
using EpicGames.Redis;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;
using HordeCommon.Rpc;
using HordeCommon.Rpc.Messages;
using HordeCommon.Rpc.Tasks;
using HordeServer.Agents.Leases;
using HordeServer.Agents.Sessions;
using HordeServer.Auditing;
using HordeServer.Server;
using Microsoft.Extensions.Logging;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;
using StackExchange.Redis;
using Microsoft.Extensions.Hosting;
using OpenTelemetry.Trace;

namespace HordeServer.Agents
{
	/// <summary>
	/// Collection of agent documents
	/// </summary>
	public sealed class AgentCollection : IAgentCollection, IHostedService, IAsyncDisposable
	{
		class Agent : IAgent
		{
			static IReadOnlyList<string> DefaultProperties { get; } = new List<string>();
			static IReadOnlyDictionary<string, int> DefaultResources { get; } = new Dictionary<string, int>();

			readonly AgentCollection _collection;
			readonly AgentDocument _document;

			AgentId IAgent.Id => _document.Id;
			SessionId? IAgent.SessionId => _document.SessionId;
			DateTime? IAgent.SessionExpiresAt => _document.SessionExpiresAt;
			AgentStatus IAgent.Status => _document.Status;
			DateTime? IAgent.LastStatusChange => _document.LastStatusChange;
			bool IAgent.Enabled => _document.Enabled;
			bool IAgent.Ephemeral => _document.Ephemeral;
			bool IAgent.Deleted => _document.Deleted;
			string? IAgent.Version => _document.Version;
			string? IAgent.Comment => _document.Comment;
			IReadOnlyList<string> IAgent.Properties => _document.Properties ?? DefaultProperties;
			IReadOnlyDictionary<string, int> IAgent.Resources => _document.Resources ?? DefaultResources;
			string? IAgent.LastUpgradeVersion => _document.LastUpgradeVersion;
			DateTime? IAgent.LastUpgradeTime => _document.LastUpgradeTime;
			int? IAgent.UpgradeAttemptCount => _document.UpgradeAttemptCount;
			IReadOnlyList<PoolId> IAgent.Pools => _document.Pools;
			IReadOnlyList<PoolId> IAgent.DynamicPools => _document.DynamicPools;
			IReadOnlyList<PoolId> IAgent.ExplicitPools => _document.ExplicitPools;
			bool IAgent.RequestConform => _document.RequestConform;
			bool IAgent.RequestFullConform => _document.RequestFullConform;
			bool IAgent.RequestRestart => _document.RequestRestart;
			bool IAgent.RequestShutdown => _document.RequestShutdown;
			bool IAgent.RequestForceRestart => _document.RequestForceRestart;
			string? IAgent.LastShutdownReason => _document.LastShutdownReason;
			IReadOnlyList<AgentWorkspaceInfo> IAgent.Workspaces => _document.Workspaces;
			DateTime IAgent.LastConformTime => _document.LastConformTime;
			int? IAgent.ConformAttemptCount => _document.ConformAttemptCount;
			static readonly IReadOnlyList<AgentLease> s_emptyLeases = new List<AgentLease>();
			IReadOnlyList<IAgentLease> IAgent.Leases => _document.Leases ?? s_emptyLeases;
			string IAgent.EnrollmentKey => _document.EnrollmentKey;
			DateTime IAgent.UpdateTime => _document.UpdateTime;
			uint IAgent.UpdateIndex => _document.UpdateIndex;

			public Agent(AgentCollection collection, AgentDocument document)
			{
				_collection = collection;
				_document = document;
			}

			public async Task<ISession?> GetSessionAsync(SessionId sessionId, CancellationToken cancellationToken = default)
			{
				return await _collection.GetSessionAsync(_document.Id, sessionId, cancellationToken);
			}

			public async Task<IReadOnlyList<ISession>> FindSessionsAsync(DateTime? startTime, DateTime? finishTime, int index, int count, CancellationToken cancellationToken = default)
			{
				return await _collection.FindSessionsAsync(_document.Id, startTime, finishTime, index, count, cancellationToken);
			}

			public async Task<IAgent?> TryCreateLeaseAsync(CreateLeaseOptions options, CancellationToken cancellationToken = default)
			{
				AgentDocument? newDocument = await _collection.TryCreateLeaseAsync(_document, options, cancellationToken);
				return _collection.CreateAgentObject(newDocument);
			}

			public async Task<IAgent?> TryCancelLeaseAsync(int leaseIdx, CancellationToken cancellationToken = default)
			{
				AgentDocument? newDocument = await _collection.TryCancelLeaseAsync(_document, leaseIdx, cancellationToken);
				return _collection.CreateAgentObject(newDocument);
			}

			public async Task<IAgent?> TryDeleteAsync(CancellationToken cancellationToken = default)
			{
				AgentDocument? newDocument = await _collection.TryDeleteAsync(_document, cancellationToken);
				return _collection.CreateAgentObject(newDocument);
			}

			public async Task<IAgent?> TryResetAsync(bool ephemeral, string enrollmentKey, CancellationToken cancellationToken = default)
			{
				AgentDocument? newDocument = await _collection.TryResetAsync(_document, ephemeral, enrollmentKey, cancellationToken);
				return _collection.CreateAgentObject(newDocument);
			}

			public async Task<IAgent?> TryCreateSessionAsync(CreateSessionOptions options, CancellationToken cancellationToken = default)
			{
				AgentDocument? newDocument = await _collection.TryCreateSessionAsync(_document, options, cancellationToken);
				return _collection.CreateAgentObject(newDocument);
			}

			public async Task<IAgent?> TryTerminateSessionAsync(CancellationToken cancellationToken = default)
			{
				AgentDocument? newDocument = await _collection.TryTerminateSessionAsync(_document, cancellationToken);
				return _collection.CreateAgentObject(newDocument);
			}

			public async Task<IAgent?> TryUpdateSessionAsync(UpdateSessionOptions options, CancellationToken cancellationToken = default)
			{
				AgentDocument? newDocument = await _collection.TryUpdateSessionAsync(_document, options, cancellationToken);
				return _collection.CreateAgentObject(newDocument);
			}

			public async Task<IAgent?> TryUpdateAsync(UpdateAgentOptions options, CancellationToken cancellationToken = default)
			{
				AgentDocument? newDocument = await _collection.TryUpdateSettingsAsync(_document, options, cancellationToken);
				return _collection.CreateAgentObject(newDocument);
			}

			public async Task<IAgent?> TryUpdateWorkspacesAsync(List<AgentWorkspaceInfo> workspaces, bool requestConform, CancellationToken cancellationToken = default)
			{
				AgentDocument? newDocument = await _collection.TryUpdateWorkspacesAsync(_document, workspaces, requestConform, cancellationToken);
				return _collection.CreateAgentObject(newDocument);
			}

			public async Task<IAgent?> WaitForUpdateAsync(CancellationToken cancellationToken = default)
			{
				AgentDocument? newDocument = await _collection.WaitForUpdateAsync(_document, cancellationToken);
				return _collection.CreateAgentObject(newDocument);
			}
		}

		/// <summary>
		/// Concrete implementation of an agent document
		/// </summary>
		class AgentDocument
		{
			[BsonRequired, BsonId]
			public AgentId Id { get; set; }

			public SessionId? SessionId { get; set; }
			public DateTime? SessionExpiresAt { get; set; }

			public AgentStatus Status { get; set; }
			public DateTime? LastStatusChange { get; set; }

			[BsonRequired]
			public bool Enabled { get; set; } = true;

			public bool Ephemeral { get; set; }

			[BsonIgnoreIfDefault, BsonDefaultValue(false)]
			public bool Deleted { get; set; }

			[BsonElement("Version2")]
			public string? Version { get; set; }

			public List<string>? Properties { get; set; }
			public Dictionary<string, int>? Resources { get; set; }

			[BsonIgnoreIfNull]
			public string? LastUpgradeVersion { get; set; }

			[BsonIgnoreIfNull]
			public DateTime? LastUpgradeTime { get; set; }

			[BsonIgnoreIfNull]
			public int? UpgradeAttemptCount { get; set; }

			public List<PoolId> Pools { get; set; } = new List<PoolId>();
			public List<PoolId> DynamicPools { get; set; } = new List<PoolId>();
			public List<PoolId> ExplicitPools { get; set; } = new List<PoolId>();

			[BsonIgnoreIfDefault, BsonDefaultValue(false)]
			public bool RequestConform { get; set; }

			[BsonIgnoreIfDefault, BsonDefaultValue(false)]
			public bool RequestFullConform { get; set; }

			[BsonIgnoreIfDefault, BsonDefaultValue(false)]
			public bool RequestRestart { get; set; }

			[BsonIgnoreIfDefault, BsonDefaultValue(false)]
			public bool RequestShutdown { get; set; }

			[BsonIgnoreIfDefault, BsonDefaultValue(false)]
			public bool RequestForceRestart { get; set; }

			[BsonIgnoreIfNull]
			public string? LastShutdownReason { get; set; }

			public List<AgentWorkspaceInfo> Workspaces { get; set; } = new List<AgentWorkspaceInfo>();
			public DateTime LastConformTime { get; set; }

			[BsonIgnoreIfNull]
			public int? ConformAttemptCount { get; set; }

			public List<AgentLease>? Leases { get; set; }
			public DateTime UpdateTime { get; set; }
			public uint UpdateIndex { get; set; }
			public string EnrollmentKey { get; set; } = String.Empty;
			public string? Comment { get; set; }

			[BsonElement("dv")]
			public int DocumentVersion { get; set; } = 0;

			[BsonConstructor]
			private AgentDocument()
			{
			}

			public AgentDocument(AgentId id, bool ephemeral, string enrollmentKey)
			{
				Id = id;
				Ephemeral = ephemeral;
				EnrollmentKey = enrollmentKey;
			}
		}

		/// <summary>
		/// Concrete implementation of ISession
		/// </summary>
		class SessionDocument : ISession
		{
			[BsonRequired, BsonId]
			public SessionId Id { get; set; }

			[BsonRequired]
			public AgentId AgentId { get; set; }

			public DateTime StartTime { get; set; }
			public DateTime? FinishTime { get; set; }
			public string Version { get; set; } = String.Empty;

			[BsonConstructor]
			private SessionDocument()
			{
			}

			public SessionDocument(SessionId id, AgentId agentId, DateTime startTime, string? version)
			{
				Id = id;
				AgentId = agentId;
				StartTime = startTime;
				if (version != null)
				{
					Version = version;
				}
			}
		}

		readonly IMongoCollection<AgentDocument> _agentCollection;
		readonly IMongoCollection<SessionDocument> _sessionCollection;
		readonly ILeaseCollection _leaseCollection;
		readonly IAuditLog<AgentId> _auditLog;
		readonly IRedisService _redisService;
		readonly IClock _clock;
		readonly RedisChannel<AgentId> _updateEventChannel;
		readonly ITicker _sharedTicker;
		readonly ConcurrentDictionary<AgentId, TaskCompletionSource> _agentIdToTcs = new ConcurrentDictionary<AgentId, TaskCompletionSource>();
		IAsyncDisposable? _updateEventSubscription;
		readonly Tracer _tracer;
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public AgentCollection(IMongoService mongoService, IRedisService redisService, ILeaseCollection leaseCollection, IClock clock, IAuditLog<AgentId> auditLog, Tracer tracer, ILogger<AgentCollection> logger)
		{
			List<MongoIndex<AgentDocument>> agentIndexes = new List<MongoIndex<AgentDocument>>();
			agentIndexes.Add(keys => keys.Ascending(x => x.Deleted).Ascending(x => x.Id).Ascending(x => x.Pools));
			agentIndexes.Add(keys => keys.Ascending(x => x.SessionExpiresAt), sparse: true);
			_agentCollection = mongoService.GetCollection<AgentDocument>("Agents", agentIndexes);

			List<MongoIndex<SessionDocument>> sessionIndexes = new List<MongoIndex<SessionDocument>>();
			sessionIndexes.Add(keys => keys.Ascending(x => x.AgentId).Ascending(x => x.StartTime).Ascending(x => x.FinishTime));
			sessionIndexes.Add(keys => keys.Ascending(x => x.FinishTime));
			_sessionCollection = mongoService.GetCollection<SessionDocument>("Sessions", sessionIndexes);

			_redisService = redisService;
			_leaseCollection = leaseCollection;
			_clock = clock;
			_updateEventChannel = new RedisChannel<AgentId>(RedisChannel.Literal("agents/notify"));
			_sharedTicker = clock.AddSharedTicker($"{nameof(AgentCollection)}.{nameof(TickSharedAsync)}", TimeSpan.FromSeconds(30.0), TickSharedAsync, logger);
			_auditLog = auditLog;
			_tracer = tracer;
			_logger = logger;
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			await _sharedTicker.DisposeAsync();
			if (_updateEventSubscription != null)
			{
				await _updateEventSubscription.DisposeAsync();
				_updateEventSubscription = null;
			}
		}

		/// <inheritdoc/>
		public async Task StartAsync(CancellationToken cancellationToken)
		{
			_updateEventSubscription = await _redisService.GetDatabase().Multiplexer.SubscribeAsync(_updateEventChannel, OnAgentUpdate);
			await _sharedTicker.StartAsync();
		}

		/// <inheritdoc/>
		public async Task StopAsync(CancellationToken cancellationToken)
		{
			await _sharedTicker.StopAsync();
			if (_updateEventSubscription != null)
			{
				await _updateEventSubscription.DisposeAsync();
				_updateEventSubscription = null;
			}
		}

		internal async ValueTask TickSharedAsync(CancellationToken stoppingToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(AgentService)}.{nameof(TickSharedAsync)}");

			await TerminateExpiredSessionsAsync(stoppingToken);
			await DeleteExpiredEphemeralAgentsAsync(stoppingToken);
		}

		private async Task TerminateExpiredSessionsAsync(CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(AgentService)}.{nameof(TerminateExpiredSessionsAsync)}");

			int c = 0;
			while (!cancellationToken.IsCancellationRequested)
			{
				// Find all the agents which are ready to be expired
				const int MaxAgents = 100;
				DateTime utcNow = _clock.UtcNow;

				FilterDefinition<AgentDocument> filter = Builders<AgentDocument>.Filter.Exists(x => x.SessionExpiresAt) & Builders<AgentDocument>.Filter.Lt(x => x.SessionExpiresAt, utcNow);

				List<AgentDocument> expiredAgents = await _agentCollection.Find(filter).ToListAsync(cancellationToken);
				expiredAgents = await PostLoadAsync(expiredAgents, cancellationToken);

				// Transition each agent to being offline
				foreach (AgentDocument expiredAgent in expiredAgents)
				{
					cancellationToken.ThrowIfCancellationRequested();
					_logger.LogDebug("Terminating session {SessionId} for agent {Agent}", expiredAgent.SessionId, expiredAgent.Id);
					await TryTerminateSessionAsync(expiredAgent, cancellationToken);
				}
				c += expiredAgents.Count;

				// Try again if we didn't fetch everything
				if (expiredAgents.Count < MaxAgents)
				{
					break;
				}
			}
			span.SetAttribute("NumAgentsTerminated", c);
		}

		private async Task DeleteExpiredEphemeralAgentsAsync(CancellationToken cancellationToken)
		{
			using TelemetrySpan span = _tracer.StartActiveSpan($"{nameof(AgentService)}.{nameof(DeleteExpiredEphemeralAgentsAsync)}");
			int c = 0;

			List<AgentDocument> deletedDocuments = await _agentCollection.Find(x => x.Deleted).ToListAsync(cancellationToken);
			deletedDocuments = await PostLoadAsync(deletedDocuments, cancellationToken);

			foreach (AgentDocument agent in deletedDocuments)
			{
				cancellationToken.ThrowIfCancellationRequested();
				bool noStatusChangeDuringPeriod = _clock.UtcNow > agent.LastStatusChange + TimeSpan.FromHours(1);
				if (agent is { Status: AgentStatus.Stopped, Ephemeral: true } && noStatusChangeDuringPeriod)
				{
					_logger.LogDebug("Deleting ephemeral agent {Agent}", agent.Id);
					await ForceDeleteAsync(agent.Id, cancellationToken);
					c++;
				}
			}

			span.SetAttribute("NumAgentsDeleted", c);
		}

		async ValueTask<AgentDocument?> PostLoadAsync(AgentDocument? document, CancellationToken cancellationToken)
		{
			while (document != null)
			{
				AgentDocument? newDocument = null;
				if (document.DocumentVersion == 0)
				{
					UpdateDefinition<AgentDocument> updateDefinition = Builders<AgentDocument>.Update
						.Set(x => x.Pools, CreatePoolsList(document.Pools, document.DynamicPools, document.Properties))
						.Set(x => x.DynamicPools, CreatePoolsList(document.DynamicPools))
						.Set(x => x.ExplicitPools, CreatePoolsList(document.Pools))
						.Set(x => x.DocumentVersion, 1);

					newDocument = await TryUpdateAsync(document, updateDefinition, cancellationToken);
				}
				else
				{
					break;
				}
				document = newDocument ?? await _agentCollection.Find<AgentDocument>(x => x.Id == document.Id).FirstOrDefaultAsync(cancellationToken);
			}
			return document;
		}

		async ValueTask<List<AgentDocument>> PostLoadAsync(List<AgentDocument> documents, CancellationToken cancellationToken)
		{
			List<AgentDocument> newDocuments = new List<AgentDocument>();
			foreach (AgentDocument document in documents)
			{
				AgentDocument? newDocument = await PostLoadAsync(document, cancellationToken);
				if (newDocument != null)
				{
					newDocuments.Add(newDocument);
				}
			}
			return newDocuments;
		}

		[return: NotNullIfNotNull("document")]
		Agent? CreateAgentObject(AgentDocument? document)
		{
			if (document == null)
			{
				return null;
			}
			else
			{
				return new Agent(this, document);
			}
		}

		/// <inheritdoc/>
		public async Task<IAgent> AddAsync(AgentId id, bool ephemeral, string enrollmentKey, CancellationToken cancellationToken)
		{
			AgentDocument agent = new AgentDocument(id, ephemeral, enrollmentKey);
			await _agentCollection.InsertOneAsync(agent, null, cancellationToken);
			return CreateAgentObject(agent);
		}

		/// <inheritdoc/>
		async Task<AgentDocument?> TryResetAsync(AgentDocument agent, bool ephemeral, string enrollmentKey, CancellationToken cancellationToken = default)
		{
			AgentDocument agentDocument = (AgentDocument)agent;

			UpdateDefinition<AgentDocument> update = Builders<AgentDocument>.Update
				.Set(x => x.Ephemeral, ephemeral)
				.Set(x => x.EnrollmentKey, enrollmentKey)
				.Unset(x => x.Deleted)
				.Unset(x => x.SessionId);

			return await TryUpdateAsync(agentDocument, update, cancellationToken);
		}

		/// <inheritdoc/>
		async Task<AgentDocument?> TryDeleteAsync(AgentDocument agent, CancellationToken cancellationToken)
		{
			UpdateDefinition<AgentDocument> update = Builders<AgentDocument>.Update
				.Set(x => x.Deleted, true)
				.Set(x => x.EnrollmentKey, "")
				.Unset(x => x.SessionId);

			return await TryUpdateAsync(agent, update, cancellationToken);
		}

		/// <inheritdoc/>
		async Task ForceDeleteAsync(AgentId agentId, CancellationToken cancellationToken)
		{
			await _sessionCollection.DeleteManyAsync(x => x.AgentId == agentId, cancellationToken);
			await _agentCollection.DeleteOneAsync(x => x.Id == agentId, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IAgent?> GetAsync(AgentId agentId, CancellationToken cancellationToken)
		{
			AgentDocument? document = await GetDocumentAsync(agentId, cancellationToken);
			return CreateAgentObject(document);
		}

		async Task<AgentDocument?> GetDocumentAsync(AgentId agentId, CancellationToken cancellationToken)
		{
			AgentDocument? document = await _agentCollection.Find<AgentDocument>(x => x.Id == agentId).FirstOrDefaultAsync(cancellationToken);
			document = await PostLoadAsync(document, cancellationToken);
			return document;
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<IAgent>> GetManyAsync(List<AgentId> agentIds, CancellationToken cancellationToken)
		{
			List<AgentDocument> documents = await _agentCollection.Find(p => agentIds.Contains(p.Id)).ToListAsync(cancellationToken);
			documents = await PostLoadAsync(documents, cancellationToken);
			return documents.ConvertAll(x => CreateAgentObject(x));
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<IAgent>> FindAsync(PoolId? poolId, DateTime? modifiedAfter, string? property, AgentStatus? status, bool? enabled, bool includeDeleted, int? index, int? count, bool consistentRead, CancellationToken cancellationToken)
		{
			FilterDefinitionBuilder<AgentDocument> filterBuilder = new FilterDefinitionBuilder<AgentDocument>();

			FilterDefinition<AgentDocument> filter = filterBuilder.Empty;
			if (!includeDeleted)
			{
				filter &= filterBuilder.Ne(x => x.Deleted, true);
			}

			if (poolId != null)
			{
				filter &= filterBuilder.AnyEq(x => x.Pools, poolId.Value);
			}

			if (modifiedAfter != null)
			{
				filter &= filterBuilder.Gt(x => x.UpdateTime, modifiedAfter.Value);
			}

			if (property != null)
			{
				filter &= filterBuilder.AnyEq(x => x.Properties, property);
			}

			if (status != null)
			{
				filter &= filterBuilder.Eq(x => x.Status, status.Value);
			}

			if (enabled != null)
			{
				filter &= filterBuilder.Eq(x => x.Enabled, enabled.Value);
			}

			IMongoCollection<AgentDocument> collection = consistentRead ? _agentCollection : _agentCollection.WithReadPreference(ReadPreference.SecondaryPreferred);
			IFindFluent<AgentDocument, AgentDocument> search = collection.Find(filter);
			if (index != null)
			{
				search = search.Skip(index.Value);
			}
			if (count != null)
			{
				search = search.Limit(count.Value);
			}

			List<AgentDocument> documents = await search.ToListAsync(cancellationToken);
			documents = await PostLoadAsync(documents, cancellationToken);
			return documents.ConvertAll(x => CreateAgentObject(x));
		}

		/// <inheritdoc/>
		public async Task<List<LeaseId>> FindActiveLeaseIdsAsync(CancellationToken cancellationToken)
		{
			RedisValue[] activeLeaseIds = await _redisService.GetDatabase().SetMembersAsync(RedisKeyActiveLeaseIds());
			return activeLeaseIds.Select(x => LeaseId.Parse(x.ToString())).ToList();
		}

		/// <inheritdoc/>
		public async Task<List<LeaseId>> GetChildLeaseIdsAsync(LeaseId id, CancellationToken cancellationToken)
		{
			RedisValue[] childIds = await _redisService.GetDatabase().SetMembersAsync(RedisKeyLeaseChildren(id));
			return childIds.Select(x => LeaseId.Parse(x.ToString())).ToList();
		}

		/// <inheritdoc/>
		async Task<SessionDocument?> GetSessionAsync(AgentId agentId, SessionId sessionId, CancellationToken cancellationToken = default)
		{
			FilterDefinitionBuilder<SessionDocument> filterBuilder = Builders<SessionDocument>.Filter;

			FilterDefinition<SessionDocument> filter = filterBuilder.Eq(x => x.Id, sessionId) & filterBuilder.Eq(x => x.AgentId, agentId);
			return await _sessionCollection.Find(filter).FirstOrDefaultAsync(cancellationToken);
		}

		/// <inheritdoc/>
		async Task<List<SessionDocument>> FindSessionsAsync(AgentId agentId, DateTime? startTime, DateTime? finishTime, int index, int count, CancellationToken cancellationToken = default)
		{
			FilterDefinitionBuilder<SessionDocument> filterBuilder = Builders<SessionDocument>.Filter;

			FilterDefinition<SessionDocument> filter = filterBuilder.Eq(x => x.AgentId, agentId);
			if (startTime != null)
			{
				filter &= filterBuilder.Gte(x => x.StartTime, startTime.Value);
			}
			if (finishTime != null)
			{
				filter &= filterBuilder.Or(filterBuilder.Eq(x => x.FinishTime, null), filterBuilder.Lte(x => x.FinishTime, finishTime.Value));
			}

			return await _sessionCollection.Find(filter).SortByDescending(x => x.StartTime).Skip(index).Limit(count).ToListAsync(cancellationToken);
		}

		/// <summary>
		/// Update a single document
		/// </summary>
		/// <param name="current">The document to update</param>
		/// <param name="update">The update definition</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The updated agent document or null if update failed</returns>
		private async Task<AgentDocument?> TryUpdateAsync(AgentDocument current, UpdateDefinition<AgentDocument> update, CancellationToken cancellationToken)
		{
			uint prevUpdateIndex = current.UpdateIndex++;
			current.UpdateTime = DateTime.UtcNow;

			Expression<Func<AgentDocument, bool>> filter = x => x.Id == current.Id && x.UpdateIndex == prevUpdateIndex;
			UpdateDefinition<AgentDocument> updateWithIndex = update.Set(x => x.UpdateIndex, current.UpdateIndex).Set(x => x.UpdateTime, current.UpdateTime);

			AgentDocument? newDocument = await _agentCollection.FindOneAndUpdateAsync<AgentDocument>(filter, updateWithIndex, new FindOneAndUpdateOptions<AgentDocument, AgentDocument> { ReturnDocument = ReturnDocument.After }, cancellationToken);
			if (newDocument == null)
			{
				return null;
			}

			PublishUpdateEvent(newDocument.Id);
			return newDocument;
		}

		/// <inheritdoc/>
		async Task<AgentDocument?> TryUpdateSettingsAsync(AgentDocument agent, UpdateAgentOptions options, CancellationToken cancellationToken)
		{
			// Update the database
			UpdateDefinitionBuilder<AgentDocument> updateBuilder = new UpdateDefinitionBuilder<AgentDocument>();

			List<UpdateDefinition<AgentDocument>> updates = new List<UpdateDefinition<AgentDocument>>();
			if (options.ExplicitPools != null)
			{
				List<PoolId> pools = CreatePoolsList(agent.DynamicPools, options.ExplicitPools, agent.Properties);
				updates.Add(updateBuilder.Set(x => x.Pools, pools));

				List<PoolId> explicitPools = CreatePoolsList(options.ExplicitPools).ToList();
				updates.Add(updateBuilder.Set(x => x.ExplicitPools, explicitPools));
			}
			if (options.Enabled != null)
			{
				updates.Add(updateBuilder.Set(x => x.Enabled, options.Enabled.Value));
			}
			if (options.RequestConform != null)
			{
				updates.Add(updateBuilder.Set(x => x.RequestConform, options.RequestConform.Value));
				updates.Add(updateBuilder.Unset(x => x.ConformAttemptCount));
			}
			if (options.RequestFullConform != null)
			{
				updates.Add(updateBuilder.Set(x => x.RequestFullConform, options.RequestFullConform.Value));
				updates.Add(updateBuilder.Unset(x => x.ConformAttemptCount));
			}
			if (options.RequestRestart != null)
			{
				if (options.RequestRestart.Value)
				{
					updates.Add(updateBuilder.Set(x => x.RequestRestart, true));
				}
				else
				{
					updates.Add(updateBuilder.Unset(x => x.RequestRestart));
				}
			}
			if (options.RequestShutdown != null)
			{
				if (options.RequestShutdown.Value)
				{
					updates.Add(updateBuilder.Set(x => x.RequestShutdown, true));
				}
				else
				{
					updates.Add(updateBuilder.Unset(x => x.RequestShutdown));
				}
			}
			if (options.RequestForceRestart != null)
			{
				if (options.RequestForceRestart.Value)
				{
					updates.Add(updateBuilder.Set(x => x.RequestForceRestart, true));
				}
				else
				{
					updates.Add(updateBuilder.Unset(x => x.RequestForceRestart));
				}
			}
			if (options.ShutdownReason != null)
			{
				updates.Add(updateBuilder.Set(x => x.LastShutdownReason, options.ShutdownReason));
			}
			if (options.Comment != null)
			{
				updates.Add(updateBuilder.Set(x => x.Comment, options.Comment));
			}

			// Apply the update
			return await TryUpdateAsync(agent, updateBuilder.Combine(updates), cancellationToken);
		}

		/// <inheritdoc/>
		async Task<AgentDocument?> TryUpdateSessionAsync(AgentDocument agent, UpdateSessionOptions options, CancellationToken cancellationToken)
		{
			// Create an update definition for the agent
			UpdateDefinitionBuilder<AgentDocument> updateBuilder = Builders<AgentDocument>.Update;
			List<UpdateDefinition<AgentDocument>> updates = new List<UpdateDefinition<AgentDocument>>();

			if (options.Status != null && agent.Status != options.Status.Value)
			{
				updates.Add(updateBuilder.Set(x => x.Status, options.Status.Value));
				updates.Add(updateBuilder.Set(x => x.LastStatusChange, _clock.UtcNow));
			}
			if (options.SessionExpiresAt != null)
			{
				updates.Add(updateBuilder.Set(x => x.SessionExpiresAt, options.SessionExpiresAt.Value));
			}

			List<string> newProperties = agent.Properties ?? new List<string>();
			if (options.Capabilities != null)
			{
				options.Capabilities.Flatten(out newProperties, out Dictionary<string, int> newResources);

				if (!ResourcesEqual(newResources, agent.Resources))
				{
					updates.Add(updateBuilder.Set(x => x.Resources, newResources));
				}
			}
			if (options.DynamicPools != null)
			{
				List<PoolId> dynamicPools = CreatePoolsList(options.DynamicPools).ToList();
				if (!Enumerable.SequenceEqual(dynamicPools, agent.DynamicPools))
				{
					updates.Add(updateBuilder.Set(x => x.DynamicPools, dynamicPools));
				}
			}
			if (options.Leases != null)
			{
				// Flag for whether the leases array should be updated
				bool updateLeases = false;
				Dictionary<LeaseId, RpcLease> idToRemoteLease = options.Leases.ToDictionary(x => x.Id, x => x);

				// Remove any completed leases from the agent
				List<AgentLease> leases = agent.Leases?.ConvertAll(x => new AgentLease(x)) ?? new List<AgentLease>();
				for (int idx = 0; idx < leases.Count; idx++)
				{
					AgentLease lease = leases[idx];
					if (!idToRemoteLease.TryGetValue(lease.Id, out RpcLease? remoteLease))
					{
						if (lease.State != LeaseState.Pending)
						{
							leases.RemoveAt(idx--);
							updateLeases = true;
						}
					}
					else
					{
						if (remoteLease.State == RpcLeaseState.Cancelled || remoteLease.State == RpcLeaseState.Completed)
						{
							leases.RemoveAt(idx--);
							updateLeases = true;
						}
						else if (remoteLease.State == RpcLeaseState.Active && lease.State == LeaseState.Pending)
						{
							lease.State = LeaseState.Active;
							updateLeases = true;
						}
					}
				}

				// If the agent is stopping, cancel all the leases. Clear out the current session once it's complete.
				AgentStatus status = options.Status ?? agent.Status;
				if (status == AgentStatus.Stopping || status == AgentStatus.Busy)
				{
					foreach (AgentLease lease in leases)
					{
						if (lease.State != LeaseState.Cancelled)
						{
							lease.State = LeaseState.Cancelled;
							updateLeases = true;
						}
					}
				}

				if (updateLeases)
				{
					updates.Add(updateBuilder.Set(x => x.Leases, leases));
				}
			}

			// Update the pools
			List<PoolId> pools = CreatePoolsList(options.DynamicPools ?? agent.DynamicPools, agent.ExplicitPools, newProperties);
			if (!Enumerable.SequenceEqual(pools, agent.Pools))
			{
				updates.Add(updateBuilder.Set(x => x.Pools, pools));
			}

			SetPoolProperties(newProperties, pools);

			if (!Enumerable.SequenceEqual(agent.Properties ?? Enumerable.Empty<string>(), newProperties, StringComparer.Ordinal))
			{
				updates.Add(updateBuilder.Set(x => x.Properties, newProperties));
			}

			// If there are no new updates, return immediately. This is important for preventing UpdateSession calls from returning immediately.
			if (updates.Count == 0)
			{
				return agent;
			}

			// Update the agent, and try to create new lease documents if we succeed
			AgentDocument? newAgent = await TryUpdateAsync(agent, updateBuilder.Combine(updates), cancellationToken);
			if (newAgent == null)
			{
				return null;
			}

			// Remove any complete leases
			if (agent.Leases != null)
			{
				HashSet<LeaseId> newLeaseIds = new HashSet<LeaseId>(newAgent.Leases?.Select(x => x.Id) ?? Enumerable.Empty<LeaseId>());
				foreach (AgentLease oldLease in agent.Leases.Where(x => !newLeaseIds.Contains(x.Id)))
				{
					await RemoveActiveLeaseAsync(oldLease);

					RpcLease? rpcLease = options.Leases?.FirstOrDefault(x => x.Id == oldLease.Id);
					if (rpcLease != null && rpcLease.State == RpcLeaseState.Completed)
					{
						await _leaseCollection.TrySetOutcomeAsync(oldLease.Id, _clock.UtcNow, LeaseOutcome.Success, rpcLease.Output.ToArray(), CancellationToken.None);
					}
					else
					{
						await _leaseCollection.TrySetOutcomeAsync(oldLease.Id, _clock.UtcNow, LeaseOutcome.Cancelled, null, CancellationToken.None);
					}
				}
			}

			return newAgent;
		}

		static void SetPoolProperties(List<string> properties, List<PoolId> pools)
		{
			properties.RemoveAll(x => x.StartsWith($"{KnownPropertyNames.Pool}=", StringComparison.OrdinalIgnoreCase));
			properties.AddRange(pools.Select(x => $"{KnownPropertyNames.Pool}={x}"));
			properties.Sort(StringComparer.OrdinalIgnoreCase);
		}

		static List<PoolId> CreatePoolsList(IEnumerable<PoolId> pools)
			=> pools.Distinct().OrderBy(x => x.Id.Text).ToList();

		static List<PoolId> CreatePoolsList(IEnumerable<PoolId> dynamicPools, IEnumerable<PoolId> explicitPools, IEnumerable<string>? properties)
		{
			List<PoolId> pools = new List<PoolId>();
			pools.AddRange(dynamicPools);
			pools.AddRange(explicitPools);

			if (properties != null)
			{
				foreach (string property in properties)
				{
					const string Key = KnownPropertyNames.RequestedPools + "=";
					if (property.StartsWith(Key, StringComparison.Ordinal))
					{
						try
						{
							pools.AddRange(property[Key.Length..].Split(",").Select(x => new PoolId(x)));
						}
						catch
						{
							// Ignored
						}
					}
				}
			}

			return CreatePoolsList(pools);
		}

		static bool ResourcesEqual(IReadOnlyDictionary<string, int>? dictA, IReadOnlyDictionary<string, int>? dictB)
		{
			dictA ??= ReadOnlyDictionary<string, int>.Empty;
			dictB ??= ReadOnlyDictionary<string, int>.Empty;

			if (dictA.Count != dictB.Count)
			{
				return false;
			}

			foreach (KeyValuePair<string, int> pair in dictA)
			{
				int value;
				if (!dictB.TryGetValue(pair.Key, out value) || value != pair.Value)
				{
					return false;
				}
			}

			return true;
		}

		/// <inheritdoc/>
		async Task<AgentDocument?> TryUpdateWorkspacesAsync(AgentDocument agent, List<AgentWorkspaceInfo> workspaces, bool requestConform, CancellationToken cancellationToken)
		{
			DateTime lastConformTime = DateTime.UtcNow;

			// Set the new workspaces
			UpdateDefinition<AgentDocument> update = Builders<AgentDocument>.Update.Set(x => x.Workspaces, workspaces);
			update = update.Set(x => x.LastConformTime, lastConformTime);
			update = update.Unset(x => x.ConformAttemptCount);
			if (!requestConform)
			{
				update = update.Unset(x => x.RequestConform);
				update = update.Unset(x => x.RequestFullConform);
			}

			// Update the agent
			return await TryUpdateAsync(agent, update, cancellationToken);
		}

		/// <inheritdoc/>
		async Task<AgentDocument?> TryCreateSessionAsync(AgentDocument agent, CreateSessionOptions options, CancellationToken cancellationToken)
		{
			options.Capabilities.Flatten(out List<string> newProperties, out Dictionary<string, int> newResources);

			List<PoolId> newDynamicPools = new(options.DynamicPools);
			List<PoolId> newPools = CreatePoolsList(agent.ExplicitPools, newDynamicPools, newProperties);
			SetPoolProperties(newProperties, newPools);

			// Create a new session document
			SessionDocument newSession = new SessionDocument(SessionIdUtils.GenerateNewId(), agent.Id, _clock.UtcNow, options.Version);
			await _sessionCollection.InsertOneAsync(newSession, null, cancellationToken);

			try
			{
				// Reset the agent to use the new session
				UpdateDefinitionBuilder<AgentDocument> updateBuilder = Builders<AgentDocument>.Update;

				List<UpdateDefinition<AgentDocument>> updates = new List<UpdateDefinition<AgentDocument>>();
				updates.Add(updateBuilder.Set(x => x.SessionId, newSession.Id));
				updates.Add(updateBuilder.Set(x => x.SessionExpiresAt, newSession.StartTime + AgentService.SessionExpiryTime));
				updates.Add(updateBuilder.Set(x => x.Status, options.Status));
				updates.Add(updateBuilder.Unset(x => x.Leases));
				updates.Add(updateBuilder.Unset(x => x.Deleted));
				updates.Add(updateBuilder.Set(x => x.Properties, newProperties));
				updates.Add(updateBuilder.Set(x => x.Resources, newResources));
				updates.Add(updateBuilder.Set(x => x.Pools, newPools));
				updates.Add(updateBuilder.Set(x => x.DynamicPools, newDynamicPools));
				updates.Add(updateBuilder.Set(x => x.Version, options.Version));
				updates.Add(updateBuilder.Unset(x => x.RequestRestart));
				updates.Add(updateBuilder.Unset(x => x.RequestShutdown));
				updates.Add(updateBuilder.Unset(x => x.RequestForceRestart));
				updates.Add(updateBuilder.Set(x => x.LastShutdownReason, "Unexpected"));

				if (String.Equals(options.Version, agent.LastUpgradeVersion, StringComparison.Ordinal))
				{
					updates.Add(updateBuilder.Unset(x => x.UpgradeAttemptCount));
				}

				if (agent.Status != options.Status)
				{
					updates.Add(updateBuilder.Set(x => x.LastStatusChange, options.LastStatusChange));
				}

				foreach (AgentLease agentLease in agent.Leases ?? Enumerable.Empty<AgentLease>())
				{
					await RemoveActiveLeaseAsync(agentLease);
				}

				// Apply the update
				AgentDocument? newAgent = await TryUpdateAsync(agent, updateBuilder.Combine(updates), cancellationToken);
				if (newAgent == null)
				{
					await _sessionCollection.DeleteOneAsync(x => x.Id == newSession.Id, CancellationToken.None);
					return null;
				}

				return newAgent;
			}
			catch
			{
				await _sessionCollection.DeleteOneAsync(x => x.Id == newSession.Id, CancellationToken.None);
				throw;
			}
		}

		/// <inheritdoc/>
		async Task<AgentDocument?> TryTerminateSessionAsync(AgentDocument agent, CancellationToken cancellationToken)
		{
			// Get the time that the session finishes at
			DateTime finishTime = _clock.UtcNow;
			if (agent.SessionExpiresAt.HasValue && agent.SessionExpiresAt.Value < finishTime)
			{
				finishTime = agent.SessionExpiresAt.Value;
			}

			UpdateDefinition<AgentDocument> update = new BsonDocument();

			update = update.Unset(x => x.SessionId);
			update = update.Unset(x => x.SessionExpiresAt);
			update = update.Unset(x => x.Leases);
			update = update.Set(x => x.Status, AgentStatus.Stopped);
			update = update.Set(x => x.LastStatusChange, finishTime);

			if (agent.Ephemeral)
			{
				update = update.Set(x => x.Deleted, true);
			}

			AgentDocument? newAgent = await TryUpdateAsync(agent, update, cancellationToken);
			if (newAgent == null)
			{
				return null;
			}

			// Update the session document
			ILogger agentLogger = GetLogger(agent.Id);
			agentLogger.LogInformation("Terminated session {SessionId}", agent.SessionId);
			await _sessionCollection.UpdateOneAsync(x => x.Id == agent.SessionId, Builders<SessionDocument>.Update.Set(x => x.FinishTime, finishTime), cancellationToken: CancellationToken.None);

			// Remove any outstanding leases
			if (agent.Leases != null)
			{
				foreach (AgentLease agentLease in agent.Leases)
				{
					agentLogger.LogInformation("Removing lease {LeaseId} during session terminate...", agentLease.Id);

					// Remove it from the active list used for autoscaling
					await RemoveActiveLeaseAsync(agentLease);

					// Update the lease
					await _leaseCollection.TrySetOutcomeAsync(agentLease.Id, finishTime, LeaseOutcome.Failed, null, CancellationToken.None);
				}
			}

			return newAgent;
		}

		private static string RedisKeyActiveLeaseIds() => $"agent/active-lease-id";
		private static string RedisKeyLeaseChildren(LeaseId parentId) => $"agent/lease-children/{parentId.ToString()}";

		/// <inheritdoc/>
		async Task<AgentDocument?> TryCreateLeaseAsync(AgentDocument agent, CreateLeaseOptions options, CancellationToken cancellationToken)
		{
			AgentLease newLease = new AgentLease(options);

			List<AgentLease> leases = new List<AgentLease>();
			if (agent.Leases != null)
			{
				leases.AddRange(agent.Leases);
			}
			leases.Add(newLease);

			List<UpdateDefinition<AgentDocument>> updates = new List<UpdateDefinition<AgentDocument>>();
			updates.Add(Builders<AgentDocument>.Update.Set(x => x.Leases, leases));
			GetNewLeaseUpdates(agent, newLease.Payload, updates);

			UpdateDefinition<AgentDocument> update = Builders<AgentDocument>.Update.Combine(updates);

			AgentDocument? updatedDoc = await TryUpdateAsync(agent, update, cancellationToken);
			if (updatedDoc == null)
			{
				return null;
			}

			await AddActiveLeaseAsync(newLease);

			try
			{
				DateTime startTime = _clock.UtcNow;
				await _leaseCollection.AddAsync(options.Id, options.ParentId, options.Name, agent.Id, agent.SessionId!.Value, options.StreamId, options.PoolId, options.LogId, startTime, Any.Pack(options.Payload).ToByteArray(), cancellationToken);
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Unable to create lease {LeaseId} for agent {AgentId}; lease already exists?", options.Id, agent.Id);
			}

			return updatedDoc;
		}

		private async Task AddActiveLeaseAsync(AgentLease lease)
		{
			IDatabase redis = _redisService.GetDatabase();

			await redis.SetAddAsync(RedisKeyActiveLeaseIds(), lease.Id.ToString());
			await redis.KeyExpireAsync(RedisKeyActiveLeaseIds(), TimeSpan.FromHours(36));

			if (lease.ParentId != null)
			{
				await redis.SetAddAsync(RedisKeyLeaseChildren(lease.ParentId.Value), lease.Id.ToString());
				await redis.KeyExpireAsync(RedisKeyLeaseChildren(lease.ParentId.Value), TimeSpan.FromHours(36));
			}
		}

		private async Task RemoveActiveLeaseAsync(AgentLease lease)
		{
			IDatabase redis = _redisService.GetDatabase();
			await redis.SetRemoveAsync(RedisKeyActiveLeaseIds(), lease.Id.ToString());

			if (lease.ParentId != null)
			{
				await redis.SetRemoveAsync(RedisKeyLeaseChildren(lease.ParentId.Value), lease.Id.ToString());
			}
		}

		static void GetNewLeaseUpdates(AgentDocument agent, ReadOnlySpan<byte> payloadData, List<UpdateDefinition<AgentDocument>> updates)
		{
			Any payload = Any.Parser.ParseFrom(payloadData);
			if (payload.TryUnpack(out ConformTask conformTask))
			{
				int newConformAttemptCount = (agent.ConformAttemptCount ?? 0) + 1;
				updates.Add(Builders<AgentDocument>.Update.Set(x => x.ConformAttemptCount, newConformAttemptCount));
				updates.Add(Builders<AgentDocument>.Update.Set(x => x.LastConformTime, DateTime.UtcNow));
			}
			else if (payload.TryUnpack(out UpgradeTask upgradeTask))
			{
				string newVersion = upgradeTask.SoftwareId;

				int versionIdx = newVersion.IndexOf(':', StringComparison.Ordinal);
				if (versionIdx != -1)
				{
					newVersion = newVersion.Substring(versionIdx + 1);
				}

				int newUpgradeAttemptCount = (agent.UpgradeAttemptCount ?? 0) + 1;
				updates.Add(Builders<AgentDocument>.Update.Set(x => x.LastUpgradeVersion, newVersion));
				updates.Add(Builders<AgentDocument>.Update.Set(x => x.UpgradeAttemptCount, newUpgradeAttemptCount));
				updates.Add(Builders<AgentDocument>.Update.Set(x => x.LastUpgradeTime, DateTime.UtcNow));
			}
		}

		/// <inheritdoc/>
		async Task<AgentDocument?> TryCancelLeaseAsync(AgentDocument agent, int leaseIdx, CancellationToken cancellationToken)
		{
			UpdateDefinition<AgentDocument> update = Builders<AgentDocument>.Update.Set(x => x.Leases![leaseIdx].State, LeaseState.Cancelled);
			AgentDocument? newAgent = await TryUpdateAsync(agent, update, cancellationToken);
			if (newAgent != null)
			{
				PublishUpdateEvent(agent.Id);
				if (agent.Leases != null && leaseIdx < agent.Leases.Count)
				{
					await RemoveActiveLeaseAsync(agent.Leases[leaseIdx]);
				}
			}
			return newAgent;
		}

		/// <inheritdoc/>
		void PublishUpdateEvent(AgentId agentId)
		{
			_ = _redisService.GetDatabase().PublishAsync(_updateEventChannel, agentId, CommandFlags.FireAndForget);
		}

		void OnAgentUpdate(AgentId agentId)
		{
			if (_agentIdToTcs.TryGetValue(agentId, out TaskCompletionSource? tcs))
			{
				tcs.TrySetResult();
			}
		}

		async Task<AgentDocument?> WaitForUpdateAsync(AgentDocument agent, CancellationToken cancellationToken)
		{
			TaskCompletionSource newTcs = new TaskCompletionSource();
			try
			{
				TaskCompletionSource tcs = _agentIdToTcs.GetOrAdd(agent.Id, newTcs);

				AgentDocument? newAgent = await GetDocumentAsync(agent.Id, cancellationToken);
				if (newAgent != null && newAgent.UpdateIndex == agent.UpdateIndex)
				{
					await tcs.Task.WaitAsync(cancellationToken);
					newAgent = await GetDocumentAsync(agent.Id, cancellationToken);
				}

				return newAgent;
			}
			finally
			{
				newTcs.TrySetResult();
				_agentIdToTcs.TryRemove(new KeyValuePair<AgentId, TaskCompletionSource>(agent.Id, newTcs));
			}
		}

		/// <inheritdoc/>
		public IAuditLogChannel<AgentId> GetLogger(AgentId agentId)
		{
			return _auditLog[agentId];
		}
	}
}
