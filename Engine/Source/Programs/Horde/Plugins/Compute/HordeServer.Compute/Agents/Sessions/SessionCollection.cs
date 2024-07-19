// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Sessions;
using HordeServer.Server;
using HordeServer.Utilities;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace HordeServer.Agents.Sessions
{
	/// <summary>
	/// Collection of session documents
	/// </summary>
	public class SessionCollection : ISessionCollection
	{
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
			public List<string> Properties { get; set; } = new List<string>();
			public Dictionary<string, int> Resources { get; set; } = new Dictionary<string, int>();
			public string Version { get; set; } = String.Empty;

			IReadOnlyList<string> ISession.Properties => Properties;
			IReadOnlyDictionary<string, int> ISession.Resources => Resources;

			[BsonConstructor]
			private SessionDocument()
			{
			}

			public SessionDocument(SessionId id, AgentId agentId, DateTime startTime, IReadOnlyList<string>? properties, IReadOnlyDictionary<string, int>? resources, string? version)
			{
				Id = id;
				AgentId = agentId;
				StartTime = startTime;
				if (properties != null)
				{
					Properties = new List<string>(properties);
				}
				if (resources != null)
				{
					Resources = new Dictionary<string, int>(resources);
				}
				if (version != null)
				{
					Version = version;
				}
			}
		}

		/// <summary>
		/// Collection of session documents
		/// </summary>
		readonly IMongoCollection<SessionDocument> _sessions;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service</param>
		public SessionCollection(IMongoService mongoService)
		{
			List<MongoIndex<SessionDocument>> indexes = new List<MongoIndex<SessionDocument>>();
			indexes.Add(keys => keys.Ascending(x => x.AgentId).Ascending(x => x.StartTime).Ascending(x => x.FinishTime));
			indexes.Add(keys => keys.Ascending(x => x.FinishTime));

			_sessions = mongoService.GetCollection<SessionDocument>("Sessions", indexes);
		}

		/// <inheritdoc/>
		public async Task<ISession> AddAsync(SessionId id, AgentId agentId, DateTime startTime, IReadOnlyList<string>? properties, IReadOnlyDictionary<string, int>? resources, string? version, CancellationToken cancellationToken = default)
		{
			SessionDocument newSession = new SessionDocument(id, agentId, startTime, properties, resources, version);
			await _sessions.InsertOneAsync(newSession, null, cancellationToken);
			return newSession;
		}

		/// <inheritdoc/>
		public async Task<ISession?> GetAsync(SessionId sessionId, CancellationToken cancellationToken = default)
		{
			return await _sessions.Find(x => x.Id == sessionId).FirstOrDefaultAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<List<ISession>> FindAsync(AgentId agentId, DateTime? startTime, DateTime? finishTime, int index, int count, CancellationToken cancellationToken = default)
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

			List<SessionDocument> results = await _sessions.Find(filter).SortByDescending(x => x.StartTime).Skip(index).Limit(count).ToListAsync(cancellationToken);
			return results.ConvertAll<ISession>(x => x);
		}

		/// <inheritdoc/>
		public async Task<List<ISession>> FindActiveSessionsAsync(int? index, int? count, CancellationToken cancellationToken = default)
		{
			List<SessionDocument> results = await _sessions.Find(x => x.FinishTime == null).Range(index, count).ToListAsync(cancellationToken);
			return results.ConvertAll<ISession>(x => x);
		}

		/// <inheritdoc/>
		public Task UpdateAsync(SessionId sessionId, DateTime? finishTime, IReadOnlyList<string>? properties, IReadOnlyDictionary<string, int>? resources, CancellationToken cancellationToken = default)
		{
			List<UpdateDefinition<SessionDocument>> updates = new List<UpdateDefinition<SessionDocument>>();
			if (finishTime != null)
			{
				updates.Add(Builders<SessionDocument>.Update.Set(x => x.FinishTime, finishTime));
			}
			if (properties != null)
			{
				updates.Add(Builders<SessionDocument>.Update.Set(x => x.Properties, new List<string>(properties)));
			}
			if (resources != null)
			{
				updates.Add(Builders<SessionDocument>.Update.Set(x => x.Resources, new Dictionary<string, int>(resources)));
			}
			return _sessions.FindOneAndUpdateAsync(x => x.Id == sessionId, Builders<SessionDocument>.Update.Combine(updates), cancellationToken: cancellationToken);
		}

		/// <inheritdoc/>
		public Task DeleteAsync(SessionId sessionId, CancellationToken cancellationToken = default)
		{
			return _sessions.DeleteOneAsync(x => x.Id == sessionId, null, cancellationToken);
		}
	}
}
