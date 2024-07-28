// Copyright Epic Games, Inc. All Rights Reserved.

using System.Runtime.CompilerServices;
using EpicGames.Core;
using EpicGames.Horde.Acls;
using EpicGames.Horde.Artifacts;
using EpicGames.Horde.Commits;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Streams;
using HordeServer.Commits;
using HordeServer.Server;
using HordeServer.Storage;
using HordeServer.Utilities;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace HordeServer.Artifacts
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

			[BsonElement("dsc"), BsonIgnoreIfNull]
			public string? Description { get; set; }

			[BsonElement("str")]
			public StreamId StreamId { get; set; }

			[BsonElement("com")]
			public string? CommitName { get; set; }

			[BsonElement("chg")]
			public int CommitOrder { get; set; } // Was P4 changelist number

			[BsonIgnore]
			public CommitIdWithOrder CommitId
			{
				get => (CommitName != null)? new CommitIdWithOrder(CommitName, CommitOrder) : CommitIdWithOrder.FromPerforceChange(CommitOrder);
				set => (CommitName, CommitOrder) = (value.Name, value.Order);
			}

			[BsonElement("key")]
			public List<string> Keys { get; set; } = new List<string>();

			IReadOnlyList<string> IArtifact.Keys => Keys;

			[BsonElement("met")]
			public List<string> Metadata { get; set; } = new List<string>();

			IReadOnlyList<string> IArtifact.Metadata => Metadata;

			[BsonElement("ns")]
			public NamespaceId NamespaceId { get; set; }

			[BsonElement("ref")]
			public RefName RefName { get; set; }

			[BsonElement("scp")]
			public AclScopeName AclScope { get; set; }

			[BsonElement("cre")]
			public DateTime CreatedAtUtc { get; set; }

			[BsonElement("upd")]
			public int UpdateIndex { get; set; }

			DateTime IArtifact.CreatedAtUtc => (CreatedAtUtc == default) ? BinaryIdUtils.ToObjectId(Id.Id).CreationTime : CreatedAtUtc;

			[BsonConstructor]
			private Artifact()
			{
			}

			public Artifact(ArtifactId id, ArtifactName name, ArtifactType type, string? description, StreamId streamId, CommitIdWithOrder commitId, IEnumerable<string> keys, IEnumerable<string> metadata, NamespaceId namespaceId, RefName refName, DateTime createdAtUtc, AclScopeName scopeName)
			{
				Id = id;
				Name = name;
				Type = type;
				Description = description;
				StreamId = streamId;
				CommitId = commitId;
				Keys.AddRange(keys);
				Metadata.AddRange(metadata);
				NamespaceId = namespaceId;
				RefName = refName;
				CreatedAtUtc = createdAtUtc;
				AclScope = scopeName;
			}
		}

		readonly IMongoCollection<Artifact> _artifacts;
		readonly IClock _clock;
		readonly ICommitService _commitService;

		/// <summary>
		/// Constructor
		/// </summary>
		public ArtifactCollection(IMongoService mongoService, IClock clock, ICommitService commitService)
		{
			List<MongoIndex<Artifact>> indexes = new List<MongoIndex<Artifact>>();
			indexes.Add(keys => keys.Ascending(x => x.Keys));
			indexes.Add(keys => keys.Ascending(x => x.Type).Descending(x => x.Id));
			indexes.Add(keys => keys.Ascending(x => x.StreamId).Descending(x => x.CommitOrder).Ascending(x => x.Name).Descending(x => x.Id));
			_artifacts = mongoService.GetCollection<Artifact>("ArtifactsV2", indexes);

			_clock = clock;
			_commitService = commitService;
		}

#pragma warning disable CA1308 // Expect ansi-only keys here
		static string NormalizeKey(string key)
			=> key.ToLowerInvariant();
#pragma warning restore CA1308

		/// <summary>
		/// Gets the base path for a set of artifacts
		/// </summary>
		public static string GetArtifactPath(StreamId streamId, ArtifactType type) => $"{type}/{streamId}";

		/// <inheritdoc/>
		public async Task<IArtifact> AddAsync(ArtifactName name, ArtifactType type, string? description, StreamId streamId, CommitId commitId, IEnumerable<string> keys, IEnumerable<string> metadata, AclScopeName scopeName, CancellationToken cancellationToken)
		{
			if (name.Id.IsEmpty)
			{
				throw new ArgumentException($"Artifact name cannot be empty", nameof(name));
			}
			if (type.Id.IsEmpty)
			{
				throw new ArgumentException($"Artifact type for '{name}' is not valid", nameof(type));
			}

			ArtifactId id = new ArtifactId(BinaryIdUtils.CreateNew());

			NamespaceId namespaceId = Namespace.Artifacts;
			RefName refName = new RefName($"{GetArtifactPath(streamId, type)}/{commitId}/{name}/{id}");

			CommitIdWithOrder commitIdWithOrder = await _commitService.GetOrderedAsync(streamId, commitId, cancellationToken);

			Artifact artifact = new Artifact(id, name, type, description, streamId, commitIdWithOrder, keys.Select(x => NormalizeKey(x)), metadata, namespaceId, refName, _clock.UtcNow, scopeName);
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
		public async IAsyncEnumerable<IArtifact> FindAsync(StreamId? streamId = null, CommitId? minCommitId = null, CommitId? maxCommitId = null, ArtifactName? name = null, ArtifactType? type = null, IEnumerable<string>? keys = null, int maxResults = 100, [EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			FilterDefinition<Artifact> filter = FilterDefinition<Artifact>.Empty;
			if (streamId != null)
			{
				filter &= Builders<Artifact>.Filter.Eq(x => x.StreamId, streamId.Value);
				if (minCommitId != null)
				{
					CommitIdWithOrder minCommitIdWithOrder = await _commitService.GetOrderedAsync(streamId.Value, minCommitId, cancellationToken);
					filter &= Builders<Artifact>.Filter.Gte(x => x.CommitOrder, minCommitIdWithOrder.Order);
				}
				if (maxCommitId != null)
				{
					CommitIdWithOrder maxCommitIdWithOrder = await _commitService.GetOrderedAsync(streamId.Value, maxCommitId, cancellationToken);
					filter &= Builders<Artifact>.Filter.Lte(x => x.CommitOrder, maxCommitIdWithOrder.Order);
				}
			}
			if (name != null)
			{
				filter &= Builders<Artifact>.Filter.Eq(x => x.Name, name.Value);
			}
			if (type != null)
			{
				filter &= Builders<Artifact>.Filter.Eq(x => x.Type, type.Value);
			}
			if (keys != null && keys.Any())
			{
				filter &= Builders<Artifact>.Filter.All(x => x.Keys, keys.Select(x => NormalizeKey(x)));
			}

			using (IAsyncCursor<Artifact> cursor = await _artifacts.Find(filter).SortByDescending(x => x.CommitOrder).ThenByDescending(x => x.Id).Limit(maxResults).ToCursorAsync(cancellationToken))
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

		/// <inheritdoc/>
		public async IAsyncEnumerable<IEnumerable<IArtifact>> FindExpiredAsync(ArtifactType type, DateTime? expireAtUtc, [EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			FilterDefinition<Artifact> filter = Builders<Artifact>.Filter.Eq(x => x.Type, type);
			if (expireAtUtc != null)
			{
				filter &= Builders<Artifact>.Filter.Lt(x => x.Id, new ArtifactId(BinaryIdUtils.FromObjectId(ObjectId.GenerateNewId(expireAtUtc.Value))));
			}

			IFindFluent<Artifact, Artifact> query = _artifacts.Find(filter).SortByDescending(x => x.Id);
			using (IAsyncCursor<Artifact> cursor = await query.ToCursorAsync(cancellationToken))
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
	}
}
