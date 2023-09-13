// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

#pragma warning disable CA2227

namespace EpicGames.Horde.Api
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
		/// Keys used to collate artifacts
		/// </summary>
		public IReadOnlyList<string> Keys { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public GetArtifactResponse(ArtifactId id, ArtifactType type, IReadOnlyList<string> keys)
		{
			Id = id;
			Type = type;
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

	/// <summary>
	/// Extension methods for the artifacts endpoint
	/// </summary>
	public static class ArtifactExtensions
	{
		/// <summary>
		/// Gets metadata about an artifact object
		/// </summary>
		/// <param name="horde">The Horde client instance</param>
		/// <param name="id">Identifier for the artifact</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static Task<GetArtifactResponse> GetArtifactAsync(this HordeHttpClient horde, ArtifactId id, CancellationToken cancellationToken = default)
		{
			return horde.GetAsync<GetArtifactResponse>($"api/v2/artifacts/{id}", cancellationToken);
		}

		/// <summary>
		/// Finds artifacts with a set of ids or keys
		/// </summary>
		/// <param name="horde">The Horde client instance</param>
		/// <param name="ids">Artifact ids to return</param>
		/// <param name="keys">Keys to find</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Information about all the artifacts</returns>
		public static async Task<List<GetArtifactResponse>> FindArtifactsAsync(this HordeHttpClient horde, IEnumerable<ArtifactId>? ids = null, IEnumerable<string>? keys = null, CancellationToken cancellationToken = default)
		{
			QueryStringBuilder queryParams = new QueryStringBuilder();
			if (ids != null)
			{
				foreach (ArtifactId id in ids)
				{
					queryParams.Add("id", id.ToString());
				}
			}
			if (keys != null)
			{
				foreach (string key in keys)
				{
					queryParams.Add("key", key);
				}
			}

			FindArtifactsResponse response = await horde.GetAsync<FindArtifactsResponse>($"api/v2/artifacts?{queryParams}", cancellationToken);
			return response.Artifacts;
		}

#if false
			/// <summary>
			/// Gets metadata about an artifact object
			/// </summary>
			/// <param name="id">Identifier of the artifact to retrieve</param>
			/// <param name="filter">Filter for returned properties</param>
			/// <returns>Information about all the artifacts</returns>
			[HttpGet]
			[Route("/api/v2/artifacts/{id}")]
			[ProducesResponseType(typeof(FindArtifactsResponse), 200)]
			public async Task<ActionResult<object>> GetArtifactAsync(ArtifactId id, [FromQuery] PropertyFilter? filter = null);

			/// <summary>
			/// Retrieves bundles for a particular artifact
			/// </summary>
			/// <param name="id">Identifier of the artifact to retrieve</param>
			/// <param name="locator">The blob locator</param>
			/// <param name="length">Length of data to return</param>
			/// <param name="offset">Offset of the data to return</param>
			/// <param name="cancellationToken">Cancellation token for the operation</param>
			/// <returns>Information about all the artifacts</returns>
			[HttpGet]
			[Route("/api/v2/artifacts/{id}/blobs/{*locator}")]
			[Route("/api/v2/artifacts/{id}/bundles/{*locator}")]
			public async Task<ActionResult<object>> ReadArtifactBlobAsync(ArtifactId id, BundleLocator locator, [FromQuery] int? offset = null, [FromQuery] int? length = null, CancellationToken cancellationToken = default)

			/// <summary>
			/// Retrieves the root blob for an artifact
			/// </summary>
			/// <param name="id">Identifier of the artifact to retrieve</param>
			/// <param name="cancellationToken">Cancellation token for the operation</param>
			/// <returns>Information about all the artifacts</returns>
			[HttpGet]
			[Route("/api/v2/artifacts/{id}/ref")]
			public async Task<ActionResult<object>> ReadArtifactRefAsync(ArtifactId id, CancellationToken cancellationToken = default)

			/// <summary>
			/// Gets metadata about an artifact object
			/// </summary>
			/// <param name="id">Identifier of the artifact to retrieve</param>
			/// <param name="path">Path to fetch</param>
			/// <param name="filter">Filter for returned properties</param>
			/// <param name="cancellationToken">Cancellation token for the operation</param>
			/// <returns>Information about all the artifacts</returns>
			[HttpGet]
			[Route("/api/v2/artifacts/{id}/browse")]
			[ProducesResponseType(typeof(GetArtifactDirectoryResponse), 200)]
			public async Task<ActionResult<object>> BrowseArtifactAsync(ArtifactId id, [FromQuery] string? path = null, [FromQuery] PropertyFilter? filter = null, CancellationToken cancellationToken = default)

			/// <summary>
			/// Downloads an individual file from an artifact
			/// </summary>
			/// <param name="id">Identifier of the artifact to retrieve</param>
			/// <param name="path">Path to fetch</param>
			/// <param name="inline">Whether to request the file be downloaded vs displayed inline</param>
			/// <param name="cancellationToken">Cancellation token for the operation</param>
			/// <returns>Information about all the artifacts</returns>
			[HttpGet]
			[Route("/api/v2/artifacts/{id}/file")]
			public async Task<ActionResult<object>> GetFileAsync(ArtifactId id, [FromQuery] string path, [FromQuery] bool inline = false, CancellationToken cancellationToken = default)

			/// <summary>
			/// Downloads an individual file from an artifact
			/// </summary>
			/// <param name="id">Identifier of the artifact to retrieve</param>
			/// <param name="filter">Paths to include in the zip file. The post version of this request allows for more parameters than can fit in a request string.</param>
			/// <param name="cancellationToken">Cancellation token for the operation</param>
			/// <returns>Information about all the artifacts</returns>
			[HttpGet]
			[Route("/api/v2/artifacts/{id}/zip")]
			public async Task<ActionResult<object>> GetZipAsync(ArtifactId id, [FromQuery(Name = "filter")] string[]? filter, CancellationToken cancellationToken = default)

			/// <summary>
			/// Downloads an individual file from an artifact
			/// </summary>
			/// <param name="id">Identifier of the artifact to retrieve</param>
			/// <param name="request">Filter for the zip file</param>
			/// <param name="cancellationToken">Cancellation token for the operation</param>
			/// <returns>Information about all the artifacts</returns>
			[HttpPost]
			[Route("/api/v2/artifacts/{id}/zip")]
			public async Task<ActionResult<object>> CreateZipFromFilterAsync(ArtifactId id, CreateZipRequest request, CancellationToken cancellationToken = default)

			/// <summary>
			/// Gets metadata about an artifact object
			/// </summary>
			/// <param name="ids">Artifact ids to return</param>
			/// <param name="keys">Keys to find</param>
			/// <param name="filter">Filter for returned values</param>
			/// <returns>Information about all the artifacts</returns>
			[HttpGet]
			[Route("/api/v2/artifacts")]
			[ProducesResponseType(typeof(FindArtifactsResponse), 200)]
			public async Task<ActionResult<object>> FindArtifactsAsync([FromQuery(Name = "id")] IEnumerable<ArtifactId>? ids = null, [FromQuery(Name = "key")] IEnumerable<string>? keys = null, [FromQuery] PropertyFilter? filter = null)
		}
#endif
	}
}
