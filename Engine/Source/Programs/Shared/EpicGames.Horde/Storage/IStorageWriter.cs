// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Information about an alias to be added alongside a blob
	/// </summary>
	/// <param name="Name">Name of the alias</param>
	/// <param name="Rank">Rank of the alias</param>
	/// <param name="Data">Inline data to be stored for the alias</param>
	public record class AliasInfo(string Name, int Rank, ReadOnlyMemory<byte> Data)
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public AliasInfo(string name, int rank = 0) : this(name, rank, ReadOnlyMemory<byte>.Empty)
		{ }
	}

	/// <summary>
	/// Interface for writing new nodes to the store
	/// </summary>
	public interface IStorageWriter : IAsyncDisposable
	{
		/// <summary>
		/// Create another writer instance, allowing multiple threads to write in parallel.
		/// </summary>
		/// <returns>New writer instance</returns>
		IStorageWriter Fork();

		/// <summary>
		/// Flush any pending nodes to storage
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task FlushAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets an output buffer for writing.
		/// </summary>
		/// <param name="usedSize">Current size in the existing buffer that has been written to</param>
		/// <param name="desiredSize">Desired size of the returned buffer</param>
		/// <returns>Buffer to be written into.</returns>
		Memory<byte> GetOutputBuffer(int usedSize, int desiredSize);

		/// <summary>
		/// Finish writing a blob that has been written into the output buffer.
		/// </summary>
		/// <param name="type">Type of the node that was written</param>
		/// <param name="size">Used size of the buffer</param>
		/// <param name="references">References to other nodes</param>
		/// <param name="aliases">Aliases for this node</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the written node</returns>
		ValueTask<IBlobHandle> WriteBlobAsync(BlobType type, int size, IReadOnlyList<IBlobHandle> references, IReadOnlyList<AliasInfo> aliases, CancellationToken cancellationToken = default);
	}

	/// <summary>
	/// Default implementation of <see cref="IStorageWriter"/> which writes each node individually the the owning client.
	/// </summary>
	public sealed class DefaultStorageWriter : IStorageWriter
	{
		readonly IStorageClient _outer;
		readonly string _basePath;
		byte[] _data = Array.Empty<byte>();
		int _offset;

		/// <summary>
		/// Constructor
		/// </summary>
		public DefaultStorageWriter(IStorageClient outer, string? basePath)
		{
			_outer = outer;
			_basePath = basePath ?? String.Empty;

			if (!_basePath.EndsWith("/", StringComparison.Ordinal))
			{
				_basePath += "/";
			}
		}

		/// <inheritdoc/>
		public ValueTask DisposeAsync() => new ValueTask();

		/// <inheritdoc/>
		public Task FlushAsync(CancellationToken cancellationToken = default) => Task.CompletedTask;

		/// <inheritdoc/>
		public IStorageWriter Fork() => new DefaultStorageWriter(_outer, _basePath);

		/// <inheritdoc/>
		public Memory<byte> GetOutputBuffer(int usedSize, int desiredSize)
		{
			if (_offset + desiredSize > _data.Length)
			{
				byte[] newData = new byte[(desiredSize + 4095) & ~4095];
				_data.AsSpan(_offset, usedSize).CopyTo(newData);
				_data = newData;
				_offset = 0;
			}
			return _data.AsMemory(_offset);
		}

		/// <inheritdoc/>
		public async ValueTask<IBlobHandle> WriteBlobAsync(BlobType type, int size, IReadOnlyList<IBlobHandle> references, IReadOnlyList<AliasInfo> aliases, CancellationToken cancellationToken = default)
		{
			ReadOnlyMemory<byte> data = _data.AsMemory(_offset, size);
			_offset += size;

			using ReadOnlyMemoryStream stream = new ReadOnlyMemoryStream(data);

			IBlobHandle handle = await _outer.WriteBlobAsync(type, stream, references, _basePath, cancellationToken);
			foreach (AliasInfo aliasInfo in aliases)
			{
				await _outer.AddAliasAsync(aliasInfo.Name, handle, aliasInfo.Rank, aliasInfo.Data, cancellationToken);
			}
			return handle;
		}
	}

	/// <summary>
	/// Index of known nodes that can be used for deduplication.
	/// </summary>
	public sealed class DedupeStorageWriter : IStorageWriter
	{
		record BlobKey(IoHash Hash, BlobType Type);

		class DedupeCache
		{
			readonly int _maxKeys;
			readonly Queue<BlobKey> _blobKeys = new Queue<BlobKey>();
			readonly Dictionary<BlobKey, IBlobHandle> _blobKeyToHandle = new Dictionary<BlobKey, IBlobHandle>();

			public DedupeCache(int maxKeys)
			{
				_maxKeys = maxKeys;
				_blobKeys = new Queue<BlobKey>(maxKeys);
				_blobKeyToHandle = new Dictionary<BlobKey, IBlobHandle>(maxKeys);
			}

			internal void Add(BlobKey key, IBlobHandle handle)
			{
				BlobKey? prevKey;
				if (_blobKeys.Count == _maxKeys && _blobKeys.TryDequeue(out prevKey))
				{
					_blobKeyToHandle.Remove(prevKey);
				}
				_blobKeyToHandle.TryAdd(key, handle);
			}

			internal bool TryGetValue(BlobKey key, [NotNullWhen(true)] out IBlobHandle? handle) => _blobKeyToHandle.TryGetValue(key, out handle);
		}

		class WrappedHandle : IBlobHandle
		{
			public object _lockObject = new object();
			public IBlobHandle? _inner;

			/// <inheritdoc/>
			public IBlobHandle? Outer => _inner?.Outer;

			/// <inheritdoc/>
			public bool TryAppendIdentifier(Utf8StringBuilder builder)
			{
				return _inner?.TryAppendIdentifier(builder) ?? false;
			}

			/// <inheritdoc/>
			public ValueTask FlushAsync(CancellationToken cancellationToken)
			{
				if (_inner == null)
				{
					throw new InvalidOperationException();
				}
				else
				{
					return _inner.FlushAsync(cancellationToken);
				}
			}

			/// <inheritdoc/>
			public ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default)
			{
				if (_inner == null)
				{
					throw new InvalidOperationException();
				}
				else
				{
					return _inner.ReadAsync(cancellationToken);
				}
			}

			/// <inheritdoc/>
			public override bool Equals(object? obj) => _inner is not null && obj is WrappedHandle other && _inner == other._inner;

			/// <inheritdoc/>
			public override int GetHashCode() => HashCode.Combine((_inner is null) ? 0 : _inner.GetHashCode(), 1);
		}

		/// <summary>
		/// Default value for maximum number of keys
		/// </summary>
		public const int DefaultMaxKeys = 64 * 1024;

		readonly IStorageWriter _inner;
		readonly DedupeCache _cache;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="inner"></param>
		/// <param name="maxKeys"></param>
		public DedupeStorageWriter(IStorageWriter inner, int maxKeys = DefaultMaxKeys)
		{
			_inner = inner;
			_cache = new DedupeCache(maxKeys);
		}

		private DedupeStorageWriter(IStorageWriter inner, DedupeCache cache)
		{
			_inner = inner;
			_cache = cache;
		}

		/// <inheritdoc/>
		public ValueTask DisposeAsync() => _inner.DisposeAsync();

		/// <inheritdoc/>
		public Task FlushAsync(CancellationToken cancellationToken = default) => _inner.FlushAsync(cancellationToken);

		/// <inheritdoc/>
		public IStorageWriter Fork() => new DedupeStorageWriter(_inner.Fork(), _cache);

		/// <inheritdoc/>
		public Memory<byte> GetOutputBuffer(int usedSize, int desiredSize) => _inner.GetOutputBuffer(usedSize, desiredSize);

		/// <inheritdoc/>
		public async ValueTask<IBlobHandle> WriteBlobAsync(BlobType type, int size, IReadOnlyList<IBlobHandle> references, IReadOnlyList<AliasInfo> aliases, CancellationToken cancellationToken = default)
		{
			ReadOnlyMemory<byte> data = _inner.GetOutputBuffer(size, size).Slice(0, size);
			IoHash hash = IoHash.Compute(data.Span);
			BlobKey key = new BlobKey(hash, type);

			WrappedHandle? wrappedHandle;
			lock (_cache)
			{
				IBlobHandle? handle;
				if (_cache.TryGetValue(key, out handle))
				{
					return handle;
				}

				wrappedHandle = new WrappedHandle();
				_cache.Add(key, wrappedHandle);
			}

			wrappedHandle._inner = await _inner.WriteBlobAsync(type, size, references.ConvertAll(x => ((WrappedHandle)x)._inner!), aliases, cancellationToken);
			return wrappedHandle;
		}
	}

	/// <summary>
	/// Extension methods for <see cref="IStorageWriter"/>
	/// </summary>
	public static class StorageWriterExtensions
	{
		/// <summary>
		/// Wraps a <see cref="IStorageWriter"/> with a <see cref="DedupeStorageWriter"/>
		/// </summary>
		public static DedupeStorageWriter WithDedupe(this IStorageWriter writer, int maxKeys = DedupeStorageWriter.DefaultMaxKeys) => new DedupeStorageWriter(writer, maxKeys);

		/// <summary>
		/// Finish writing a node.
		/// </summary>
		/// <param name="writer">Writer instance to manipulate</param>
		/// <param name="type">Type of the node that was written</param>
		/// <param name="size">Used size of the buffer</param>
		/// <param name="references">References to other nodes</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the written node</returns>
		public static ValueTask<IBlobHandle> WriteBlobAsync(this IStorageWriter writer, BlobType type, int size, IReadOnlyList<IBlobHandle> references, CancellationToken cancellationToken = default)
		{
			return writer.WriteBlobAsync(type, size, references, Array.Empty<AliasInfo>(), cancellationToken);
		}

		/// <summary>
		/// Finish writing a node.
		/// </summary>
		/// <param name="writer">Writer instance to manipulate</param>
		/// <param name="type">Type of the node that was written</param>
		/// <param name="size">Used size of the buffer</param>
		/// <param name="references">References to other nodes</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Handle to the written node</returns>
		public static async ValueTask<HashedNodeRef<T>> WriteHashedNodeRefAsync<T>(this IStorageWriter writer, BlobType type, int size, IReadOnlyList<IBlobHandle> references, CancellationToken cancellationToken = default) where T : Node
		{
			IoHash hash = IoHash.Compute(writer.GetOutputBuffer(size, size).Span.Slice(0, size));
			IBlobHandle blobHandle = await WriteBlobAsync(writer, type, size, references, cancellationToken);
			return new HashedNodeRef<T>(hash, blobHandle);
		}
	}
}
