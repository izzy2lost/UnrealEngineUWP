// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Commits;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using EpicGames.Horde.Streams;

namespace EpicGames.Horde.Artifacts
{
	class ArtifactHttpCollection : IArtifactCollection
	{
		[DebuggerDisplay("{Id}")]
		class Artifact : IArtifact
		{
			readonly ArtifactHttpCollection _collection;

			public Artifact(ArtifactHttpCollection collection, GetArtifactResponse response)
				: this(collection, response.Id, response.Name, response.Type, response.Description, response.StreamId, response.CommitId, response.Keys, response.Metadata, response.NamespaceId, response.RefName, response.CreatedAtUtc)
			{ }

			public Artifact(ArtifactHttpCollection collection, ArtifactId id, ArtifactName name, ArtifactType type, string? description, StreamId streamId, CommitIdWithOrder commitId, IReadOnlyList<string> keys, IReadOnlyList<string> metadata, NamespaceId namespaceId, RefName refName, DateTime createdAtUtc)
			{
				_collection = collection;

				Id = id;
				Name = name;
				Type = type;
				Description = description;
				StreamId = streamId;
				CommitId = commitId;
				Keys = keys;
				Metadata = metadata;
				NamespaceId = namespaceId;
				RefName = refName;
				CreatedAtUtc = createdAtUtc;
			}

			public ArtifactId Id { get; }
			public ArtifactName Name { get; }
			public ArtifactType Type { get; }
			public string? Description { get; }
			public StreamId StreamId { get; }
			public CommitIdWithOrder CommitId { get; }
			public IReadOnlyList<string> Keys { get; }
			public IReadOnlyList<string> Metadata { get; }
			public NamespaceId NamespaceId { get; }
			public RefName RefName { get; }
			public DateTime CreatedAtUtc { get; }

			public IBlobRef<DirectoryNode> Content
				=> _collection.Open(this);

			public Task DeleteAsync(CancellationToken cancellationToken)
				=> _collection.DeleteAsync(Id, cancellationToken);
		}

		readonly IHordeClient _hordeClient;

		public ArtifactHttpCollection(IHordeClient hordeClient)
			=> _hordeClient = hordeClient;

		IBlobRef<DirectoryNode> Open(Artifact artifact)
		{
			IStorageClient store = _hordeClient.CreateStorageClient(artifact.Id);
			return store.CreateBlobRef<DirectoryNode>(new RefName("default"));
		}

		/// <inheritdoc/>
		public async Task<IArtifact> AddAsync(ArtifactName name, ArtifactType type, string? description, StreamId streamId, CommitId commitId, IEnumerable<string> keys, IEnumerable<string> metadata, CancellationToken cancellationToken = default)
		{
			HordeHttpClient hordeHttpClient = _hordeClient.CreateHttpClient();
			CreateArtifactResponse response = await hordeHttpClient.CreateArtifactAsync(name, type, description, streamId, commitId, keys, metadata, cancellationToken);
			return new Artifact(this, response.ArtifactId, name, type, description, streamId, CommitIdWithOrder.FromPerforceChange(commitId.GetPerforceChange()), keys.ToList(), metadata.ToList(), response.NamespaceId, response.RefName, DateTime.UtcNow);
		}

		/// <inheritdoc/>
		public async IAsyncEnumerable<IArtifact> FindAsync(StreamId? streamId = null, CommitId? minCommitId = null, CommitId? maxCommitId = null, ArtifactName? name = null, ArtifactType? type = null, IEnumerable<string>? keys = null, int maxResults = 100, [EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			HordeHttpClient hordeHttpClient = _hordeClient.CreateHttpClient();

			List<GetArtifactResponse> responses = await hordeHttpClient.FindArtifactsAsync(streamId, minCommitId, maxCommitId, name, type, keys, maxResults, cancellationToken);
			foreach (GetArtifactResponse response in responses)
			{
				yield return new Artifact(this, response);
			}
		}

		async Task DeleteAsync(ArtifactId artifactId, CancellationToken cancellationToken = default)
		{
			HordeHttpClient hordeHttpClient = _hordeClient.CreateHttpClient();
			await hordeHttpClient.DeleteArtifactAsync(artifactId, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<IArtifact?> GetAsync(ArtifactId artifactId, CancellationToken cancellationToken = default)
		{
			HordeHttpClient hordeHttpClient = _hordeClient.CreateHttpClient();
			GetArtifactResponse? response = await hordeHttpClient.GetArtifactAsync(artifactId, cancellationToken);
			return new Artifact(this, response);
		}
	}
}
