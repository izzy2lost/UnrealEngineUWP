// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Horde.Storage.Backends;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
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

			public Task WaitAsync(CancellationToken cancellationToken) => _readTask.Task.WaitAsync(cancellationToken);
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
				IStorageObject storageObject = await ReadAsync(path, offset, length, cancellationToken);
				return storageObject.CreateStream();
			}

			public async Task<IStorageObject> ReadAsync(string path, int offset, int? length, CancellationToken cancellationToken = default)
			{
				IStorageObject storageObject = await _cacheStorage.ReadAsync($"{_keyPrefix}{path}", ctx => _inner.OpenAsync(path, ctx), cancellationToken);
				return storageObject.CreateSlice(offset, length);
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
		}

		object LockObject => _items;

		readonly FileStorageBackend _backend;
		readonly long _maxSize;
		readonly ILogger _logger;

		readonly LinkedList<Item> _items = new LinkedList<Item>();
		readonly Dictionary<string, Item> _pathToItem = new Dictionary<string, Item>(StringComparer.Ordinal);
		readonly Dictionary<string, PendingItem> _pathToPendingItem = new Dictionary<string, PendingItem>(StringComparer.Ordinal);

		long _size;

		internal IEnumerable<string> Items => _items.Select(x => x.Key);

		/// <summary>
		/// Constructor
		/// </summary>
		public StorageBackendCache(DirectoryReference cacheDir, long maxSize, ILogger logger)
		{
			_backend = new FileStorageBackend(cacheDir);
			_maxSize = maxSize;
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
		public async Task<IStorageObject> ReadAsync(string key, Func<CancellationToken, Task<Stream>> createStreamAsync, CancellationToken cancellationToken = default)
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
			using Stream stream = await createStreamAsync(cancellationToken);
			long totalLength = stream.Length;

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
					}
					catch (Exception ex)
					{
						_logger.LogDebug(ex, "Unable to delete cache item {Path}: {Message}", item.Path, ex.Message);
					}
				}
			}

			string path = await _backend.WriteAsync(stream, cancellationToken: cancellationToken);
			lock (LockObject)
			{
				Item item = new Item(key, path, totalLength);
				_items.AddFirst(item.ListNode);
				_pathToItem.Add(key, item);

				_pathToPendingItem.Remove(key);
			}
		}
	}
}
