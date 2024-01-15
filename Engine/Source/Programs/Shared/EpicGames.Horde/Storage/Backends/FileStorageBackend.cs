// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage.ObjectStores;

namespace EpicGames.Horde.Storage.Backends
{
	/// <summary>
	/// Storage backend that utilizes the local filesystem
	/// </summary>
	public sealed class FileStorageBackend : IStorageBackend
	{
		readonly FileObjectStore _objectStore;

		/// <inheritdoc/>
		public bool SupportsRedirects => false;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="baseDir">Base directory for the store</param>
		public FileStorageBackend(DirectoryReference baseDir)
		{
			_objectStore = new FileObjectStore(baseDir);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_objectStore.Dispose();
		}

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

		/// <inheritdoc/>
		public void GetStats(StorageStats stats)
			=> _objectStore.GetStats(stats);
	}
}

