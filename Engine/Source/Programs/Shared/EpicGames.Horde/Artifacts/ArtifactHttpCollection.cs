// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Acls;
using EpicGames.Horde.Commits;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Streams;

namespace EpicGames.Horde.Artifacts
{
	class ArtifactHttpCollection : IArtifactCollection
	{
		class Artifact : IArtifact
		{
			readonly GetArtifactResponse _response;

			public Artifact(GetArtifactResponse response)
			{
				_response = response;
			}

			public ArtifactId Id => _response.Id;
			public ArtifactName Name => _response.Name;
			public ArtifactType Type => _response.Type;
			public string? Description => _response.Description;
			public StreamId StreamId => _response.StreamId;
			public CommitIdWithOrder CommitId => _response.CommitId;
			public IReadOnlyList<string> Keys => _response.Keys;
			public IReadOnlyList<string> Metadata => _response.Metadata;
			public NamespaceId NamespaceId => _response.NamespaceId;
			public RefName RefName => _response.RefName;
			public DateTime CreatedAtUtc => _response.CreatedAtUtc;
		}

		readonly IHordeClient _hordeClient;

		public ArtifactHttpCollection(IHordeClient hordeClient)
			=> _hordeClient = hordeClient;

		/// <inheritdoc/>
		public Task<IArtifact> AddAsync(ArtifactName name, ArtifactType type, string? description, StreamId streamId, CommitId commitId, IEnumerable<string> keys, IEnumerable<string> metadata, AclScopeName scopeName, CancellationToken cancellationToken = default)
		{
			throw new NotImplementedException();
		}

		/// <inheritdoc/>
		public Task DeleteAsync(IEnumerable<ArtifactId> ids, CancellationToken cancellationToken = default)
		{
			throw new NotImplementedException();
		}

		/// <inheritdoc/>
		public async IAsyncEnumerable<IArtifact> FindAsync(StreamId? streamId = null, CommitId? minCommitId = null, CommitId? maxCommitId = null, ArtifactName? name = null, ArtifactType? type = null, IEnumerable<string>? keys = null, int maxResults = 100, [EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			HordeHttpClient hordeHttpClient = _hordeClient.CreateHttpClient();

			List<GetArtifactResponse> responses = await hordeHttpClient.FindArtifactsAsync(streamId, minCommitId, maxCommitId, name, type, keys, maxResults, cancellationToken);
			foreach (GetArtifactResponse response in responses)
			{
				yield return new Artifact(response);
			}
		}

		/// <inheritdoc/>
		public IAsyncEnumerable<IEnumerable<IArtifact>> FindExpiredAsync(ArtifactType type, DateTime? expireAtUtc, CancellationToken cancellationToken = default)
		{
			throw new NotImplementedException();
		}

		/// <inheritdoc/>
		public async Task<IArtifact?> GetAsync(ArtifactId artifactId, CancellationToken cancellationToken = default)
		{
			HordeHttpClient hordeHttpClient = _hordeClient.CreateHttpClient();
			GetArtifactResponse? response = await hordeHttpClient.GetArtifactAsync(artifactId, cancellationToken);
			return new Artifact(response);
		}
	}
}
