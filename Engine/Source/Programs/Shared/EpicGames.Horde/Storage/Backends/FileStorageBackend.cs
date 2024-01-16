// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage.ObjectStores;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Storage.Backends
{
	/// <summary>
	/// Storage backend that utilizes the local filesystem
	/// </summary>
	public sealed class FileStorageBackend : IStorageBackend
	{
		readonly DirectoryReference _rootDir;
		readonly FileObjectStore _objectStore;
		readonly ILogger _logger;

		/// <inheritdoc/>
		public bool SupportsRedirects => false;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="rootDir">Base directory for the store</param>
		/// <param name="logger">Logger interface</param>
		public FileStorageBackend(DirectoryReference rootDir, ILogger logger)
		{
			_rootDir = rootDir;
			_objectStore = new FileObjectStore(rootDir);
			_logger = logger;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_objectStore.Dispose();
		}

		#region Blobs

		/// <summary>
		/// Gets the path for storing a file on disk
		/// </summary>
		static ObjectKey GetBlobFile(BlobLocator locator) => new ObjectKey(locator.Path);

		/// <inheritdoc/>
		public Task<Stream> OpenBlobAsync(BlobLocator locator, int offset, int? length, CancellationToken cancellationToken)
			=> _objectStore.OpenAsync(GetBlobFile(locator), offset, length, cancellationToken);

		/// <summary>
		/// Maps a file into memory for reading, and returns a handle to it
		/// </summary>
		/// <param name="locator">Path to the file</param>
		/// <param name="offset">Offset of the data to retrieve</param>
		/// <param name="length">Length of the data</param>
		/// <returns>Handle to the data. Must be disposed by the caller.</returns>
		public IReadOnlyMemoryOwner<byte> Read(BlobLocator locator, int offset, int? length)
			=> _objectStore.Read(GetBlobFile(locator), offset, length);

		/// <inheritdoc/>
		public Task<IReadOnlyMemoryOwner<byte>> ReadBlobAsync(BlobLocator locator, int offset, int? length, CancellationToken cancellationToken = default)
			=> _objectStore.ReadAsync(GetBlobFile(locator), offset, length, cancellationToken);

		/// <inheritdoc/>
		public async Task<BlobLocator> WriteBlobAsync(Stream stream, string? prefix = null, CancellationToken cancellationToken = default)
		{
			BlobLocator locator = StorageHelpers.CreateUniqueLocator(prefix);
			await _objectStore.WriteAsync(GetBlobFile(locator), stream, cancellationToken);
			return locator;
		}

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetBlobReadRedirectAsync(BlobLocator path, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public ValueTask<(BlobLocator, Uri)?> TryGetBlobWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default) => default;

		#endregion

		/// <summary>
		/// Reads a ref from a file on disk
		/// </summary>
		public static async ValueTask<BlobLocator> ReadRefAsync(FileReference file)
		{
			string text = await FileReference.ReadAllTextAsync(file);
			return new BlobLocator(text);
		}

		FileReference GetRefFile(RefName name) => FileReference.Combine(_rootDir, name.ToString() + ".ref");

		#region Aliases

		/// <inheritdoc/>
		public Task AddAliasAsync(string name, BlobLocator locator, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default)
			=> throw new NotSupportedException("File storage client does not currently support aliases.");

		/// <inheritdoc/>
		public Task RemoveAliasAsync(string name, BlobLocator locator, CancellationToken cancellationToken = default)
			=> throw new NotSupportedException("File storage client does not currently support aliases.");

		/// <inheritdoc/>
		public Task<BlobAliasLocator[]> FindAliasesAsync(string alias, int? maxResults = null, CancellationToken cancellationToken = default)
			=> throw new NotSupportedException("File storage client does not currently support aliases.");

		#endregion

		#region Refs

		/// <inheritdoc/>
		public Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default)
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
		public async Task<BlobLocator?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			FileReference file = GetRefFile(name);
			if (!FileReference.Exists(file))
			{
				return null;
			}

			_logger.LogInformation("Reading {File}", file);
			string[] lines = await FileReference.ReadAllLinesAsync(file, cancellationToken);

			return new BlobLocator(lines[0].Trim());
		}

		/// <inheritdoc/>
		public async Task WriteRefAsync(RefName name, BlobLocator locator, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
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

		#endregion

		/// <inheritdoc/>
		public void GetStats(StorageStats stats)
			=> _objectStore.GetStats(stats);
	}
}