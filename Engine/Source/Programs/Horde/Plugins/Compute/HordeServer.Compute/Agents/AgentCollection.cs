// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.ObjectModel;
using System.Diagnostics.CodeAnalysis;
using System.Linq.Expressions;
using EpicGames.Core;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Agents.Sessions;
using EpicGames.Redis;
using Google.Protobuf.WellKnownTypes;
using HordeCommon.Rpc.Tasks;
using HordeServer.Auditing;
using HordeServer.Server;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;
using StackExchange.Redis;

namespace HordeServer.Agents
{
	/// <summary>
	/// Collection of agent documents
	/// </summary>
	public class AgentCollection : IAgentCollection
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
			IReadOnlyList<AgentLease> IAgent.Leases => _document.Leases ?? s_emptyLeases;
			string IAgent.EnrollmentKey => _document.EnrollmentKey;
			DateTime IAgent.UpdateTime => _document.UpdateTime;
			uint IAgent.UpdateIndex => _document.UpdateIndex;

			public Agent(AgentCollection collection, AgentDocument document)
			{
				_collection = collection;
				_document = document;
			}

			public async Task<IAgent?> TryAddLeaseAsync(AgentLease newLease, CancellationToken cancellationToken = default)
			{
				AgentDocument? newDocument = await _collection.TryAddLeaseAsync(_document, newLease, cancellationToken);
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

		readonly IMongoCollection<AgentDocument> _agents;
		readonly IAuditLog<AgentId> _auditLog;
		readonly IRedisService _redisService;
		readonly IClock _clock;
		readonly RedisChannel<AgentId> _updateEventChannel;

		/// <summary>
		/// Constructor
		/// </summary>
		public AgentCollection(IMongoService mongoService, IRedisService redisService, IClock clock, IAuditLog<AgentId> auditLog)
		{
			List<MongoIndex<AgentDocument>> indexes = new List<MongoIndex<AgentDocument>>();
			indexes.Add(keys => keys.Ascending(x => x.Deleted).Ascending(x => x.Id).Ascending(x => x.Pools));
			indexes.Add(keys => keys.Ascending(x => x.SessionExpiresAt), sparse: true);

			_agents = mongoService.GetCollection<AgentDocument>("Agents", indexes);
			_redisService = redisService;
			_clock = clock;
			_updateEventChannel = new RedisChannel<AgentId>(RedisChannel.Literal("agents/notify"));
			_auditLog = auditLog;
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
				document = newDocument ?? await _agents.Find<AgentDocument>(x => x.Id == document.Id).FirstOrDefaultAsync(cancellationToken);
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
			await _agents.InsertOneAsync(agent, null, cancellationToken);
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

			AgentDocument? newAgent = await TryUpdateAsync(agentDocument, update, cancellationToken);
			if (newAgent != null)
			{
				await PublishUpdateEventAsync(agent.Id);
			}
			return newAgent;
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
		public async Task ForceDeleteAsync(AgentId agentId, CancellationToken cancellationToken)
		{
			await _agents.DeleteOneAsync(x => x.Id == agentId, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IAgent?> GetAsync(AgentId agentId, CancellationToken cancellationToken)
		{
			AgentDocument? document = await _agents.Find<AgentDocument>(x => x.Id == agentId).FirstOrDefaultAsync(cancellationToken);
			document = await PostLoadAsync(document, cancellationToken);
			return CreateAgentObject(document);
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<IAgent>> GetManyAsync(List<AgentId> agentIds, CancellationToken cancellationToken)
		{
			List<AgentDocument> documents = await _agents.Find(p => agentIds.Contains(p.Id)).ToListAsync(cancellationToken);
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

			IMongoCollection<AgentDocument> collection = consistentRead ? _agents : _agents.WithReadPreference(ReadPreference.SecondaryPreferred);
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
		public async Task<IReadOnlyList<IAgent>> FindExpiredAsync(DateTime utcNow, int maxAgents, CancellationToken cancellationToken)
		{
			FilterDefinition<AgentDocument> filter = Builders<AgentDocument>.Filter.Exists(x => x.SessionExpiresAt) & Builders<AgentDocument>.Filter.Lt(x => x.SessionExpiresAt, utcNow);

			List<AgentDocument> documents = await _agents.Find(filter).Limit(maxAgents).ToListAsync(cancellationToken);
			documents = await PostLoadAsync(documents, cancellationToken);
			return documents.ConvertAll(x => CreateAgentObject(x));
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyList<IAgent>> FindDeletedAsync(CancellationToken cancellationToken)
		{
			List<AgentDocument> documents = await _agents.Find(x => x.Deleted).ToListAsync(cancellationToken);
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

			return await _agents.FindOneAndUpdateAsync<AgentDocument>(filter, updateWithIndex, new FindOneAndUpdateOptions<AgentDocument, AgentDocument> { ReturnDocument = ReturnDocument.After }, cancellationToken);
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
			AgentDocument? newAgent = await TryUpdateAsync(agent, updateBuilder.Combine(updates), cancellationToken);
			if (newAgent != null)
			{
				if (newAgent.RequestRestart != agent.RequestRestart || newAgent.RequestConform != agent.RequestConform || newAgent.RequestShutdown != agent.RequestShutdown || newAgent.RequestForceRestart != agent.RequestForceRestart)
				{
					await PublishUpdateEventAsync(agent.Id);
				}
			}
			return newAgent;
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
			if (options.Properties != null)
			{
				List<string> newProperties = options.Properties.OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();
				if (!(agent.Properties ?? Enumerable.Empty<string>()).SequenceEqual(newProperties, StringComparer.Ordinal))
				{
					updates.Add(updateBuilder.Set(x => x.Properties, newProperties));
				}
			}
			if (options.Resources != null && !ResourcesEqual(options.Resources, agent.Resources))
			{
				updates.Add(updateBuilder.Set(x => x.Resources, new Dictionary<string, int>(options.Resources)));
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
				foreach (AgentLease lease in options.Leases)
				{
					if (lease.Payload != null && (agent.Leases == null || !agent.Leases.Any(x => x.Id == lease.Id)))
					{
						GetNewLeaseUpdates(agent, lease, updates);
					}
				}

				List<AgentLease> currentLeases = agent.Leases ?? new List<AgentLease>();
				List<AgentLease> newLeases = options.Leases;
				List<AgentLease> leasesToAdd = newLeases.Where(nl => currentLeases.All(cl => cl.Id != nl.Id)).ToList();
				List<AgentLease> leasesToRemove = currentLeases.Where(cl => newLeases.All(nl => nl.Id != cl.Id)).ToList();

				foreach (AgentLease lease in leasesToAdd)
				{
					await AddActiveLeaseAsync(lease);
				}

				foreach (AgentLease lease in leasesToRemove)
				{
					await RemoveActiveLeaseAsync(lease);
				}

				updates.Add(updateBuilder.Set(x => x.Leases, options.Leases));
			}

			// Update the pools
			List<PoolId> pools = CreatePoolsList(options.DynamicPools ?? agent.DynamicPools, agent.ExplicitPools, options.Properties ?? agent.Properties ?? Enumerable.Empty<string>());
			if (!Enumerable.SequenceEqual(pools, agent.Pools))
			{
				updates.Add(updateBuilder.Set(x => x.Pools, pools));
			}

			// If there are no new updates, return immediately. This is important for preventing UpdateSession calls from returning immediately.
			if (updates.Count == 0)
			{
				return agent;
			}

			// Update the agent, and try to create new lease documents if we succeed
			return await TryUpdateAsync(agent, updateBuilder.Combine(updates), cancellationToken);
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
			List<string> newProperties = options.Properties.OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();
			Dictionary<string, int> newResources = new(options.Resources);
			List<PoolId> newDynamicPools = new(options.DynamicPools);

			// Reset the agent to use the new session
			UpdateDefinitionBuilder<AgentDocument> updateBuilder = Builders<AgentDocument>.Update;

			List<UpdateDefinition<AgentDocument>> updates = new List<UpdateDefinition<AgentDocument>>();
			updates.Add(updateBuilder.Set(x => x.SessionId, options.SessionId));
			updates.Add(updateBuilder.Set(x => x.SessionExpiresAt, options.SessionExpiresAt));
			updates.Add(updateBuilder.Set(x => x.Status, options.Status));
			updates.Add(updateBuilder.Unset(x => x.Leases));
			updates.Add(updateBuilder.Unset(x => x.Deleted));
			updates.Add(updateBuilder.Set(x => x.Properties, newProperties));
			updates.Add(updateBuilder.Set(x => x.Resources, newResources));
			updates.Add(updateBuilder.Set(x => x.Pools, CreatePoolsList(agent.ExplicitPools, newDynamicPools, newProperties)));
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
			return await TryUpdateAsync(agent, updateBuilder.Combine(updates), cancellationToken);
		}

		/// <inheritdoc/>
		async Task<AgentDocument?> TryTerminateSessionAsync(AgentDocument agent, CancellationToken cancellationToken)
		{
			UpdateDefinition<AgentDocument> update = new BsonDocument();

			update = update.Unset(x => x.SessionId);
			update = update.Unset(x => x.SessionExpiresAt);
			update = update.Unset(x => x.Leases);
			update = update.Set(x => x.Status, AgentStatus.Stopped);
			update = update.Set(x => x.LastStatusChange, _clock.UtcNow);

			if (agent.Ephemeral)
			{
				update = update.Set(x => x.Deleted, true);
			}

			foreach (AgentLease agentLease in agent.Leases ?? Enumerable.Empty<AgentLease>())
			{
				await RemoveActiveLeaseAsync(agentLease);
			}

			return await TryUpdateAsync(agent, update, cancellationToken);
		}

		private static string RedisKeyActiveLeaseIds() => $"agent/active-lease-id";
		private static string RedisKeyLeaseChildren(LeaseId parentId) => $"agent/lease-children/{parentId.ToString()}";

		/// <inheritdoc/>
		async Task<AgentDocument?> TryAddLeaseAsync(AgentDocument agent, AgentLease newLease, CancellationToken cancellationToken)
		{
			List<AgentLease> leases = new List<AgentLease>();
			if (agent.Leases != null)
			{
				leases.AddRange(agent.Leases);
			}
			leases.Add(newLease);

			List<UpdateDefinition<AgentDocument>> updates = new List<UpdateDefinition<AgentDocument>>();
			updates.Add(Builders<AgentDocument>.Update.Set(x => x.Leases, leases));
			GetNewLeaseUpdates(agent, newLease, updates);

			UpdateDefinition<AgentDocument> update = Builders<AgentDocument>.Update.Combine(updates);
			AgentDocument? updatedDoc = await TryUpdateAsync(agent, update, cancellationToken);

			if (updatedDoc != null)
			{
				await AddActiveLeaseAsync(newLease);
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

		static void GetNewLeaseUpdates(AgentDocument agent, AgentLease lease, List<UpdateDefinition<AgentDocument>> updates)
		{
			if (lease.Payload != null)
			{
				Any payload = Any.Parser.ParseFrom(lease.Payload.ToArray());
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
		}

		/// <inheritdoc/>
		async Task<AgentDocument?> TryCancelLeaseAsync(AgentDocument agent, int leaseIdx, CancellationToken cancellationToken)
		{
			UpdateDefinition<AgentDocument> update = Builders<AgentDocument>.Update.Set(x => x.Leases![leaseIdx].State, LeaseState.Cancelled);
			AgentDocument? newAgent = await TryUpdateAsync(agent, update, cancellationToken);
			if (newAgent != null)
			{
				await PublishUpdateEventAsync(agent.Id);
				if (agent.Leases != null && leaseIdx < agent.Leases.Count)
				{
					await RemoveActiveLeaseAsync(agent.Leases[leaseIdx]);
				}
			}
			return newAgent;
		}

		/// <inheritdoc/>
		public IAuditLogChannel<AgentId> GetLogger(AgentId agentId)
		{
			return _auditLog[agentId];
		}

		/// <inheritdoc/>
		public Task PublishUpdateEventAsync(AgentId agentId)
		{
			return _redisService.GetDatabase().PublishAsync(_updateEventChannel, agentId);
		}

		/// <inheritdoc/>
		public async Task<IAsyncDisposable> SubscribeToUpdateEventsAsync(Action<AgentId> onUpdate)
		{
			return await _redisService.GetDatabase().Multiplexer.SubscribeAsync(_updateEventChannel, onUpdate);
		}
	}
}
