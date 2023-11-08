// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage.Bundles;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Storage.Clients
{
	/// <summary>
	/// Base class for an implementation of <see cref="IStorageClient"/>, providing implementations for some common functionality using bundles.
	/// </summary>
	public sealed class BundleStorageClient : IStorageClient
	{
		/// <summary>
		/// Handle to a bundle object
		/// </summary>
		class BundleHandle : IBlobHandle
		{
			readonly BundleStorageClient _storageClient;
			readonly BlobLocator _locator;
			List<IBlobHandle>? _refs;

			/// <inheritdoc/>
			public IBlobHandle? Outer => null;

			/// <summary>
			/// Constructor
			/// </summary>
			public BundleHandle(BundleStorageClient storageClient, BlobLocator locator)
			{
				_storageClient = storageClient;
				_locator = locator;
			}

			/// <inheritdoc/>
			public ValueTask<BlobType> ReadTypeAsync(CancellationToken cancellationToken = default) => new ValueTask<BlobType>(Bundle.BlobType);

			/// <inheritdoc/>
			public async ValueTask<IReadOnlyList<IBlobHandle>> ReadImportsAsync(CancellationToken cancellationToken = default)
			{
				if (_refs == null)
				{
					List<IBlobHandle> refs = new List<IBlobHandle>();

					Bundles.V1.BundleHeader header = await _storageClient.ReadHeaderAsync(_locator, cancellationToken);
					foreach (BlobLocator import in header.Imports)
					{
						refs.Add(_storageClient.CreateBlobHandle(new BlobLocator(import.Path)));
					}

					_refs = refs;
				}
				return _refs;
			}

			/// <inheritdoc/>
			public Task<Stream> OpenBodyAsync(int offset = 0, int? length = null, CancellationToken cancellationToken = default)
			{
				return _storageClient.OpenBlobAsync(_locator, offset, length, cancellationToken);
			}

			/// <inheritdoc/>
			public async ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default)
			{
				using (Stream stream = await _storageClient.OpenBlobAsync(_locator, 0, cancellationToken: cancellationToken))
				{
					byte[] data = await stream.ReadAllBytesAsync(cancellationToken);
					return new BlobData(Bundle.BlobType, data, await ReadImportsAsync(cancellationToken));
				}
			}

			/// <inheritdoc/>
			public bool TryAppendIdentifier(Utf8StringBuilder builder)
			{
				builder.Append(_locator.Path);
				return true;
			}

			/// <inheritdoc/>
			public override bool Equals(object? obj) => obj is BundleHandle other && _locator == other._locator;

			/// <inheritdoc/>
			public override int GetHashCode() => _locator.GetHashCode();
		}

		readonly IStorageClient _inner;
		readonly Bundles.V1.BundleReader _bundleReader;

		/// <inheritdoc/>
		public bool SupportsRedirects { get; } = false;

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleStorageClient(IStorageClient inner, BundleCache cache, ILogger logger)
		{
			_inner = inner;
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
			return await handle.ReadAsync(cancellationToken);
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
				return new Bundles.V1.FlushedNodeHandle(_bundleReader, baseLocator, new BundleHandle(this, baseLocator), fragment.Span);
			}
			else
			{
				return new BundleHandle(this, locator);
			}
		}

		/// <inheritdoc/>
		public IStorageWriter CreateWriter(string? basePath = null, BundleOptions? options = null) => new Bundles.V1.BundleWriter(this, _bundleReader, basePath, options);

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
