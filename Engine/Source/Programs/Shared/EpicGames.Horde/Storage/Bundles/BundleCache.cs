// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics.CodeAnalysis;
using Microsoft.Extensions.Caching.Memory;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Options for creating a storage cache
	/// </summary>
	public class BundleCacheOptions
	{
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
	/// Implements caching functionality for reading from a storage client
	/// </summary>
	public sealed class BundleCache : IDisposable
	{
		readonly MemoryCache? _headerCache;
		readonly MemoryCache? _packetCache;

		/// <summary>
		/// Instance of an empty cache
		/// </summary>
		public static BundleCache None { get; } = new BundleCache(new BundleCacheOptions { HeaderCacheSize = 0, PacketCacheSize = 0 });

		/// <summary>
		/// Size of the configured header cache
		/// </summary>
		public long HeaderCacheSize { get; }

		/// <summary>
		/// Size of the configured packet cache
		/// </summary>
		public long PacketCacheSize { get; }

		/// <summary>
		/// Whether there is a packet cache present
		/// </summary>
		public bool HasPacketCache => _packetCache != null;

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleCache() : this(new BundleCacheOptions()) 
		{ 
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleCache(BundleCacheOptions options)
		{
			if (options.HeaderCacheSize > 0)
			{
				HeaderCacheSize = options.HeaderCacheSize;
				_headerCache = new MemoryCache(new MemoryCacheOptions { SizeLimit = options.HeaderCacheSize });
			}
			if (options.PacketCacheSize > 0)
			{
				PacketCacheSize = options.PacketCacheSize;
				_packetCache = new MemoryCache(new MemoryCacheOptions { SizeLimit = options.PacketCacheSize });
			}
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_headerCache?.Dispose();
			_packetCache?.Dispose();
		}

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
	}
}
