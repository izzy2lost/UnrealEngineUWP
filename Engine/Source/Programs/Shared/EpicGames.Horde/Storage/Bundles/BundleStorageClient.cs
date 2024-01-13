// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers.Text;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage.Clients;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Storage.Bundles
{
	/// <summary>
	/// Base class for an implementation of <see cref="IStorageClient"/>, providing implementations for some common functionality using bundles.
	/// </summary>
	public sealed class BundleStorageClient : IStorageClient
	{
		readonly IStorageClient _inner;
		readonly BundleCache _cache;
		readonly Bundles.V1.BundleReader _bundleReader;

		internal Bundles.V1.BundleReader BundleReader => _bundleReader;

		/// <summary>
		/// Allocator which trims the cache to keep below a maximum size
		/// </summary>
		public IMemoryAllocator<byte> Allocator => _cache.Allocator;

		/// <summary>
		/// Cache for bundle data
		/// </summary>
		public BundleCache Cache => _cache;

		/// <inheritdoc/>
		public bool SupportsRedirects { get; } = false;

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleStorageClient(IStorageClient inner, BundleCache cache, ILogger logger)
		{
			_inner = inner;
			_cache = cache;
			_bundleReader = new Bundles.V1.BundleReader(this, cache, logger);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			_inner.Dispose();
		}

		/// <summary>
		/// Creates a bundle storage client around a memory client backend
		/// </summary>
		public static BundleStorageClient CreateFromMemory(ILogger logger)
		{
			MemoryStorageClient blobStore = new MemoryStorageClient();
			return new BundleStorageClient(blobStore, BundleCache.None, logger);
		}

		/// <summary>
		/// Creates a bundle storage client around a directory on the filesystem
		/// </summary>
		public static BundleStorageClient CreateFromDirectory(DirectoryReference rootDir, BundleCache cache, ILogger logger)
		{
			FileStorageClient fileStorageClient = new FileStorageClient(rootDir, logger);
			return new BundleStorageClient(fileStorageClient, cache, logger);
		}

		#region Blobs

		/// <inheritdoc/>
		public async Task<Stream> OpenBlobAsync(BlobLocator locator, int offset = 0, int? length = null, CancellationToken cancellationToken = default) 
			=> await OpenBundleAsync(locator, offset, length, cancellationToken);

		/// <inheritdoc/>
		public async ValueTask<BlobData> ReadBlobAsync(BlobLocator locator, CancellationToken cancellationToken = default)
		{
			IBlobHandle handle = CreateBlobHandle(locator);
			return await handle.ReadBlobDataAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async ValueTask<IBlobHandle> WriteBlobAsync(BlobType type, Stream stream, IReadOnlyList<IBlobHandle> references, string? basePath = null, CancellationToken cancellationToken = default)
		{
			if (type == Bundle.BlobType)
			{
				return await WriteBundleAsync(stream, references, basePath, cancellationToken);
			}
			else
			{
				await using IStorageWriter writer = CreateWriter(basePath);

				int length = 0;
				for (; ; )
				{
					int readLength = await stream.ReadAsync(writer.GetOutputBuffer(length, length + 1), cancellationToken);
					if (readLength == 0)
					{
						break;
					}
					length += readLength;
				}

				return await writer.WriteBlobAsync(type, length, references, Array.Empty<AliasInfo>(), cancellationToken);
			}
		}

		/// <inheritdoc/>
		public ValueTask<Uri?> TryGetReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken = default) => new ValueTask<Uri?>();

		/// <inheritdoc/>
		public ValueTask<(BlobLocator, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default) => new ValueTask<(BlobLocator, Uri)?>();

		#endregion

		#region Bundles

		/// <summary>
		/// Read a bundle from the underlying storage
		/// </summary>
		async Task<Stream> OpenBundleAsync(BlobLocator locator, int offset, int? length, CancellationToken cancellationToken)
			=> await _inner.CreateBlobHandle(locator).OpenBodyAsync(offset, length, cancellationToken);

		/// <summary>
		/// Write a bundle to the underlying storage
		/// </summary>
		ValueTask<IBlobHandle> WriteBundleAsync(Stream stream, IReadOnlyList<IBlobHandle> references, string? basePath = null, CancellationToken cancellationToken = default)
			=> _inner.WriteBlobAsync(Bundle.BlobType, stream, references, basePath, cancellationToken);

		/// <inheritdoc/>
		public Task<Bundles.V1.BundleHeader> ReadHeaderAsync(BlobLocator locator, CancellationToken cancellationToken) => _bundleReader.ReadHeaderAsync(locator, cancellationToken);

		#endregion

		#region Nodes

		/// <inheritdoc/>
		public IBlobHandle CreateBlobHandle(BlobLocator locator)
		{
			if (locator.TryUnwrap(out BlobLocator baseLocator, out Utf8String fragment))
			{
				FlushedBundleHandle bundleHandle = new FlushedBundleHandle(this, CreateBlobHandle(baseLocator));

				int exportIdx;
				if (Utf8Parser.TryParse(fragment.Span, out exportIdx, out int numBytesRead) && numBytesRead == fragment.Length)
				{
					return new Bundles.V1.FlushedNodeHandle(_bundleReader, baseLocator, bundleHandle, exportIdx);
				}

				int ampIdx = fragment.IndexOf('&');
				if (ampIdx == -1)
				{
					return new Bundles.V2.FlushedPacketHandle(this, bundleHandle, fragment.Span, _cache);
				}
				else
				{
					return new Bundles.V2.FlushedExportHandle(new Bundles.V2.FlushedPacketHandle(this, bundleHandle, fragment.Slice(0, ampIdx).Span, _cache), fragment.Span.Slice(ampIdx + 1));
				}
			}
			else
			{
				return new FlushedBundleHandle(this, _inner.CreateBlobHandle(baseLocator));
			}
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
		public Task AddAliasAsync(string name, IBlobHandle handle, int rank, ReadOnlyMemory<byte> data, CancellationToken cancellationToken)
			=> _inner.AddAliasAsync(name, handle, rank, data, cancellationToken);

		/// <inheritdoc/>
		public Task RemoveAliasAsync(string name, IBlobHandle handle, CancellationToken cancellationToken)
			=> _inner.RemoveAliasAsync(name, handle, cancellationToken);

		/// <inheritdoc/>
		public async Task<BlobAlias[]> FindAliasesAsync(string name, int? maxLength = null, CancellationToken cancellationToken = default)
		{
			BlobAlias[] aliases = await _inner.FindAliasesAsync(name, maxLength, cancellationToken);
			for (int idx = 0; idx < aliases.Length; idx++)
			{
				BlobAlias alias = aliases[idx];
				aliases[idx] = new BlobAlias(CreateBlobHandle(alias.Target.GetLocator()), alias.Rank, alias.Data);
			}
			return aliases;
		}

		#endregion

		#region Refs

		/// <inheritdoc/>
		public Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default)
			=> _inner.DeleteRefAsync(name, cancellationToken);

		/// <inheritdoc/>
		public async Task<IBlobHandle?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			IBlobHandle? target = await _inner.TryReadRefAsync(name, cacheTime, cancellationToken);
			if (target != null)
			{
				target = CreateBlobHandle(target.GetLocator());
			}
			return target;
		}

		/// <inheritdoc/>
		public Task WriteRefAsync(RefName name, IBlobHandle target, RefOptions? options = null, CancellationToken cancellationToken = default)
			=> _inner.WriteRefAsync(name, target, options, cancellationToken);

		#endregion

		/// <inheritdoc/>
		public void GetStats(StorageStats stats)
		{
			_inner.GetStats(stats);
			_bundleReader.GetStats(stats);
		}
	}
}
