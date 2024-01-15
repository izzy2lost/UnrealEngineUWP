// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Backends
{
	/// <summary>
	/// In-memory implementation of a storage backend
	/// </summary>
	public sealed class MemoryStorageBackend : IStorageBackend
	{
		/// <summary>
		/// Data storage
		/// </summary>
		readonly ConcurrentDictionary<BlobLocator, byte[]> _locatorToData = new ConcurrentDictionary<BlobLocator, byte[]>();

		/// <summary>
		/// Read only access to the stored blobs
		/// </summary>
		public IReadOnlyDictionary<BlobLocator, byte[]> Blobs => _locatorToData; 

		/// <inheritdoc/>
		public bool SupportsRedirects => false;

		/// <inheritdoc/>
		public void Dispose()
		{
		}

		/// <inheritdoc/>
		public Task<Stream> OpenBlobAsync(BlobLocator locator, int offset, int? length, CancellationToken cancellationToken)
		{
			return Task.FromResult<Stream>(new ReadOnlyMemoryStream(GetData(locator, offset, length)));
		}

		/// <inheritdoc/>
		public Task<IReadOnlyMemoryOwner<byte>> ReadBlobAsync(BlobLocator locator, int offset, int? length, CancellationToken cancellationToken)
		{
			return Task.FromResult(ReadOnlyMemoryOwner.Create(GetData(locator, offset, length)));
		}

		ReadOnlyMemory<byte> GetData(BlobLocator locator, int offset, int? length)
		{
			ReadOnlyMemory<byte> data = _locatorToData[locator].AsMemory(offset);
			if (length != null && length.Value < data.Length)
			{
				data = data.Slice(0, length.Value);
			}
			return data;
		}

		/// <inheritdoc/>
		public async Task<BlobLocator> WriteBlobAsync(Stream stream, string? prefix = null, CancellationToken cancellationToken = default)
		{
			BlobLocator locator = StorageHelpers.CreateUniqueLocator(prefix);
			using (MemoryStream buffer = new MemoryStream())
			{
				await stream.CopyToAsync(buffer, cancellationToken);
				_locatorToData[locator] = buffer.ToArray();
			}
			return locator;
		}

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetBlobReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public ValueTask<(BlobLocator, Uri)?> TryGetBlobWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public void GetStats(StorageStats stats) { }
	}
}
