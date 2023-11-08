// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage.Backends;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Storage.Clients
{
	/// <summary>
	/// Implementation of <see cref="IStorageClient"/> which writes data to files on disk.
	/// </summary>
	public class FileStorageClient : KeyValueStorageClient
	{
		readonly DirectoryReference _rootDir;
		readonly FileStorageBackend _backend;
		readonly ILogger _logger;

		/// <inheritdoc/>
		public override bool SupportsRedirects => false;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="rootDir">Root directory for storing blobs</param>
		/// <param name="logger">Logger interface</param>
		public FileStorageClient(DirectoryReference rootDir, ILogger logger)
		{
			_rootDir = rootDir;
			_backend = new FileStorageBackend(rootDir);
			_logger = logger;

			DirectoryReference.CreateDirectory(_rootDir);
		}

		/// <inheritdoc/>
		protected override void Dispose(bool disposing)
		{
			base.Dispose(disposing);

			if (disposing)
			{
				_backend.Dispose();
			}
		}

		/// <summary>
		/// Reads a ref from a file on disk
		/// </summary>
		public static async ValueTask<BlobLocator> ReadRefAsync(FileReference file)
		{
			string text = await FileReference.ReadAllTextAsync(file);
			return new BlobLocator(text);
		}

		FileReference GetRefFile(RefName name) => FileReference.Combine(_rootDir, name.ToString() + ".ref");

		#region Blobs

		/// <inheritdoc/>
		public override async ValueTask<BlobData> ReadBlobAsync(BlobLocator locator, CancellationToken cancellationToken = default)
		{
			IReadOnlyMemoryOwner<byte> owner = await _backend.ReadAsync(locator.ToString(), cancellationToken);
			return new BlobDataWithOwner(BlobType.Leaf, owner.Memory, Array.Empty<IBlobHandle>(), owner);
		}

		/// <inheritdoc/>
		public override async ValueTask<IBlobHandle> WriteBlobAsync(BlobType type, Stream stream, IReadOnlyList<IBlobHandle> references, string? basePath = null, CancellationToken cancellationToken = default)
		{
			string path = await _backend.WriteAsync(stream, basePath, cancellationToken);
			return CreateBlobHandle(new BlobLocator(path));
		}

		/// <inheritdoc/>
		public override ValueTask<Uri?> TryGetReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public override ValueTask<(BlobLocator, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default) => default;

		#endregion

		#region Aliases

		/// <inheritdoc/>
		public override Task AddAliasAsync(string name, IBlobHandle handle, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException("File storage client does not currently support aliases.");
		}

		/// <inheritdoc/>
		public override Task RemoveAliasAsync(string name, IBlobHandle handle, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException("File storage client does not currently support aliases.");
		}

		/// <inheritdoc/>
		public override Task<BlobAlias[]> FindAliasesAsync(string alias, int? maxResults = null, CancellationToken cancellationToken = default)
		{
			throw new NotSupportedException("File storage client does not currently support aliases.");
		}

		#endregion

		#region Refs

		/// <inheritdoc/>
		public override Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default)
		{
			FileInfo file = GetRefFile(name).ToFileInfo();
			if (file.Exists)
			{
				file.Delete();
				return Task.FromResult(true);
			}
			return Task.FromResult(false);
		}

		/// <inheritdoc/>
		public override async Task<IBlobHandle?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			FileReference file = GetRefFile(name);
			if (!FileReference.Exists(file))
			{
				return null;
			}

			_logger.LogInformation("Reading {File}", file);
			string[] lines = await FileReference.ReadAllLinesAsync(file, cancellationToken);

			IBlobHandle handle = CreateBlobHandle(new BlobLocator(lines[0].Trim()));
			return handle;
		}

		/// <inheritdoc/>
		public override async Task WriteRefAsync(RefName name, IBlobHandle target, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			await target.FlushAsync(cancellationToken);
			BlobLocator locator = target.GetLocator();

			FileReference file = GetRefFile(name);
			DirectoryReference.CreateDirectory(file.Directory);
			_logger.LogInformation("Writing {File}", file);

			for (int attempt = 0; ; attempt++)
			{
				try
				{
					await FileReference.WriteAllTextAsync(file, locator.ToString());
					break;
				}
				catch (IOException ex) when (attempt < 3)
				{
					_logger.LogDebug(ex, "Unable to write to {File}; retrying...", file);
					await Task.Delay(100 * attempt, cancellationToken);
				}
			}
		}

		/// <inheritdoc/>
		public override void GetStats(StorageStats stats)
		{
		}

		#endregion
	}
}
