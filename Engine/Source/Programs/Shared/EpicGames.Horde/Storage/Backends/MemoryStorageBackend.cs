// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
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
		class StorageObject : IStorageObject
		{
			public ReadOnlyMemory<byte> Data { get; }
			public StorageObject(ReadOnlyMemory<byte> data) => Data = data;
			public void Dispose() { }
		}

		/// <summary>
		/// Data storage
		/// </summary>
		readonly ConcurrentDictionary<string, byte[]> _pathToData = new ConcurrentDictionary<string, byte[]>();

		/// <summary>
		/// Read only access to the stored blobs
		/// </summary>
		public IReadOnlyDictionary<string, byte[]> Blobs => _pathToData; 

		/// <inheritdoc/>
		public bool SupportsRedirects => false;

		/// <inheritdoc/>
		public void Dispose()
		{
		}

		/// <inheritdoc/>
		public Task<Stream> OpenAsync(string path, int offset, int? length, CancellationToken cancellationToken)
		{
			return Task.FromResult<Stream>(new ReadOnlyMemoryStream(GetData(path, offset, length)));
		}

		/// <inheritdoc/>
		public Task<IStorageObject> ReadAsync(string path, int offset, int? length, CancellationToken cancellationToken)
		{
			return Task.FromResult<IStorageObject>(new StorageObject(GetData(path, offset, length)));
		}

		ReadOnlyMemory<byte> GetData(string path, int offset, int? length)
		{
			ReadOnlyMemory<byte> data = _pathToData[path].AsMemory(offset);
			if (length != null && length.Value < data.Length)
			{
				data = data.Slice(0, length.Value);
			}
			return data;
		}

		/// <inheritdoc/>
		public async Task<string> WriteAsync(Stream stream, string? prefix = null, CancellationToken cancellationToken = default)
		{
			string path = StorageHelpers.CreateUniqueName(prefix);
			await WriteExplicitPathAsync(path, stream, cancellationToken);
			return path;
		}

		/// <inheritdoc/>
		public async Task WriteExplicitPathAsync(string path, Stream stream, CancellationToken cancellationToken = default)
		{
			using (MemoryStream buffer = new MemoryStream())
			{
				await stream.CopyToAsync(buffer, cancellationToken);
				_pathToData[path] = buffer.ToArray();
			}
		}

		/// <inheritdoc/>
		public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken)
		{
			return Task.FromResult(_pathToData.ContainsKey(path));
		}

		/// <inheritdoc/>
		public Task DeleteAsync(string path, CancellationToken cancellationToken)
		{
			_pathToData.TryRemove(path, out _);
			return Task.CompletedTask;
		}

		/// <inheritdoc/>
		public async IAsyncEnumerable<string> EnumerateAsync([EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			foreach (string path in _pathToData.Keys)
			{
				yield return path;
				cancellationToken.ThrowIfCancellationRequested();
				await Task.Yield();
			}
		}

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetReadRedirectAsync(string path, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public ValueTask<(string, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default) => default;

		/// <inheritdoc/>
		public void GetStats(StorageStats stats) { }
	}
}
