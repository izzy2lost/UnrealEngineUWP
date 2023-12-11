// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Artifacts;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Streams;
using Horde.Server.Acls;
using Horde.Server.Server;
using Horde.Server.Storage;
using Horde.Server.Utilities;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Server.Artifacts
{
	/// <summary>
	/// Implementation of <see cref="IArtifactCollection"/>
	/// </summary>
	class ArtifactCollection : IArtifactCollection
	{
		class Artifact : IArtifact
		{
			[BsonRequired, BsonId]
			public ArtifactId Id { get; set; }

			[BsonElement("nam")]
			public ArtifactName Name { get; set; }

			[BsonElement("typ")]
			public ArtifactType Type { get; set; }

			[BsonElement("str")]
			public StreamId StreamId { get; set; }

			[BsonElement("chg")]
			public int Change { get; set; }

			[BsonElement("key")]
			public List<string> Keys { get; set; } = new List<string>();

			IReadOnlyList<string> IArtifact.Keys => Keys;

			[BsonElement("ns")]
			public NamespaceId NamespaceId { get; set; }

			[BsonElement("ref")]
			public RefName RefName { get; set; }

			[BsonElement("scp")]
			public AclScopeName AclScope { get; set; }

			[BsonElement("exp")]
			public DateTime? ExpireAtUtc { get; set; }

			[BsonElement("upd")]
			public int UpdateIndex { get; set; }

			[BsonConstructor]
			private Artifact()
			{
			}

			public Artifact(ArtifactId id, ArtifactName name, ArtifactType type, StreamId streamId, int change, IEnumerable<string> keys, NamespaceId namespaceId, RefName refName, DateTime? expireAtUtc, AclScopeName scopeName)
			{
				Id = id;
				Name = name;
				Type = type;
				StreamId = streamId;
				Change = change;
				Keys.AddRange(keys);
				NamespaceId = namespaceId;
				RefName = refName;
				ExpireAtUtc = expireAtUtc;
				AclScope = scopeName;
			}
		}

		private readonly IMongoCollection<Artifact> _artifacts;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service</param>
		public ArtifactCollection(MongoService mongoService)
		{
			List<MongoIndex<Artifact>> indexes = new List<MongoIndex<Artifact>>();
			indexes.Add(keys => keys.Ascending(x => x.Keys));
			indexes.Add(keys => keys.Ascending(x => x.ExpireAtUtc), sparse: true);
			indexes.Add(keys => keys.Ascending(x => x.StreamId).Descending(x => x.Change).Ascending(x => x.Name).Descending(x => x.Id));
			_artifacts = mongoService.GetCollection<Artifact>("ArtifactsV2", indexes);
		}

		/// <inheritdoc/>
		public async Task<IArtifact> AddAsync(ArtifactName name, ArtifactType type, StreamId streamId, int change, IEnumerable<string> keys, DateTime? expireAtUtc, AclScopeName scopeName, CancellationToken cancellationToken)
		{
			ArtifactId id = new ArtifactId(BinaryIdUtils.CreateNew());

			NamespaceId namespaceId = Namespace.Artifacts;
			RefName refName = new RefName($"{streamId}/{change}/{id}");

			Artifact artifact = new Artifact(id, name, type, streamId, change, keys, namespaceId, refName, expireAtUtc, scopeName);
			await _artifacts.InsertOneAsync(artifact, null, cancellationToken);
			return artifact;
		}

		/// <inheritdoc/>
		public async Task DeleteAsync(IEnumerable<ArtifactId> ids, CancellationToken cancellationToken = default)
		{
			FilterDefinition<Artifact> filter = Builders<Artifact>.Filter.In(x => x.Id, ids);
			await _artifacts.DeleteManyAsync(filter, cancellationToken);
		}

		/// <inheritdoc/>
		public async IAsyncEnumerable<IArtifact> FindAsync(StreamId streamId, int? minChange = null, int? maxChange = null, ArtifactName? name = null, ArtifactType? type = null, IEnumerable<string>? keys = null, [EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			FilterDefinition<Artifact> filter = Builders<Artifact>.Filter.Eq(x => x.StreamId, streamId);
			if (minChange != null)
			{
				filter = filter & Builders<Artifact>.Filter.Gte(x => x.Change, minChange.Value);
			}
			if (maxChange != null)
			{
				filter = filter & Builders<Artifact>.Filter.Lte(x => x.Change, maxChange.Value);
			}
			if (name != null)
			{
				filter = filter &= Builders<Artifact>.Filter.Eq(x => x.Name, name.Value);
			}
			if (type != null)
			{
				filter = filter &= Builders<Artifact>.Filter.Eq(x => x.Type, type.Value);
			}
			if (keys != null && keys.Any())
			{
				filter = filter & Builders<Artifact>.Filter.All(x => x.Keys, keys);
			}

			using (IAsyncCursor<Artifact> cursor = await _artifacts.Find(filter).SortByDescending(x => x.Change).ThenByDescending(x => x.Id).ToCursorAsync(cancellationToken))
			{
				while (await cursor.MoveNextAsync(cancellationToken))
				{
					foreach (Artifact artifact in cursor.Current)
					{
						yield return artifact;
					}
				}
			}
		}

		/// <summary>
		/// Finds artifacts which are ready for expiry
		/// </summary>
		/// <param name="utcNow">Current time for expiring artifacts</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Sequence of artifacts</returns>
		public async IAsyncEnumerable<IEnumerable<IArtifact>> FindExpiredAsync(DateTime utcNow, [EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			using (IAsyncCursor<Artifact> cursor = await _artifacts.Find(x => x.ExpireAtUtc!.Value < utcNow).ToCursorAsync(cancellationToken))
			{
				while (await cursor.MoveNextAsync(cancellationToken))
				{
					yield return cursor.Current;
				}
			}
		}

		/// <inheritdoc/>
		public async Task<IArtifact?> GetAsync(ArtifactId artifactId, CancellationToken cancellationToken)
		{
			return await _artifacts.Find(x => x.Id == artifactId).FirstOrDefaultAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IArtifact?> TryUpdateAsync(IArtifact artifact, DateTime? expireAtUtc, CancellationToken cancellationToken)
		{
			List<UpdateDefinition<Artifact>> updates = new List<UpdateDefinition<Artifact>>();
			if (expireAtUtc != null)
			{
				updates.Add(Builders<Artifact>.Update.SetOrUnsetNull(x => x.ExpireAtUtc, expireAtUtc));
			}
			if (updates.Count == 0)
			{
				return artifact;
			}

			Artifact artifactDoc = (Artifact)artifact;
			FilterDefinition<Artifact> filter = Builders<Artifact>.Filter.Expr(x => x.Id == artifact.Id && x.UpdateIndex == artifactDoc.UpdateIndex);
			UpdateDefinition<Artifact> update = Builders<Artifact>.Update.Combine(updates);

			return await _artifacts.FindOneAndUpdateAsync(filter, update, new FindOneAndUpdateOptions<Artifact> { ReturnDocument = ReturnDocument.After }, cancellationToken);
		}
	}
}
