// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Storage.Backends;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Implementation of a local disk cache which can be shared by multiple backends
	/// </summary>
	public sealed class StorageBackendCache : IDisposable
	{
		class Item
		{
			public string Key { get; }
			public string Path { get; }
			public long Length { get; }
			public LinkedListNode<Item> ListNode { get; }

			public Item(string key, string path, long length)
			{
				Key = key;
				Path = path;
				Length = length;
				ListNode = new LinkedListNode<Item>(this);
			}
		}

		class PendingItem
		{
			int _refCount;
			readonly BackgroundTask _readTask;

			public PendingItem(BackgroundTask readTask) => _readTask = readTask;
			public void AddRef() => Interlocked.Increment(ref _refCount);

			public async ValueTask ReleaseAsync()
			{
				if (Interlocked.Decrement(ref _refCount) == 0)
				{
					await _readTask.DisposeAsync();
				}
			}

			public Task WaitAsync(CancellationToken cancellationToken) => _readTask.WaitAsync(cancellationToken);
		}

		sealed class ObjectStoreWrapper : IObjectStore
		{
			readonly string _keyPrefix;
			readonly StorageBackendCache _cacheStorage;
			readonly IObjectStore _inner;

			public bool SupportsRedirects => _inner.SupportsRedirects;

			public ObjectStoreWrapper(string keyPrefix, StorageBackendCache cacheStorage, IObjectStore inner)
			{
				_keyPrefix = keyPrefix;
				_cacheStorage = cacheStorage;
				_inner = inner;
			}

			public void Dispose() => _inner.Dispose();

			public async Task<Stream> OpenAsync(ObjectKey key, int offset, int? length, CancellationToken cancellationToken = default)
			{
				IReadOnlyMemoryOwner<byte> storageObject = await ReadAsync(key, offset, length, cancellationToken);
				return storageObject.AsStream();
			}

			public async Task<IReadOnlyMemoryOwner<byte>> ReadAsync(ObjectKey key, int offset, int? length, CancellationToken cancellationToken = default)
			{
#pragma warning disable CA2000 // Dispose objects before losing scope
				IReadOnlyMemoryOwner<byte> storageObject = await _cacheStorage.ReadAsync($"{_keyPrefix}{key}", ctx => _inner.OpenAsync(key, ctx), cancellationToken);
#pragma warning restore CA2000 // Dispose objects before losing scope
				return storageObject.Slice(offset, length);
			}

			public Task WriteAsync(ObjectKey key, Stream stream, CancellationToken cancellationToken = default) => _inner.WriteAsync(key, stream, cancellationToken);
			public Task DeleteAsync(ObjectKey key, CancellationToken cancellationToken = default) => _inner.DeleteAsync(key, cancellationToken);
			public IAsyncEnumerable<ObjectKey> EnumerateAsync(CancellationToken cancellationToken = default) => _inner.EnumerateAsync(cancellationToken);
			public Task<bool> ExistsAsync(ObjectKey key, CancellationToken cancellationToken = default) => _inner.ExistsAsync(key, cancellationToken);
			public ValueTask<Uri?> TryGetReadRedirectAsync(ObjectKey key, CancellationToken cancellationToken = default) => _inner.TryGetReadRedirectAsync(key, cancellationToken);
			public ValueTask<Uri?> TryGetWriteRedirectAsync(ObjectKey key, CancellationToken cancellationToken = default) => _inner.TryGetWriteRedirectAsync(key, cancellationToken);

			public void GetStats(StorageStats stats)
			{
				_inner.GetStats(stats);
				_cacheStorage.GetStats(stats);
			}
		}

		sealed class BackendWrapper : IStorageBackend
		{
			readonly string _keyPrefix;
			readonly StorageBackendCache _cacheStorage;
			readonly IStorageBackend _inner;

			public bool SupportsRedirects => _inner.SupportsRedirects;

			public BackendWrapper(string keyPrefix, StorageBackendCache cacheStorage, IStorageBackend inner)
			{
				_keyPrefix = keyPrefix;
				_cacheStorage = cacheStorage;
				_inner = inner;
			}

			public void Dispose() => _inner.Dispose();

			public async Task<Stream> OpenAsync(string path, int offset, int? length, CancellationToken cancellationToken = default)
			{
				IReadOnlyMemoryOwner<byte> storageObject = await ReadAsync(path, offset, length, cancellationToken);
				return storageObject.AsStream();
			}

			public async Task<IReadOnlyMemoryOwner<byte>> ReadAsync(string path, int offset, int? length, CancellationToken cancellationToken = default)
			{
#pragma warning disable CA2000 // Dispose objects before losing scope
				IReadOnlyMemoryOwner<byte> storageObject = await _cacheStorage.ReadAsync($"{_keyPrefix}{path}", ctx => _inner.OpenAsync(path, ctx), cancellationToken);
#pragma warning restore CA2000 // Dispose objects before losing scope
				return storageObject.Slice(offset, length);
			}

			public Task<string> WriteAsync(Stream stream, string? prefix = null, CancellationToken cancellationToken = default) => _inner.WriteAsync(stream, prefix, cancellationToken);

#pragma warning disable CS0618 // Type or member is obsolete
			public Task WriteExplicitPathAsync(string path, Stream stream, CancellationToken cancellationToken = default) => _inner.WriteExplicitPathAsync(path, stream, cancellationToken);
#pragma warning restore CS0618 // Type or member is obsolete

			public Task DeleteAsync(string path, CancellationToken cancellationToken = default) => _inner.DeleteAsync(path, cancellationToken);
			public IAsyncEnumerable<string> EnumerateAsync(CancellationToken cancellationToken = default) => _inner.EnumerateAsync(cancellationToken);
			public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken = default) => _inner.ExistsAsync(path, cancellationToken);
			public ValueTask<Uri?> TryGetReadRedirectAsync(string path, CancellationToken cancellationToken = default) => _inner.TryGetReadRedirectAsync(path, cancellationToken);
			public ValueTask<(string, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default) => _inner.TryGetWriteRedirectAsync(prefix, cancellationToken);

			public void GetStats(StorageStats stats)
			{
				_inner.GetStats(stats);
				_cacheStorage.GetStats(stats);
			}
		}

		object LockObject => _items;

		readonly FileStorageBackend _backend;
		readonly long _maxSize;
		readonly ILogger _logger;

		readonly LinkedList<Item> _items = new LinkedList<Item>();
		readonly Dictionary<string, Item> _pathToItem = new Dictionary<string, Item>(StringComparer.Ordinal);
		readonly Dictionary<string, PendingItem> _pathToPendingItem = new Dictionary<string, PendingItem>(StringComparer.Ordinal);

		long _size;
		long _cleanCount;
		long _cleanTimeTicks;
		long _fetchTimeTicks;
		long _writeTimeTicks;
		long _fetchBytes;

		internal IEnumerable<string> Items => _items.Select(x => x.Key);

		/// <summary>
		/// Constructor
		/// </summary>
		public StorageBackendCache()
			: this(null, null)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public StorageBackendCache(DirectoryReference? cacheDir, long? maxSize)
			: this(cacheDir, maxSize, NullLogger.Instance)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="cacheDir">Directory to storage cache files. Will be cleared on startup. Defaults to a randomly generated directory in the users's temp folder.</param>
		/// <param name="maxSize">Maximum size of the cache. Defaults to 50mb.</param>
		/// <param name="logger">Logger for error/warning messages</param>
		public StorageBackendCache(DirectoryReference? cacheDir, long? maxSize, ILogger logger)
		{
			cacheDir ??= new DirectoryReference(Path.Combine(Path.GetTempPath(), $"horde-{Guid.NewGuid().ToString("n")}"));
			FileUtils.ForceDeleteDirectoryContents(cacheDir);

			_backend = new FileStorageBackend(cacheDir);
			_maxSize = maxSize ?? (50 * 1024 * 1024);
			_logger = logger;
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			foreach (Item item in _items)
			{
				_backend.Delete(item.Path);
			}
			_backend.Dispose();
		}

		/// <summary>
		/// Wraps an onject store in another store that routes requests through the cache
		/// </summary>
		/// <param name="keyPrefix">Prefix for items in this cache</param>
		/// <param name="store">Backend to wrap</param>
		public IObjectStore CreateWrapper(string keyPrefix, IObjectStore store)
		{
			return new ObjectStoreWrapper(keyPrefix, this, store);
		}

		/// <summary>
		/// Wraps a storage backend in another backend that routes requests through the cache
		/// </summary>
		/// <param name="keyPrefix">Prefix for items in this cache</param>
		/// <param name="backend">Backend to wrap</param>
		public IStorageBackend CreateWrapper(string keyPrefix, IStorageBackend backend)
		{
			return new BackendWrapper(keyPrefix, this, backend);
		}

		/// <summary>
		/// Wraps a storage backend in another backend that routes requests through the cache
		/// </summary>
		/// <param name="keyPrefix">Prefix for items in this cache</param>
		/// <param name="backend">Backend to wrap</param>
		/// <param name="cache">The cache instance. May be null.</param>
		public static IStorageBackend CreateWrapper(string keyPrefix, IStorageBackend backend, StorageBackendCache? cache)
		{
			return cache?.CreateWrapper(keyPrefix, backend) ?? backend;
		}

		/// <inheritdoc/>
		public async Task<IReadOnlyMemoryOwner<byte>> ReadAsync(string key, Func<CancellationToken, Task<Stream>> createStreamAsync, CancellationToken cancellationToken = default)
		{
			for (; ; )
			{
				PendingItem? pendingItem;
				lock (LockObject)
				{
					Item? item;
					if (_pathToItem.TryGetValue(key, out item))
					{
						_items.Remove(item.ListNode);
						_items.AddFirst(item.ListNode);
						return _backend.Read(item.Path, 0, null);
					}

					if (!_pathToPendingItem.TryGetValue(key, out pendingItem))
					{
						pendingItem = new PendingItem(BackgroundTask.StartNew(x => ReadIntoCacheAsync(key, createStreamAsync, x)));
						_pathToPendingItem.Add(key, pendingItem);
					}

					pendingItem.AddRef();
				}

				try
				{
					await pendingItem.WaitAsync(cancellationToken);
				}
				finally
				{
					await pendingItem.ReleaseAsync();
				}
			}
		}

		async Task ReadIntoCacheAsync(string key, Func<CancellationToken, Task<Stream>> createStreamAsync, CancellationToken cancellationToken)
		{
			long openStartTicks = Stopwatch.GetTimestamp();
			using Stream stream = await createStreamAsync(cancellationToken);
			long totalLength = stream.Length;
			long openFinishTicks = Stopwatch.GetTimestamp();
			Interlocked.Add(ref _fetchTimeTicks, openFinishTicks - openStartTicks);

			long cleanStartTicks = openFinishTicks;
			lock (LockObject)
			{
				_size += totalLength;

				LinkedListNode<Item>? lastNode = _items.Last;
				while (_size > _maxSize && lastNode != null)
				{
					LinkedListNode<Item> node = lastNode;
					lastNode = lastNode.Previous;

					Item item = node.Value;
					try
					{
						_backend.Delete(item.Path);

						_pathToItem.Remove(item.Key);
						_items.Remove(node);

						_size -= item.Length;
						_cleanCount++;
					}
					catch (Exception ex)
					{
						_logger.LogDebug(ex, "Unable to delete cache item {Path}: {Message}", item.Path, ex.Message);
					}
				}
			}
			long cleanFinishTicks = Stopwatch.GetTimestamp();
			Interlocked.Add(ref _cleanTimeTicks, cleanFinishTicks - cleanStartTicks);

			long writeStartTicks = cleanFinishTicks;
			string path = await _backend.WriteAsync(stream, cancellationToken: cancellationToken);
			lock (LockObject)
			{
				Item item = new Item(key, path, totalLength);
				_items.AddFirst(item.ListNode);
				_pathToItem.Add(key, item);

				_pathToPendingItem.Remove(key);
			}
			Interlocked.Add(ref _fetchBytes, totalLength);
			long writeFinishTicks = Stopwatch.GetTimestamp();
			Interlocked.Add(ref _writeTimeTicks, writeFinishTicks - writeStartTicks);
		}

		/// <summary>
		/// Get stats for the cache operation
		/// </summary>
		public void GetStats(StorageStats stats)
		{
			stats.Add("Cache clean count", _cleanCount);
			stats.Add("Cache fetch time (ms)", (_fetchTimeTicks * 1000) / Stopwatch.Frequency);
			stats.Add("Cache clean time (ms)", (_cleanTimeTicks * 1000) / Stopwatch.Frequency);
			stats.Add("Cache write time (ms)", (_writeTimeTicks * 1000) / Stopwatch.Frequency);
			stats.Add("Cache fetch bytes", _fetchBytes);
		}
	}
}
