// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Commits;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Streams;

namespace EpicGames.Horde.Artifacts
{
	/// <summary>
	/// Information about an artifact
	/// </summary>
	public interface IArtifact
	{
		/// <summary>
		/// Identifier for the Artifact. Randomly generated.
		/// </summary>
		ArtifactId Id { get; }

		/// <summary>
		/// Name of the artifact
		/// </summary>
		ArtifactName Name { get; }

		/// <summary>
		/// Type of artifact
		/// </summary>
		ArtifactType Type { get; }

		/// <summary>
		/// Description for the artifact
		/// </summary>
		string? Description { get; }

		/// <summary>
		/// Identifier for the stream that produced the artifact
		/// </summary>
		StreamId StreamId { get; }

		/// <summary>
		/// Change that the artifact corresponds to
		/// </summary>
		CommitIdWithOrder CommitId { get; }

		/// <summary>
		/// Keys used to collate artifacts
		/// </summary>
		IReadOnlyList<string> Keys { get; }

		/// <summary>
		/// Metadata for the artifact
		/// </summary>
		IReadOnlyList<string> Metadata { get; }

		/// <summary>
		/// Storage namespace containing the data
		/// </summary>
		NamespaceId NamespaceId { get; }

		/// <summary>
		/// Name of the ref containing the root data object
		/// </summary>
		RefName RefName { get; }

		/// <summary>
		/// Time at which the artifact was created
		/// </summary>
		DateTime CreatedAtUtc { get; }

		/// <summary>
		/// Deletes this artifact
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task DeleteAsync(CancellationToken cancellationToken);
	}
}
