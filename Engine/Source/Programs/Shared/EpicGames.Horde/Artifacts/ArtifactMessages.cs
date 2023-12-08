// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using EpicGames.Core;
using EpicGames.Horde.Streams;

#pragma warning disable CA2227

namespace EpicGames.Horde.Artifacts
{
	/// <summary>
	/// Describes an artifact
	/// </summary>
	public class GetArtifactResponse
	{
		/// <summary>
		/// Identifier for the Artifact. Randomly generated.
		/// </summary>
		public ArtifactId Id { get; }

		/// <summary>
		/// Type of artifact
		/// </summary>
		public ArtifactType Type { get; }

		/// <summary>
		/// Stream that produced the artifact
		/// </summary>
		public StreamId StreamId { get; }

		/// <summary>
		/// Change number
		/// </summary>
		public int Change { get; }

		/// <summary>
		/// Keys used to collate artifacts
		/// </summary>
		public IReadOnlyList<string> Keys { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public GetArtifactResponse(ArtifactId id, ArtifactType type, StreamId streamId, int change, IReadOnlyList<string> keys)
		{
			Id = id;
			Type = type;
			StreamId = streamId;
			Change = change;
			Keys = keys;
		}
	}

	/// <summary>
	/// Result of an artifact search
	/// </summary>
	public class FindArtifactsResponse
	{
		/// <summary>
		/// List of artifacts matching the search criteria
		/// </summary>
		public List<GetArtifactResponse> Artifacts { get; set; } = new List<GetArtifactResponse>();
	}

	/// <summary>
	/// Describes a file within an artifact
	/// </summary>
	public class GetArtifactFileEntryResponse
	{
		/// <summary>
		/// Name of this file
		/// </summary>
		public string Name { get; }

		/// <summary>
		/// Length of this entry
		/// </summary>
		public long Length { get; }

		/// <summary>
		/// Hash of the target node
		/// </summary>
		public IoHash Hash { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public GetArtifactFileEntryResponse(string name, long length, IoHash hash)
		{
			Name = name;
			Length = length;
			Hash = hash;
		}
	}

	/// <summary>
	/// Describes a file within an artifact
	/// </summary>
	public class GetArtifactDirectoryEntryResponse : GetArtifactDirectoryResponse
	{
		/// <summary>
		/// Name of this file
		/// </summary>
		public string Name { get; }

		/// <summary>
		/// Length of this entry
		/// </summary>
		public long Length { get; }

		/// <summary>
		/// Hash of the target node
		/// </summary>
		public IoHash Hash { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public GetArtifactDirectoryEntryResponse(string name, long length, IoHash hash)
		{
			Name = name;
			Length = length;
			Hash = hash;
		}
	}

	/// <summary>
	/// Describes a directory within an artifact
	/// </summary>
	public class GetArtifactDirectoryResponse
	{
		/// <summary>
		/// Names of sub-directories
		/// </summary>
		public List<GetArtifactDirectoryEntryResponse>? Directories { get; set; }

		/// <summary>
		/// Files within the directory
		/// </summary>
		public List<GetArtifactFileEntryResponse>? Files { get; set; }
	}

	/// <summary>
	/// Request to create a zip file with artifact data
	/// </summary>
	public class CreateZipRequest
	{
		/// <summary>
		/// Filter lines for the zip. Uses standard <see cref="FileFilter"/> syntax.
		/// </summary>
		public List<string> Filter { get; set; } = new List<string>();
	}
}
