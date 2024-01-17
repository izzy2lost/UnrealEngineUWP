// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers.Text;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage.Backends;
using EpicGames.Horde.Storage.Bundles.V1;
using EpicGames.Horde.Storage.Bundles.V2;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Storage.Bundles
{
	/// <summary>
	/// Base class for an implementation of <see cref="IStorageClient"/>, providing implementations for some common functionality using bundles.
	/// </summary>
	public sealed class BundleStorageClient : IStorageClient
	{
		readonly IStorageBackend _backend;
		readonly BundleCache _cache;
		readonly PacketReaderStats _packetReaderStats = new PacketReaderStats();
		readonly Bundles.V1.BundleReader _bundleReader;

		internal Bundles.V1.BundleReader BundleReader => _bundleReader;

		/// <summary>
		/// Allocator which trims the cache to keep below a maximum size
		/// </summary>
		public IMemoryAllocator<byte> Allocator => _cache.Allocator;

		/// <summary>
		/// Accessor for the storage backend
		/// </summary>
		public IStorageBackend Backend => _backend;

		/// <summary>
		/// Cache for bundle data
		/// </summary>
		public BundleCache Cache => _cache;

		/// <summary>
		/// Stats for reading bundles
		/// </summary>
		internal PacketReaderStats PacketReaderStats => _packetReaderStats;

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleStorageClient(IStorageBackend backend, BundleCache cache, ILogger logger)
		{
			_backend = backend;
			_cache = cache;
			_bundleReader = new Bundles.V1.BundleReader(this, cache, logger);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_backend.Dispose();
		}

		/// <summary>
		/// Creates a bundle storage client around a memory client backend
		/// </summary>
		public static BundleStorageClient CreateInMemory(ILogger logger)
		{
			MemoryStorageBackend backend = new MemoryStorageBackend();
			return new BundleStorageClient(backend, BundleCache.None, logger);
		}

		/// <summary>
		/// Creates a bundle storage client around a directory on the filesystem
		/// </summary>
		public static BundleStorageClient CreateFromDirectory(DirectoryReference rootDir, BundleCache cache, ILogger logger)
		{
			FileStorageBackend backend = new FileStorageBackend(rootDir, logger);
			return new BundleStorageClient(backend, cache, logger);
		}

		/// <summary>
		/// Helper method for GC which allows enumerating all references to other bundles
		/// </summary>
		public async Task<IEnumerable<BlobLocator>> ReadBundleReferencesAsync(BlobLocator locator, CancellationToken cancellationToken)
		{
			IReadOnlyMemoryOwner<byte> data = await Backend.ReadBlobAsync(locator, 0, 32 * 1024 * 1024, cancellationToken);
			return ReadBundleReferences(data.Memory);
		}

		/// <summary>
		/// Helper method for GC which allows enumerating all references to other bundles
		/// </summary>
		IEnumerable<BlobLocator> ReadBundleReferences(ReadOnlyMemory<byte> data)
		{
			BundleSignature signature = BundleSignature.Read(data.Span);
			if (signature.Version <= BundleVersion.LatestV1)
			{
				return ReadImportsFromDataV1(data);
			}
			else if (signature.Version <= BundleVersion.LatestV2)
			{
				return ReadImportsFromDataV2(data);
			}
			else
			{
				throw new InvalidOperationException($"Unsupported bundle version {(int)signature.Version}");
			}
		}

		static IEnumerable<BlobLocator> ReadImportsFromDataV1(ReadOnlyMemory<byte> data)
		{
			BundleHeader header = BundleHeader.Read(data);
			return header.Imports.Select(x => x.BaseLocator);
		}

		IEnumerable<BlobLocator> ReadImportsFromDataV2(ReadOnlyMemory<byte> data)
		{
			HashSet<BlobLocator> locators = new HashSet<BlobLocator>();
			while (data.Length > 0)
			{
				BundleSignature signature = BundleSignature.Read(data.Span);

				using IRefCountedHandle<Bundles.V2.Packet> packet = Bundles.V2.Packet.Decode(data, Cache.Allocator);
				for (int idx = 0; idx < packet.Target.GetImportCount(); idx++)
				{
					PacketImport import = packet.Target.GetImport(idx);
					if (import.BaseIdx == -1)
					{
						locators.Add(new BlobLocator(import.Fragment.Clone()));
					}
				}

				data = data.Slice(signature.HeaderLength);
			}
			return locators;
		}

		#region Nodes

		/// <inheritdoc/>
		public IBlobHandle CreateBlobHandle(BlobLocator locator)
		{
			if (!locator.TryUnwrap(out BlobLocator baseLocator, out Utf8String fragment))
			{
				throw new Exception($"{locator} is not valid");
			}

			FlushedBundleHandle bundleHandle = new FlushedBundleHandle(this, baseLocator);

			int exportIdx;
			if (Utf8Parser.TryParse(fragment.Span, out exportIdx, out int numBytesRead) && numBytesRead == fragment.Length)
			{
				return new Bundles.V1.FlushedNodeHandle(_bundleReader, baseLocator, bundleHandle, exportIdx);
			}

			int ampIdx = fragment.IndexOf('&');
			if (ampIdx == -1)
			{
				throw new Exception($"{locator} is not valid");
			}

			Bundles.V2.PacketHandle packetHandle = new Bundles.V2.FlushedPacketHandle(this, bundleHandle, fragment.Slice(0, ampIdx).Span, _cache);
			return new Bundles.V2.ExportHandle(packetHandle, fragment.Slice(ampIdx + 1));
		}

		/// <inheritdoc/>
		public IStorageWriter CreateWriter(string? basePath = null, BundleOptions? options = null)
		{
			options ??= BundleOptions.Default;

			if (options.MaxVersion == BundleVersion.LatestV1)
			{
				return new Bundles.V1.BundleWriter(this, _bundleReader, basePath, options);
			}
			else if(options.MaxVersion == BundleVersion.LatestV2)
			{
				return new Bundles.V2.BundleWriter(this, basePath, _cache, options);
			}
			else
			{
				throw new InvalidOperationException($"Unsupported bundle version: {(int)options.MaxVersion}");
			}
		}

		/// <inheritdoc/>
		IStorageWriter IStorageClient.CreateWriter(string? basePath) => CreateWriter(basePath);

		#endregion

		#region Aliases

		/// <inheritdoc/>
		public async Task AddAliasAsync(string name, IBlobHandle handle, int rank, ReadOnlyMemory<byte> data, CancellationToken cancellationToken)
		{
			await handle.FlushAsync(cancellationToken);
			await _backend.AddAliasAsync(name, handle.GetLocator(), rank, data, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task RemoveAliasAsync(string name, IBlobHandle handle, CancellationToken cancellationToken)
		{
			await handle.FlushAsync(cancellationToken);
			await _backend.RemoveAliasAsync(name, handle.GetLocator(), cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<BlobAlias[]> FindAliasesAsync(string name, int? maxLength = null, CancellationToken cancellationToken = default)
		{
			BlobAliasLocator[] aliases = await _backend.FindAliasesAsync(name, maxLength, cancellationToken);

			BlobAlias[] result = new BlobAlias[aliases.Length];
			for (int idx = 0; idx < aliases.Length; idx++)
			{
				BlobAliasLocator alias = aliases[idx];
				result[idx] = new BlobAlias(CreateBlobHandle(alias.Target), alias.Rank, alias.Data);
			}
			return result;
		}

		#endregion

		#region Refs

		/// <inheritdoc/>
		public Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default)
			=> _backend.DeleteRefAsync(name, cancellationToken);

		/// <inheritdoc/>
		public async Task<IBlobHandle?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			BlobLocator? target = await _backend.TryReadRefAsync(name, cacheTime, cancellationToken);
			if (target == null)
			{
				return null;
			}
			return CreateBlobHandle(target.Value);
		}

		/// <inheritdoc/>
		public async Task WriteRefAsync(RefName name, IBlobHandle target, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			await target.FlushAsync(cancellationToken);
			await _backend.WriteRefAsync(name, target.GetLocator(), options, cancellationToken);
		}

		#endregion

		/// <inheritdoc/>
		public void GetStats(StorageStats stats)
		{
			_cache.GetStats(stats);
			_backend.GetStats(stats);
			_bundleReader.GetStats(stats);
			_packetReaderStats.GetStats(stats);
		}
	}
}
