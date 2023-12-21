// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http.Headers;
using System.Security.Claims;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Amazon.EC2.Model;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Bundles;
using EpicGames.Horde.Storage.Nodes;
using Horde.Server.Acls;
using Horde.Server.Server;
using Horde.Server.Utilities;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;

namespace Horde.Server.Storage
{
	/// <summary>
	/// Controller for the /api/v1/storage endpoint
	/// </summary>
	[Authorize]
	[ApiController]
	[Route("[controller]")]
	public class StorageController : HordeControllerBase
	{
		readonly IStorageClientFactory _storageClientFactory;
		readonly IOptionsSnapshot<GlobalConfig> _globalConfig;

		/// <summary>
		/// Constructor
		/// </summary>
		public StorageController(IStorageClientFactory storageClientFactory, IOptionsSnapshot<GlobalConfig> globalConfig)
		{
			_storageClientFactory = storageClientFactory;
			_globalConfig = globalConfig;
		}

		bool Authorize(NamespaceId namespaceId, AclAction action)
		{
			return _globalConfig.Value.Storage.TryGetNamespace(namespaceId, out NamespaceConfig? namespaceConfig) && namespaceConfig.Authorize(action, User);
		}

		/// <summary>
		/// Uploads data to the storage service. 
		/// </summary>
		/// <param name="namespaceId">Namespace to fetch from</param>
		/// <param name="file">Data to be uploaded. May be null, in which case the server may return a separate url.</param>
		/// <param name="prefix">Prefix for the uploaded file</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		[HttpPost]
		[Route("/api/v1/storage/{namespaceId}/blobs")]
		[Route("/api/v1/storage/{namespaceId}/bundles")]
		public async Task<ActionResult<WriteBlobResponse>> WriteBlobAsync(NamespaceId namespaceId, IFormFile? file, [FromForm] string? prefix = default, CancellationToken cancellationToken = default)
		{
			using IStorageClient? storageClient = _storageClientFactory.TryCreateClient(namespaceId);
			if (storageClient == null)
			{
				return NotFound(namespaceId);
			}
			if (!Authorize(namespaceId, StorageAclAction.WriteBlobs) && !HasPathClaim(User, HordeClaimTypes.WriteNamespace, namespaceId, prefix ?? String.Empty))
			{
				return Forbid(StorageAclAction.WriteBlobs, namespaceId);
			}

			return await WriteBlobAsync(storageClient, file, prefix, cancellationToken);
		}

		/// <summary>
		/// Writes a blob to storage. Exposed as a public utility method to allow other routes with their own authentication methods to wrap their own authentication/redirection.
		/// </summary>
		/// <param name="storageClient">The client to write to service</param>
		/// <param name="file">File to be written</param>
		/// <param name="prefix">Prefix for uploaded blobs</param>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns>Information about the written blob, or redirect information</returns>
		public static async Task<ActionResult<WriteBlobResponse>> WriteBlobAsync(IStorageClient storageClient, IFormFile? file, [FromForm] string? prefix = default, CancellationToken cancellationToken = default)
		{
			if (file == null)
			{
				(BlobLocator Path, Uri UploadUrl)? result = await storageClient.TryGetWriteRedirectAsync(prefix ?? String.Empty, cancellationToken);
				if (result == null)
				{
					return new WriteBlobResponse { SupportsRedirects = false };
				}

				return new WriteBlobResponse { Blob = result.Value.Path.ToString(), UploadUrl = result.Value.UploadUrl };
			}
			else
			{
				using Stream stream = file.OpenReadStream();
				IBlobHandle handle = await storageClient.WriteBlobAsync(Bundle.BlobType, stream, Array.Empty<IBlobHandle>(), prefix, cancellationToken);
				return new WriteBlobResponse { Blob = handle.GetLocator().ToString(), SupportsRedirects = storageClient.SupportsRedirects };
			}
		}

		/// <summary>
		/// Retrieves data from the storage service. 
		/// </summary>
		/// <param name="namespaceId">Namespace to fetch from</param>
		/// <param name="locator">Bundle to retrieve</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		[HttpGet]
		[Route("/api/v1/storage/{namespaceId}/blobs/{*locator}")]
		[Route("/api/v1/storage/{namespaceId}/bundles/{*locator}")]
		public async Task<ActionResult> ReadBlobAsync(NamespaceId namespaceId, BlobLocator locator, CancellationToken cancellationToken = default)
		{
			using IStorageClient? client = _storageClientFactory.TryCreateClient(namespaceId);
			if (client == null)
			{
				return NotFound(namespaceId);
			}
			if (!Authorize(namespaceId, StorageAclAction.ReadBlobs) && !HasPathClaim(User, HordeClaimTypes.ReadNamespace, namespaceId, locator.Path.ToString()))
			{
				return Forbid(StorageAclAction.ReadBlobs, namespaceId);
			}

			return await ReadBlobInternalAsync(client, locator, Request.Headers, cancellationToken);
		}

		/// <summary>
		/// Reads a blob from storage, without performing namespace access checks.
		/// </summary>
		internal static async Task<ActionResult> ReadBlobInternalAsync(IStorageClient storageClient, BlobLocator locator, IHeaderDictionary headers, CancellationToken cancellationToken)
		{
			Uri? redirectUrl = await storageClient.TryGetReadRedirectAsync(locator, cancellationToken);
			if (redirectUrl != null)
			{
				return new RedirectResult(redirectUrl.ToString());
			}

			// Parse the range header
			int offset = 0;
			int? length = null;

			if (headers.Range.Count > 0)
			{
				if (headers.Range.Count > 1)
				{
					return new BadRequestObjectResult(LogEvent.Create(LogLevel.Error, "Unsupported range header; only one range is allowed"));
				}

				string? value = headers.Range[0];
				if (value == null)
				{
					return new BadRequestObjectResult(LogEvent.Create(LogLevel.Error, "Unsupported range header; only one range is allowed"));
				}

				Match match = Regex.Match(value, @"^\s*bytes\s*=\s*(\d*)-(\d*)$");
				if (!match.Success)
				{
					return new BadRequestObjectResult(LogEvent.Create(LogLevel.Error, "Unsupported range header syntax; cannot parse {Value}", value));
				}

				if (match.Groups[1].Length > 0 && !Int32.TryParse(match.Groups[1].Value, out offset))
				{
					return new BadRequestObjectResult(LogEvent.Create(LogLevel.Error, "Unable to parse start for range: {Value}", value));
				}
				if (match.Groups[2].Length > 0)
				{
					int end;
					if (Int32.TryParse(match.Groups[2].Value, out end) && end > offset)
					{
						length = (end + 1) - offset;
					}
					else
					{
						return new BadRequestObjectResult(LogEvent.Create(LogLevel.Error, "Unable to parse end for range: {Value}", value));
					}
				}
			}

#pragma warning disable CA2000 // Dispose objects before losing scope
			IBlobHandle handle = storageClient.CreateBlobHandle(new BlobLocator(locator.Path));
			Stream stream = await handle.OpenBodyAsync(offset, length, cancellationToken);
			return new FileStreamResult(stream, "application/octet-stream");
#pragma warning restore CA2000 // Dispose objects before losing scope
		}

		/// <summary>
		/// Retrieves data from the storage service. 
		/// </summary>
		/// <param name="namespaceId">Namespace to fetch from</param>
		/// <param name="alias">Alias of the node to find</param>
		/// <param name="maxResults">Maximum number of results to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		[HttpGet]
		[Route("/api/v1/storage/{namespaceId}/nodes")]
		public async Task<ActionResult<FindNodesResponse>> FindNodesAsync(NamespaceId namespaceId, [FromQuery] string alias, [FromQuery] int? maxResults = null, CancellationToken cancellationToken = default)
		{
			using IStorageClient? client = _storageClientFactory.TryCreateClient(namespaceId);
			if (client == null)
			{
				return NotFound(namespaceId);
			}
			if (!Authorize(namespaceId, StorageAclAction.ReadBlobs))
			{
				return Forbid(StorageAclAction.ReadBlobs, namespaceId);
			}

			BlobAlias[] aliases = await client.FindAliasesAsync(alias, maxResults, cancellationToken);

			FindNodesResponse response = new FindNodesResponse();
			response.Nodes.AddRange(aliases.Select(x => new FindNodeResponse(x.Target.GetLocator(), x.Rank, x.Data.ToArray())));

			if (response.Nodes.Count == 0)
			{
				return NotFound();
			}

			return response;
		}

		/// <summary>
		/// Writes a ref to the storage service.
		/// </summary>
		/// <param name="namespaceId">Namespace to write to</param>
		/// <param name="refName">Name of the ref</param>
		/// <param name="request">Request for the ref to write</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		[HttpPut]
		[Route("/api/v1/storage/{namespaceId}/refs/{*refName}")]
		public async Task<ActionResult> WriteRefAsync(NamespaceId namespaceId, RefName refName, [FromBody] WriteRefRequest request, CancellationToken cancellationToken)
		{
			using IStorageClient? client = _storageClientFactory.TryCreateClient(namespaceId);
			if (client == null)
			{
				return NotFound(namespaceId);
			}
			if (!Authorize(namespaceId, StorageAclAction.WriteRefs) && !HasPathClaim(User, HordeClaimTypes.WriteNamespace, namespaceId, refName.ToString()))
			{
				return Forbid(StorageAclAction.WriteRefs, namespaceId);
			}

#pragma warning disable CS0618 // Type or member is obsolete
			if (request.Blob != null && request.ExportIdx != null)
			{
				request.Target = new BlobLocator($"{request.Blob.Value}#{request.ExportIdx.Value}");
			}
#pragma warning restore CS0618 // Type or member is obsolete

			IBlobHandle target = client.CreateBlobHandle(request.Target);
			await client.WriteRefAsync(refName, target, request.Options, cancellationToken);

			return Ok();
		}

		/// <summary>
		/// Retrieves a ref from the storage service. 
		/// </summary>
		/// <param name="namespaceId"></param>
		/// <param name="refName"></param>
		/// <param name="cancellationToken"></param>
		[HttpGet]
		[Route("/api/v1/storage/{namespaceId}/refs/{*refName}")]
		public async Task<ActionResult<ReadRefResponse>> ReadRefAsync(NamespaceId namespaceId, RefName refName, CancellationToken cancellationToken)
		{
			NamespaceConfig? namespaceConfig;
			if (!_globalConfig.Value.Storage.TryGetNamespace(namespaceId, out namespaceConfig))
			{
				return NotFound(namespaceId);
			}
			if (!namespaceConfig.Authorize(StorageAclAction.ReadRefs, User) && !HasPathClaim(User, HordeClaimTypes.ReadNamespace, namespaceId, refName.ToString()))
			{
				return Forbid(StorageAclAction.ReadRefs, namespaceId);
			}

			return await ReadRefInternalAsync(_storageClientFactory, namespaceId, refName, Request.Headers, cancellationToken);
		}

		/// <summary>
		/// Reads a ref from storage, without performing namespace access checks.
		/// </summary>
		internal static async Task<ActionResult<ReadRefResponse>> ReadRefInternalAsync(IStorageClientFactory storageService, NamespaceId namespaceId, RefName refName, IHeaderDictionary headers, CancellationToken cancellationToken)
		{
			using IStorageClient client = storageService.CreateClient(namespaceId);

			RefCacheTime cacheTime = new RefCacheTime();
			foreach (string? entry in headers.CacheControl)
			{
				if (entry != null && CacheControlHeaderValue.TryParse(entry, out CacheControlHeaderValue? value) && value?.MaxAge != null)
				{
					cacheTime = new RefCacheTime(value.MaxAge.Value);
				}
			}

			IBlobHandle? target = await client.TryReadRefAsync(refName, cacheTime, cancellationToken: cancellationToken);
			if (target == null)
			{
				return new NotFoundResult();
			}

			string link = $"/api/v1/storage/{namespaceId}/nodes/{target.GetLocator()}";
			ReadRefResponse response = new ReadRefResponse { Target = target.GetLocator(), Link = link };

#pragma warning disable CS0618 // Type or member is obsolete
			try
			{
				BlobLocator locator = target.GetLocator();
				if(locator.TryUnwrap(out BlobLocator blob, out Utf8String fragment) && Int32.TryParse(fragment.ToString(), out int exportIdx))
				{
					response.Blob = blob;
					response.ExportIdx = exportIdx;
				}
			}
			catch { }
#pragma warning restore CS0618 // Type or member is obsolete

			return response;
		}

		/// <summary>
		/// Checks whether the user has an explicit claim to read or write to a path within a namespace
		/// </summary>
		/// <param name="user">User to query</param>
		/// <param name="claimType">The claim name</param>
		/// <param name="namespaceId">Namespace id to check for</param>
		/// <param name="entity">Path to the entity to query</param>
		/// <returns>True if the user is authorized for access to the given path</returns>
		static bool HasPathClaim(ClaimsPrincipal user, string claimType, NamespaceId namespaceId, string entity)
		{
			foreach (Claim claim in user.Claims)
			{
				if (claim.Type.Equals(claimType, StringComparison.Ordinal))
				{
					int colonIdx = claim.Value.IndexOf(':', StringComparison.Ordinal);
					if (colonIdx == -1)
					{
						if (namespaceId.Text.Equals(claim.Value))
						{
							return true;
						}
					}
					else
					{
						if (namespaceId.Text.Equals(claim.Value.AsMemory(0, colonIdx)) && HasPathPrefix(entity, claim.Value.AsSpan(colonIdx + 1)))
						{
							return true;
						}
					}
				}
			}
			return false;
		}

		static bool HasPathPrefix(string name, ReadOnlySpan<char> prefix)
		{
			if (name.Length > prefix.Length)
			{
				return name[prefix.Length] == '/' && name.AsSpan(0, prefix.Length).SequenceEqual(prefix);
			}
			else if (name.Length == prefix.Length)
			{
				return name.AsSpan().SequenceEqual(prefix);
			}
			else
			{
				return false;
			}
		}

		/// <summary>
		/// Gets information about a particular bundle in storage
		/// </summary>
		/// <param name="namespaceId">Namespace containing the blob</param>
		/// <param name="locator">Blob identifier</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		[HttpGet]
		[Route("/api/v1/storage/{namespaceId}/nodes/{*locator}")]
		public async Task<ActionResult<object>> GetNodeAsync(NamespaceId namespaceId, BlobLocator locator, CancellationToken cancellationToken = default)
		{
			NamespaceConfig? namespaceConfig;
			if (!_globalConfig.Value.Storage.TryGetNamespace(namespaceId, out namespaceConfig))
			{
				return NotFound(namespaceId);
			}
			if (!namespaceConfig.Authorize(StorageAclAction.ReadBlobs, User))
			{
				return Forbid(StorageAclAction.ReadBlobs, namespaceId);
			}

			using IStorageClient storageClient = _storageClientFactory.CreateClient(namespaceId);

			string linkBase = $"/api/v1/storage/{namespaceId}";

			object content;

			using BlobData blobData = await storageClient.CreateBlobHandle(locator).ReadAsync(cancellationToken);

			Node node = Node.Deserialize(blobData);
			switch (node)
			{
				case DirectoryNode directoryNode:
					{
						List<object> directories = new List<object>();
						foreach ((string name, DirectoryEntry entry) in directoryNode.NameToDirectory)
						{
							directories.Add(new { name = name.ToString(), length = entry.Length, hash = entry.Target.Hash, link = GetNodeLink(linkBase, entry.Target.Handle) });
						}

						List<object> files = new List<object>();
						foreach ((string name, FileEntry entry) in directoryNode.NameToFile)
						{
							files.Add(new { name = name.ToString(), length = entry.Length, flags = entry.Flags, hash = entry.StreamHash, link = GetNodeLink(linkBase, entry.Target.Handle) });
						}

						content = new { directoryNode.Length, directories, files };
					}
					break;
				default:
					content = new { references = blobData.Refs.Select(x => GetNodeLink(linkBase, x)) };
					break;
			}

			return new { type = blobData.Type.Guid, @class = node.GetType().Name, content = content };
		}

		static string GetNodeLink(string linkBase, IBlobHandle handle) => GetNodeLink(linkBase, handle.GetLocator());
		
		static string GetNodeLink(string linkBase, BlobLocator locator) => $"{linkBase}/nodes/{locator}";
	}
}
