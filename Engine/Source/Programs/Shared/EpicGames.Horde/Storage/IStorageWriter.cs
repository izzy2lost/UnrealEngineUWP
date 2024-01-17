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
	/// Index of known nodes that can be used for deduplication.
	/// </summary>
	public sealed class DedupeStorageWriter : BlobWriter
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
			public bool TryGetLocator(out BlobLocator locator)
				=> _inner!.TryGetLocator(out locator);

			/// <inheritdoc/>
			public ValueTask FlushAsync(CancellationToken cancellationToken)
				=> _inner!.FlushAsync(cancellationToken);

			/// <inheritdoc/>
			public ValueTask<BlobData> ReadBlobDataAsync(CancellationToken cancellationToken = default)
				=> _inner!.ReadBlobDataAsync(cancellationToken);

			/// <inheritdoc/>
			public override bool Equals(object? obj) => _inner is not null && obj is WrappedHandle other && _inner == other._inner;

			/// <inheritdoc/>
			public override int GetHashCode() => HashCode.Combine((_inner is null) ? 0 : _inner.GetHashCode(), 1);
		}

		/// <summary>
		/// Default value for maximum number of keys
		/// </summary>
		public const int DefaultMaxKeys = 64 * 1024;

		readonly BlobWriter _inner;
		readonly DedupeCache _cache;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="inner"></param>
		/// <param name="maxKeys"></param>
		public DedupeStorageWriter(IBlobWriter inner, int maxKeys = DefaultMaxKeys)
		{
			_inner = (BlobWriter)inner;
			_cache = new DedupeCache(maxKeys);
		}

		private DedupeStorageWriter(IBlobWriter inner, DedupeCache cache)
		{
			_inner = (BlobWriter)inner;
			_cache = cache;
		}

		/// <inheritdoc/>
		public override ValueTask DisposeAsync() => _inner.DisposeAsync();

		/// <inheritdoc/>
		public override Task FlushAsync(CancellationToken cancellationToken = default) => _inner.FlushAsync(cancellationToken);

		/// <inheritdoc/>
		public override IBlobWriter Fork() => new DedupeStorageWriter(_inner.Fork(), _cache);

		/// <inheritdoc/>
		public override Memory<byte> GetOutputBuffer(int usedSize, int desiredSize) => _inner.GetOutputBuffer(usedSize, desiredSize);

		/// <inheritdoc/>
		public override async ValueTask<IBlobHandle> WriteBlobAsync(BlobType type, int size, IReadOnlyList<IBlobHandle> references, IReadOnlyList<AliasInfo> aliases, CancellationToken cancellationToken = default)
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

			wrappedHandle._inner = await _inner.WriteBlobAsync(type, size, references.ConvertAll(x => ((WrappedHandle)x.Unwrap())._inner!), aliases, cancellationToken);
			return wrappedHandle;
		}
	}

	/// <summary>
	/// Extension methods for <see cref="IBlobWriter"/>
	/// </summary>
	public static class StorageWriterExtensions
	{
		/// <summary>
		/// Wraps a <see cref="IBlobWriter"/> with a <see cref="DedupeStorageWriter"/>
		/// </summary>
		public static DedupeStorageWriter WithDedupe(this IBlobWriter writer, int maxKeys = DedupeStorageWriter.DefaultMaxKeys) => new DedupeStorageWriter(writer, maxKeys);
	}
}
