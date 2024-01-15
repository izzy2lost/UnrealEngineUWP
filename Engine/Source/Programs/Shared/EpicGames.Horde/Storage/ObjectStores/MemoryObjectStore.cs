// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.ObjectStores
{
	/// <summary>
	/// In-memory implementation of a storage backend
	/// </summary>
	public sealed class MemoryObjectStore : IObjectStore
	{
		readonly ConcurrentDictionary<ObjectKey, byte[]> _pathToData = new ConcurrentDictionary<ObjectKey, byte[]>();

		/// <summary>
		/// Read only access to the stored blobs
		/// </summary>
		public IReadOnlyDictionary<ObjectKey, byte[]> Blobs => _pathToData; 

		/// <inheritdoc/>
		public bool SupportsRedirects => false;

		/// <inheritdoc/>
		public void Dispose()
		{
		}

		/// <inheritdoc/>
		public Task<Stream> OpenAsync(ObjectKey locator, int offset, int? length, CancellationToken cancellationToken)
		{
			return Task.FromResult<Stream>(new ReadOnlyMemoryStream(GetData(locator, offset, length)));
		}

		/// <inheritdoc/>
		public Task<IReadOnlyMemoryOwner<byte>> ReadAsync(ObjectKey locator, int offset, int? length, CancellationToken cancellationToken)
		{
			return Task.FromResult(ReadOnlyMemoryOwner.Create(GetData(locator, offset, length)));
		}

		ReadOnlyMemory<byte> GetData(ObjectKey locator, int offset, int? length)
		{
			ReadOnlyMemory<byte> data = _pathToData[locator].AsMemory(offset);
			if (length != null && length.Value < data.Length)
			{
				data = data.Slice(0, length.Value);
			}
			return data;
		}

		/// <inheritdoc/>
		public async Task WriteAsync(ObjectKey locator, Stream stream, CancellationToken cancellationToken = default)
		{
			using (MemoryStream buffer = new MemoryStream())
			{
				await stream.CopyToAsync(buffer, cancellationToken);
				_pathToData[locator] = buffer.ToArray();
			}
		}

		/// <inheritdoc/>
		public Task<bool> ExistsAsync(ObjectKey locator, CancellationToken cancellationToken)
		{
			return Task.FromResult(_pathToData.ContainsKey(locator));
		}

		/// <inheritdoc/>
		public Task DeleteAsync(ObjectKey locator, CancellationToken cancellationToken)
		{
			_pathToData.TryRemove(locator, out _);
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public async IAsyncEnumerable<ObjectKey> EnumerateAsync([EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			foreach (ObjectKey locator in _pathToData.Keys)
			{
				yield return locator;
				cancellationToken.ThrowIfCancellationRequested();
				await Task.Yield();
			}
		}

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetReadRedirectAsync(ObjectKey path, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetWriteRedirectAsync(ObjectKey path, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public void GetStats(StorageStats stats) { }
	}
}
