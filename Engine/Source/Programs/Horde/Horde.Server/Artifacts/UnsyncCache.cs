// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Frozen;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Storage.Nodes;
using Horde.Server.Storage;
using Microsoft.Extensions.Caching.Memory;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Artifacts
{
	/// <summary>
	/// Manifest for Unsync
	/// </summary>
	/// <param name="Files">Files in the manifest</param>
	public record class UnsyncManifest(IReadOnlyList<UnsyncFile> Files);

	/// <summary>
	/// Output file for an unsync manifest
	/// </summary>
	/// <param name="Name">Full path to the file</param>
	/// <param name="ReadOnly">Whether the file is read only</param>
	/// <param name="Executable">Whether to set the executable bit on the file</param>
	/// <param name="Length">Length of the file</param>
	/// <param name="ModTime">Last modified time for the file</param>
	/// <param name="Blocks">List of blocks making up the file</param>
	public record class UnsyncFile(Utf8String Name, bool ReadOnly, bool Executable, long Length, DateTime ModTime, IReadOnlyList<UnsyncBlock> Blocks);

	/// <summary>
	/// Describes a block of data in an unsync manifest
	/// </summary>
	/// <param name="Offset">Offset within the file</param>
	/// <param name="Length">Length of the block</param>
	/// <param name="RollingHash">Rolling hash of the block</param>
	/// <param name="Blob">Handle to the corresponding blob</param>
	public record class UnsyncBlock(long Offset, long Length, uint RollingHash, IBlobRef<LeafChunkedDataNode> Blob);

	/// <summary>
	/// Implements a cache for downloading Unsync blobs
	/// </summary>
	public sealed class UnsyncCache : IDisposable
	{
		class ArtifactInfo : IDisposable
		{
			public IStorageClient StorageClient { get; }
			public UnsyncManifest Manifest { get; }
			public FrozenDictionary<IoHash, IBlobRef<LeafChunkedDataNode>> Blobs { get; }

			public ArtifactInfo(IStorageClient storageClient, UnsyncManifest manifest, FrozenDictionary<IoHash, IBlobRef<LeafChunkedDataNode>> blobs)
			{
				StorageClient = storageClient;
				Manifest = manifest;
				Blobs = blobs;
			}

			public void Dispose()
				=> StorageClient.Dispose();
		}

		readonly IStorageService _storageService;
		readonly MemoryCache _cache;
		readonly object _lockObject = new object();
		readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		public UnsyncCache(IStorageService storageService, ILogger<UnsyncCache> logger)
		{
			_storageService = storageService;
			_cache = new MemoryCache(new MemoryCacheOptions());
			_logger = logger;
		}

		/// <inheritdoc/>
		public void Dispose()
			=> _cache.Dispose();

		/// <summary>
		/// 
		/// </summary>
		/// <param name="artifact"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public async ValueTask<UnsyncManifest?> GetManifestAsync(IArtifact artifact, CancellationToken cancellationToken = default)
		{
			ArtifactInfo? artifactInfo = await GetArtifactInfoAsync(artifact, cancellationToken);
			return artifactInfo?.Manifest;
		}

		async Task<ArtifactInfo?> GetArtifactInfoAsync(IArtifact artifact, CancellationToken cancellationToken = default)
		{
			// Get the artifact info
			BackgroundTask<ArtifactInfo?>? artifactInfoTask;
			if (!_cache.TryGetValue(artifact.Id, out artifactInfoTask) || artifactInfoTask == null)
			{
				lock (_lockObject)
				{
					if (!_cache.TryGetValue(artifact.Id, out artifactInfoTask) || artifactInfoTask == null)
					{
						artifactInfoTask = BackgroundTask.StartNew(ctx => ReadArtifactAsync(artifact, ctx));
						using (ICacheEntry entry = _cache.CreateEntry(artifact.Id))
						{
							entry.SetSize(1);
							entry.SetSlidingExpiration(TimeSpan.FromHours(1.0));
							entry.SetValue(artifactInfoTask);
						}
					}
				}
			}

			// Wait for the read to finish
			Task<ArtifactInfo?> task = artifactInfoTask.Task ?? Task.FromResult<ArtifactInfo?>(null);
			return await task.WaitAsync(cancellationToken);
		}

		/// <summary>
		/// Reads a blob from an artifact
		/// </summary>
		/// <param name="artifact"></param>
		/// <param name="blobHash"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public async ValueTask<IBlobRef?> ReadBlobRefAsync(IArtifact artifact, IoHash blobHash, CancellationToken cancellationToken = default)
		{
			ArtifactInfo? artifactInfo = await GetArtifactInfoAsync(artifact, cancellationToken);
			if (artifactInfo == null)
			{
				return null;
			}

			IBlobRef<LeafChunkedDataNode>? blobRef;
			if (!artifactInfo.Blobs.TryGetValue(blobHash, out blobRef))
			{
				return null;
			}

			return blobRef;
		}

		async Task<ArtifactInfo?> ReadArtifactAsync(IArtifact artifact, CancellationToken cancellationToken)
		{
			IStorageClient? storageClient = null;
			try
			{
				Stopwatch timer = Stopwatch.StartNew();
				storageClient = _storageService.CreateClient(artifact.NamespaceId);

				IBlobRef<DirectoryNode>? target = await storageClient.TryReadRefAsync<DirectoryNode>(artifact.RefName, cancellationToken: cancellationToken);
				if (target == null)
				{
					return null;
				}

				List<UnsyncFile> files = new List<UnsyncFile>();
				await FindFilesAsync(new Utf8StringBuilder(), target, files, cancellationToken);

				Dictionary<IoHash, IBlobRef<LeafChunkedDataNode>> blocks = new Dictionary<IoHash, IBlobRef<LeafChunkedDataNode>>();
				foreach (UnsyncBlock block in files.SelectMany(x => x.Blocks))
				{
					blocks[block.Blob.Hash] = block.Blob;
				}

				_logger.LogDebug("Generated Unsync manifest for artifact {ArtifactId} in {Time:n1}ms", artifact.Id, timer.ElapsedMilliseconds);

				ArtifactInfo artifactInfo = new ArtifactInfo(storageClient, new UnsyncManifest(files), blocks.ToFrozenDictionary());
				storageClient = null;
				return artifactInfo;
			}
			finally
			{
				storageClient?.Dispose();
			}
		}

		static async Task FindFilesAsync(Utf8StringBuilder path, IBlobRef<DirectoryNode> directoryNodeRef, List<UnsyncFile> files, CancellationToken cancellationToken)
		{
			DirectoryNode directoryNode = await directoryNodeRef.ReadBlobAsync(cancellationToken);

			int initialPathLength = path.Length;

			foreach (FileEntry fileEntry in directoryNode.Files)
			{
				List<UnsyncBlock> blocks = new List<UnsyncBlock>();
				await FindBlocksAsync(0, fileEntry.Target, blocks, cancellationToken);

				bool readOnly = (fileEntry.Flags & FileEntryFlags.ReadOnly) != 0;
				bool executable = (fileEntry.Flags & FileEntryFlags.Executable) != 0;

				path.Append(fileEntry.Name);
				files.Add(new UnsyncFile(path.ToUtf8String().Clone(), readOnly, executable, fileEntry.Length, fileEntry.ModTime, blocks));
				path.Length = initialPathLength;
			}

			foreach (DirectoryEntry directoryEntry in directoryNode.Directories)
			{
				path.Append(directoryEntry.Name);
				path.Append('/');
				await FindFilesAsync(path, directoryEntry.Handle, files, cancellationToken);
				path.Length = initialPathLength;
			}
		}

		static async Task FindBlocksAsync(long offset, ChunkedDataNodeRef nodeRef, List<UnsyncBlock> blocks, CancellationToken cancellationToken)
		{
			if (nodeRef.Type == ChunkedDataNodeType.Leaf)
			{
				blocks.Add(new UnsyncBlock(offset, nodeRef.Length, nodeRef.RollingHash, nodeRef.GetLeafHandle()));
			}
			else
			{
				InteriorChunkedDataNode interiorNode = await nodeRef.Handle.ReadBlobAsync<InteriorChunkedDataNode>(cancellationToken: cancellationToken);
				foreach (ChunkedDataNodeRef childRef in interiorNode.Children)
				{
					await FindBlocksAsync(offset, childRef, blocks, cancellationToken);
					offset += childRef.Length;
				}
			}
		}
	}
}
