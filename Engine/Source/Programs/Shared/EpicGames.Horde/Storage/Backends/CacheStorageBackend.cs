// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace EpicGames.Horde.Storage.Backends
{
	/// <summary>
	/// Storage backend that utilizes the local filesystem
	/// </summary>
	public sealed class CacheStorageBackend : IStorageBackend
	{
		readonly string _keyPrefix;
		readonly CacheStorageBackendDetail _cacheStorage;
		readonly IStorageBackend _inner;

		/// <inheritdoc/>
		public bool SupportsRedirects => _inner.SupportsRedirects;

		/// <summary>
		/// Constructor
		/// </summary>
		public CacheStorageBackend(string keyPrefix, CacheStorageBackendDetail cacheStorage, IStorageBackend inner)
		{
			_keyPrefix = keyPrefix;
			_cacheStorage = cacheStorage;
			_inner = inner;
		}

		/// <inheritdoc/>
		public void Dispose() => _inner.Dispose();

		/// <inheritdoc/>
		public async Task<Stream> ReadAsync(string path, int offset, int? length, CancellationToken cancellationToken = default)
		{
			return await _cacheStorage.ReadAsync($"{_keyPrefix}{path}", ctx => _inner.ReadAsync(path, ctx), cancellationToken);
		}

		/// <inheritdoc/>
		public Task<string> WriteAsync(Stream stream, string? prefix = null, CancellationToken cancellationToken = default) => _inner.WriteAsync(stream, prefix, cancellationToken);

#pragma warning disable CS0618 // Type or member is obsolete
		/// <inheritdoc/>
		public Task WriteExplicitPathAsync(string path, Stream stream, CancellationToken cancellationToken = default) => _inner.WriteExplicitPathAsync(path, stream, cancellationToken);
#pragma warning restore CS0618 // Type or member is obsolete

		/// <inheritdoc/>
		public Task DeleteAsync(string path, CancellationToken cancellationToken = default) => _inner.DeleteAsync(path, cancellationToken);

		/// <inheritdoc/>
		public IAsyncEnumerable<string> EnumerateAsync(CancellationToken cancellationToken = default) => _inner.EnumerateAsync(cancellationToken);

		/// <inheritdoc/>
		public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken = default) => _inner.ExistsAsync(path, cancellationToken);

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetReadRedirectAsync(string path, CancellationToken cancellationToken = default) => _inner.TryGetReadRedirectAsync(path, cancellationToken);

		/// <inheritdoc/>
		public ValueTask<(string, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default) => _inner.TryGetWriteRedirectAsync(prefix, cancellationToken);
	}

	/// <summary>
	/// Implementation of a local disk cache which can be shared by multiple backends
	/// </summary>
	public sealed class CacheStorageBackendDetail
	{
		class Item
		{
			public string Key { get; }
			public long Length { get; }
			public LinkedListNode<Item> ListNode { get; }

			public Item(string key, long length)
			{
				Key = key;
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

		object LockObject => _items;

		readonly DirectoryReference _cacheDir;
		readonly long _maxSize;

		readonly LinkedList<Item> _items = new LinkedList<Item>();
		readonly Dictionary<string, Item> _pathToItem = new Dictionary<string, Item>(StringComparer.Ordinal);
		readonly Dictionary<string, PendingItem> _pathToPendingItem = new Dictionary<string, PendingItem>(StringComparer.Ordinal);

		long _size;

		internal IEnumerable<string> Items => _items.Select(x => x.Key);

		/// <summary>
		/// Constructor
		/// </summary>
		public CacheStorageBackendDetail(DirectoryReference cacheDir, long maxSize)
		{
			_cacheDir = cacheDir;
			_maxSize = maxSize;
		}

		/// <inheritdoc/>
		public async Task<Stream> ReadAsync(string key, Func<CancellationToken, Task<Stream>> createStreamAsync, CancellationToken cancellationToken = default)
		{
			for(; ;)
			{
				PendingItem? pendingItem;
				lock (LockObject)
				{
					Item? item;
					if (_pathToItem.TryGetValue(key, out item))
					{
						return OpenItem(item);
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

		Stream OpenItem(Item item)
		{
			_items.Remove(item.ListNode);
			_items.AddFirst(item.ListNode);

			FileReference file = FileReference.Combine(_cacheDir, item.Key);
			return FileReference.Open(file, FileMode.Open, FileAccess.Read, FileShare.Read | FileShare.Delete);
		}

		async Task ReadIntoCacheAsync(string key, Func<CancellationToken, Task<Stream>> createStreamAsync, CancellationToken cancellationToken)
		{
			using Stream stream = await createStreamAsync(cancellationToken);
			long totalLength = stream.Length;

			lock (LockObject)
			{
				_size += totalLength;

				while (_size > _maxSize && _items.Count > 0)
				{
					LinkedListNode<Item> lastItem = _items.Last!;
					_pathToItem.Remove(lastItem.Value.Key);
					_items.RemoveLast();

					_size -= lastItem.Value.Length;

					FileReference file = FileReference.Combine(_cacheDir, lastItem.Value.Key);
					FileReference.Delete(file);
				}
			}

			FileReference cacheFile = FileReference.Combine(_cacheDir, key);
			DirectoryReference.CreateDirectory(cacheFile.Directory);

			using (FileStream outputStream = FileReference.Open(cacheFile, FileMode.Create, FileAccess.Write, FileShare.None))
			{
				await stream.CopyToAsync(outputStream, cancellationToken);
			}

			lock (LockObject)
			{
				Item item = new Item(key, totalLength);
				_items.AddFirst(item.ListNode);
				_pathToItem.Add(key, item);

				_pathToPendingItem.Remove(key);
			}
		}
	}
}
