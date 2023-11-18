// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Caching.Memory;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Options for creating a storage cache
	/// </summary>
	public class BundleCacheOptions
	{
		/// <summary>
		/// Maximum size of the cache
		/// </summary>
		public long MaxSize { get; set; } = 128 * 1024 * 1024;

		/// <summary>
		/// Size of the header cache
		/// </summary>
		public long HeaderCacheSize { get; set; } = 64 * 1024 * 1024;

		/// <summary>
		/// Size of the packet cache
		/// </summary>
		public long PacketCacheSize { get; set; } = 192 * 1024 * 1024;
	}

	/// <summary>
	/// Caches items with user-defined keys, where initialization of each value is performed asynchronously. 
	/// Memory allocations are tracked via calls to Reserve(), and a hard upper limit is enforced as long as data can be freed.
	/// </summary>
	public sealed class BundleCache : IAsyncDisposable
	{
		class CacheValue : IDisposable
		{
			public object Key { get; }
			public LinkedListNode<CacheValue> Node { get; }
			public Task<IDisposable> InitTask { get; }
			int _refCount;

			public int RefCount => Interlocked.CompareExchange(ref _refCount, 0, 0);

			public CacheValue(object key, IDisposable value)
			{
				_refCount = 1;
				Key = key;
				Node = new LinkedListNode<CacheValue>(this);
				InitTask = Task.FromResult<IDisposable>(value);
			}

			public CacheValue(object key, Func<Task<IDisposable>> initTask)
			{
				_refCount = 2; // Will be released by RunAndUnlock
				Key = key;
				Node = new LinkedListNode<CacheValue>(this);
				InitTask = Task.Run(() => RunAndUnlockAsync(initTask));
			}

			public void Dispose()
			{
				InitTask.Result.Dispose();
			}

			async Task<IDisposable> RunAndUnlockAsync(Func<Task<IDisposable>> initTask)
			{
				try
				{
					return await initTask();
				}
				finally
				{
					Release();
				}
			}

			public void AddRef() => Interlocked.Increment(ref _refCount);
			public void Release() => Interlocked.Decrement(ref _refCount);
		}

		class CacheValueHandle<T> : IRefCountedHandle<T> where T : class, IDisposable
		{
			CacheValue? _item;
			T? _target;

			public int RefCount => _item?.RefCount ?? throw new ObjectDisposedException(nameof(CacheValueHandle<T>));

			public T Target => _target ?? throw new ObjectDisposedException(nameof(CacheValueHandle<T>));

			public CacheValueHandle(CacheValue? item, T? value)
			{
				_item = item;
				_target = value;
			}

			public void Dispose()
			{
				if (_item != null)
				{
					_item.Release();
					_item = null;
					_target = null;
				}
			}

			public IRefCountedHandle<T> AddRef()
			{
				if (_item == null)
				{
					throw new ObjectDisposedException(nameof(CacheValueHandle<T>));
				}

				_item.AddRef();
				return new CacheValueHandle<T>(_item, _target);
			}
		}

		// Tracks an owned blocks of memory against the cache budged.
		sealed class MemoryAllocation : IMemoryOwner<byte>
		{
			readonly BundleCache _outer;
			IMemoryOwner<byte> _owner;

			public Memory<byte> Memory => _owner.Memory;

			public MemoryAllocation(BundleCache outer, IMemoryOwner<byte> owner)
			{
				_outer = outer;
				_owner = owner;
			}

			/// <inheritdoc/>
			public void Dispose()
			{
				if (_owner != null)
				{
					long size = _owner.Memory.Length;
					_owner.Dispose();
					_outer.ReleaseSpace(size);
					_owner = null!;
				}
			}
		}

		// Custom memory allocator which tracks allocated blocks against the cache's budget. Older cache entries will be 
		// disposed to create space for new allocations.
		class MemoryAllocator : IMemoryAllocator<byte>
		{
			readonly BundleCache _outer;
			readonly IMemoryAllocator<byte> _inner;

			public MemoryAllocator(BundleCache outer, IMemoryAllocator<byte> inner)
			{
				_outer = outer;
				_inner = inner;
			}

			public IMemoryOwner<byte> Alloc(int minSize)
			{
				_outer.CreateSpace(minSize);
				IMemoryOwner<byte> owner = _inner.Alloc(minSize);
				_outer.CreateSpace(owner.Memory.Length - minSize);
				return new MemoryAllocation(_outer, owner);
			}
		}

		readonly object _lockObject = new object();
		readonly BundleCacheOptions _options;
		readonly LinkedList<CacheValue> _items = new LinkedList<CacheValue>();
		readonly Dictionary<object, CacheValue> _itemLookup = new Dictionary<object, CacheValue>();
		readonly CancellationTokenSource _cancellationSource = new CancellationTokenSource();
		readonly MemoryAllocator _allocator;

		long _currentSize;

		readonly MemoryCache? _headerCache;
		readonly MemoryCache? _packetCache;

		/// <summary>
		/// Instance of an empty cache
		/// </summary>
		public static BundleCache None { get; } = new BundleCache(new BundleCacheOptions { MaxSize = 0, HeaderCacheSize = 0, PacketCacheSize = 0 });

		/// <summary>
		/// Accessor for the default allocator
		/// </summary>
		public IMemoryAllocator<byte> Allocator => _allocator;

		/// <summary>
		/// Current size of data allocated or in the cache
		/// </summary>
		public long CurrentSize => _currentSize;

		/// <summary>
		/// Size of the configured header cache
		/// </summary>
		public long HeaderCacheSize => _options.HeaderCacheSize;

		/// <summary>
		/// Size of the configured packet cache
		/// </summary>
		public long PacketCacheSize => _options.PacketCacheSize;

		/// <summary>
		/// Whether there is a packet cache present
		/// </summary>
		public bool HasPacketCache => _packetCache != null;

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleCache() 
			: this(new BundleCacheOptions()) 
		{ 
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="options">Options for the cache</param>
		public BundleCache(BundleCacheOptions options)
			: this(options, PoolAllocator.Shared)
		{
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="options">Options for the cache</param>
		/// <param name="innerAllocator">Inner allocator to use. Will be wrapped in an allocator that tracks allocations against the cache's budget.</param>
		public BundleCache(BundleCacheOptions options, IMemoryAllocator<byte> innerAllocator)
		{
			_options = options;
			_allocator = new MemoryAllocator(this, innerAllocator);

			if (options.HeaderCacheSize > 0)
			{
				_headerCache = new MemoryCache(new MemoryCacheOptions { SizeLimit = options.HeaderCacheSize });
			}
			if (options.PacketCacheSize > 0)
			{
				_packetCache = new MemoryCache(new MemoryCacheOptions { SizeLimit = options.PacketCacheSize });
			}
		}

		/// <inheritdoc/>
		public async ValueTask DisposeAsync()
		{
			_headerCache?.Dispose();
			_packetCache?.Dispose();

			if (_items.Count > 0)
			{
				_cancellationSource.Cancel();

				foreach (CacheValue item in _items)
				{
					try
					{
						await item.InitTask;
					}
					catch (OperationCanceledException)
					{
					}
				}

				_items.Clear();
			}

			_cancellationSource.Dispose();
		}

		/// <summary>
		/// Empty the cache
		/// </summary>
		public void Trim()
		{
			CreateSpace(_options.MaxSize);
			ReleaseSpace(_options.MaxSize);
		}

		/// <summary>
		/// Find or add a new cached value to the cache
		/// </summary>
		/// <param name="key">Key for the lookup</param>
		/// <returns></returns>
		public IRefCountedHandle<TValue>? Find<TKey, TValue>(TKey key)
			where TKey : notnull
			where TValue : class, IDisposable
		{
			lock (_lockObject)
			{
				CacheValue? item;
				if (_itemLookup.TryGetValue(key, out item) && item.InitTask.TryGetResult(out IDisposable result))
				{
					item.AddRef();
					return new CacheValueHandle<TValue>(item, (TValue)result);
				}
			}
			return null;
		}

		/// <summary>
		/// Attempts to add a value to the cache. 
		/// </summary>
		/// <param name="key">Key to add the item to the cache with</param>
		/// <param name="value">Value to be added. If the item is added, ownership is implicitly transferred to the cache.</param>
		/// <returns>True if the value was added</returns>
		public bool TryAdd<TKey, TValue>(TKey key, TValue value)
			where TKey : notnull
			where TValue : class, IDisposable
		{
			lock (_lockObject)
			{
				if (!_itemLookup.ContainsKey(key))
				{
					CacheValue item = new CacheValue(key, value);
					_items.AddFirst(item);
					_itemLookup.Add(key, item);
					return true;
				}
			}
			return false;
		}

		/// <summary>
		/// Find or add a new cached value to the cache
		/// </summary>
		/// <param name="key"></param>
		/// <param name="createAsync"></param>
		/// <param name="cancellationToken"></param>
		/// <returns>Handle to the item that was read. Must be disposed by the caller.</returns>
		public async Task<IRefCountedHandle<TValue>> FindOrAddAsync<TKey, TValue>(TKey key, Func<TKey, CancellationToken, Task<TValue>> createAsync, CancellationToken cancellationToken = default)
			where TKey : notnull
			where TValue : class, IDisposable
		{
			CacheValue? item;
			lock (_lockObject)
			{
				CacheValue? untypedItem;
				if (_itemLookup.TryGetValue(key, out untypedItem))
				{
					item = untypedItem;
				}
				else
				{
					item = new CacheValue(key, async () => await createAsync(key, _cancellationSource.Token));
					_items.AddFirst(item);
					_itemLookup.Add(key, item);
				}
			}

			TValue value = (TValue)await item.InitTask.WaitAsync(cancellationToken);
			item.AddRef();
			return new CacheValueHandle<TValue>(item, value);
		}

		void CreateSpace(long size)
		{
			if (Interlocked.Add(ref _currentSize, size) > _options.MaxSize)
			{
				lock (_lockObject)
				{
					for (LinkedListNode<CacheValue>? lastNode = _items.Last; lastNode != null && Interlocked.CompareExchange(ref _currentSize, 0, 0) > _options.MaxSize; lastNode = lastNode.Previous)
					{
						CacheValue lastItem = lastNode.Value;
						if (lastItem.RefCount == 1)
						{
							_items.Remove(lastItem);
							_itemLookup.Remove(lastItem.Key);
							lastItem.Dispose();
						}
					}
				}
			}
		}

		void ReleaseSpace(long size)
		{
			Interlocked.Add(ref _currentSize, -size);
		}

		#region V1

		static void AddCachedValue(IMemoryCache? cache, string cacheKey, object value, int size)
		{
			if (cache != null)
			{
				using (ICacheEntry entry = cache.CreateEntry(cacheKey))
				{
					entry.SetValue(value);
					entry.SetSize(size);
				}
			}
		}

		static bool TryGetCachedValue<T>(IMemoryCache? cache, string cacheKey, [MaybeNull, NotNullWhen(true)] out T value)
		{
			if (cache == null)
			{
				value = default;
				return false;
			}
			return cache.TryGetValue(cacheKey, out value);
		}

		static string GetBundleInfoCacheKey(BlobLocator locator) => $"bundle:{locator}";
		static string GetEncodedPacketCacheKey(BlobLocator locator, int packetIdx) => $"encoded-packet:{locator}#{packetIdx}";
		static string GetDecodedPacketCacheKey(BlobLocator locator, int packetIdx) => $"decoded-packet:{locator}#{packetIdx}";

		/// <summary>
		/// Adds a bundle info object to the cache
		/// </summary>
		public void AddCachedHeader(BlobLocator locator, Bundles.V1.BundleInfo bundleInfo)
		{
			AddCachedValue(_headerCache, GetBundleInfoCacheKey(locator), bundleInfo, bundleInfo.HeaderLength);
		}

		/// <summary>
		/// Try to read a bundle info object from the cache
		/// </summary>
		public bool TryGetCachedHeader(BlobLocator locator, [NotNullWhen(true)] out Bundles.V1.BundleInfo? bundleInfo)
		{
			return TryGetCachedValue(_headerCache, GetBundleInfoCacheKey(locator), out bundleInfo);
		}

		/// <summary>
		/// Adds an encoded bundle packet to the cache
		/// </summary>
		public void AddCachedEncodedPacket(BlobLocator locator, int packetIdx, ReadOnlyMemory<byte> data)
		{
			AddCachedValue(_packetCache, GetEncodedPacketCacheKey(locator, packetIdx), data, data.Length);
		}

		/// <summary>
		/// Try to read an encoded bundle packet from the cache
		/// </summary>
		public bool TryGetCachedEncodedPacket(BlobLocator locator, int packetIdx, out ReadOnlyMemory<byte> data)
		{
			return TryGetCachedValue(_packetCache, GetEncodedPacketCacheKey(locator, packetIdx), out data);
		}

		/// <summary>
		/// Adds a decoded bundle packet to the cache
		/// </summary>
		public void AddCachedDecodedPacket(BlobLocator locator, int packetIdx, ReadOnlyMemory<byte> data)
		{
			AddCachedValue(_packetCache, GetDecodedPacketCacheKey(locator, packetIdx), data, data.Length);
		}

		/// <summary>
		/// Try to read a decoded bundle packet from the cache
		/// </summary>
		public bool TryGetCachedDecodedPacket(BlobLocator locator, int packetIdx, out ReadOnlyMemory<byte> data)
		{
			return TryGetCachedValue(_packetCache, GetDecodedPacketCacheKey(locator, packetIdx), out data);
		}

		#endregion
	}
}
