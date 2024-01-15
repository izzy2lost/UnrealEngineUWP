// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
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
		static ObjectKey GetBlobFile(string path) => new ObjectKey($"{path}.blob");

		/// <inheritdoc/>
		public Task<Stream> OpenAsync(string path, int offset, int? length, CancellationToken cancellationToken)
			=> _objectStore.OpenAsync(GetBlobFile(path), offset, length, cancellationToken);

		/// <summary>
		/// Maps a file into memory for reading, and returns a handle to it
		/// </summary>
		/// <param name="path">Path to the file</param>
		/// <param name="offset">Offset of the data to retrieve</param>
		/// <param name="length">Length of the data</param>
		/// <returns>Handle to the data. Must be disposed by the caller.</returns>
		public IReadOnlyMemoryOwner<byte> Read(string path, int offset, int? length)
			=> _objectStore.Read(GetBlobFile(path), offset, length);

		/// <inheritdoc/>
		public Task<IReadOnlyMemoryOwner<byte>> ReadAsync(string path, int offset, int? length, CancellationToken cancellationToken = default)
			=> _objectStore.ReadAsync(GetBlobFile(path), offset, length, cancellationToken);

		/// <inheritdoc/>
		public async Task<string> WriteAsync(Stream stream, string? prefix = null, CancellationToken cancellationToken = default)
		{
			string path = StorageHelpers.CreateUniqueName(prefix);
			await WriteExplicitPathAsync(path, stream, cancellationToken);
			return path;
		}

		/// <inheritdoc/>
		public Task WriteExplicitPathAsync(string path, Stream stream, CancellationToken cancellationToken = default)
			=> _objectStore.WriteAsync(GetBlobFile(path), stream, cancellationToken);

		/// <inheritdoc/>
		public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken)
			=> _objectStore.ExistsAsync(GetBlobFile(path), cancellationToken);

		/// <summary>
		/// Delete a file from the store
		/// </summary>
		/// <param name="path"></param>
		public void Delete(string path)
			=> _objectStore.Delete(GetBlobFile(path));

		/// <inheritdoc/>
		public Task DeleteAsync(string path, CancellationToken cancellationToken)
			=> _objectStore.DeleteAsync(GetBlobFile(path), cancellationToken);

		/// <inheritdoc/>
		public async IAsyncEnumerable<string> EnumerateAsync([EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			await foreach (ObjectKey locator in _objectStore.EnumerateAsync(cancellationToken))
			{
				yield return locator.Path.ToString();
			}
		}

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetReadRedirectAsync(string path, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public ValueTask<(string, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public void GetStats(StorageStats stats)
			=> _objectStore.GetStats(stats);
	}
}

