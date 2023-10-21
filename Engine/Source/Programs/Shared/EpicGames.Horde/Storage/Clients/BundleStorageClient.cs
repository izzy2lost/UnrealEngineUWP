// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
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
	public abstract class BundleStorageClientBase : IStorageClient
	{
		/// <summary>
		/// Blob type for bundles
		/// </summary>
		public static BlobType BundleBlobType { get; } = new BlobType(Guid.Parse("{7C5BA294-2D21-4F92-85BE-852F48CC4C1E}"), 1);

		/// <summary>
		/// Handle to a bundle object
		/// </summary>
		protected class BundleHandle : BlobHandle
		{
			readonly BundleStorageClientBase _storageClient;
			readonly BlobLocator _locator;
			List<BlobHandle>? _refs;

			/// <summary>
			/// Constructor
			/// </summary>
			public BundleHandle(BundleStorageClientBase storageClient, BlobLocator locator)
			{
				_storageClient = storageClient;
				_locator = locator;
			}

			/// <inheritdoc/>
			public override ValueTask<BlobType> GetTypeAsync(CancellationToken cancellationToken = default) => new ValueTask<BlobType>(BundleBlobType);

			/// <inheritdoc/>
			public override async ValueTask<IReadOnlyList<BlobHandle>> GetRefsAsync(CancellationToken cancellationToken = default)
			{
				if (_refs == null)
				{
					List<BlobHandle> refs = new List<BlobHandle>();

					BundleHeader header = await _storageClient.ReadHeaderAsync(_locator, cancellationToken);
					foreach (BlobLocator import in header.Imports)
					{
						refs.Add(_storageClient.CreateBlobHandle(new BlobLocator(import.Path)));
					}

					_refs = refs;
				}
				return _refs;
			}

			/// <inheritdoc/>
			public override Task<Stream> OpenAsync(int offset = 0, int? length = null, CancellationToken cancellationToken = default)
			{
				return _storageClient.OpenBlobAsync(_locator, offset, length, cancellationToken);
			}

			/// <inheritdoc/>
			public override async ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default)
			{
				using (Stream stream = await _storageClient.OpenBlobAsync(_locator, 0, cancellationToken: cancellationToken))
				{
					byte[] data = await stream.ReadAllBytesAsync(cancellationToken);
					return new BlobData(BundleBlobType, data, await GetRefsAsync(cancellationToken));
				}
			}

			/// <inheritdoc/>
			public override bool TryGetLocator([NotNullWhen(true)] out BlobLocator locator)
			{
				locator = _locator;
				return true;
			}
		}

		readonly BundleReader _bundleReader;

		/// <inheritdoc/>
		public virtual bool SupportsRedirects { get; } = false;

		/// <summary>
		/// Constructor
		/// </summary>
		protected BundleStorageClientBase(BundleReaderCache cache, ILogger logger)
		{
			_bundleReader = new BundleReader(this, cache, logger);
		}

		/// <inheritdoc/>
		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		/// <summary>
		/// Overridable dispose method
		/// </summary>
		/// <param name="disposing"></param>
		protected virtual void Dispose(bool disposing)
		{
		}

		#region Blobs

		/// <inheritdoc/>
		public async Task<Stream> OpenBlobAsync(BlobLocator locator, int offset = 0, int? length = null, CancellationToken cancellationToken = default) 
			=> await OpenBundleAsync(locator, offset, length, cancellationToken);

		/// <inheritdoc/>
		public async ValueTask<BlobData> ReadBlobAsync(BlobLocator locator, CancellationToken cancellationToken = default)
		{
			BlobHandle handle = CreateBlobHandle(locator);
			return await handle.ReadAsync(cancellationToken);
		}

		/// <inheritdoc/>
		public async ValueTask<BlobHandle> WriteBlobAsync(BlobType type, Stream stream, IReadOnlyList<BlobHandle> references, string? basePath = null, CancellationToken cancellationToken = default)
		{
			if (type == BundleBlobType)
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
		public virtual ValueTask<Uri?> TryGetReadRedirectAsync(BlobLocator locator, CancellationToken cancellationToken = default) => new ValueTask<Uri?>();

		/// <inheritdoc/>
		public virtual ValueTask<(BlobLocator, Uri)?> TryGetWriteRedirectAsync(string? prefix = null, CancellationToken cancellationToken = default) => new ValueTask<(BlobLocator, Uri)?>();

		#endregion

		#region Bundles

		/// <summary>
		/// Read a bundle from the underlying storage
		/// </summary>
		protected abstract Task<Stream> OpenBundleAsync(BlobLocator locator, int offset = 0, int? length = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Write a bundle to the underlying storage
		/// </summary>
		protected abstract ValueTask<BlobHandle> WriteBundleAsync(Stream stream, IReadOnlyList<BlobHandle> references, string? basePath = null, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public Task<BundleHeader> ReadHeaderAsync(BlobLocator locator, CancellationToken cancellationToken) => _bundleReader.ReadHeaderAsync(locator, cancellationToken);

		/// <inheritdoc/>
		public async Task<BlobData> ReadNodeDataAsync(BundleNodeLocator locator, CancellationToken cancellationToken) => await _bundleReader.ReadNodeDataAsync(locator, cancellationToken);

		#endregion

		#region Nodes

		/// <inheritdoc/>
		public BlobHandle CreateBlobHandle(BlobLocator locator)
		{
			if (locator.CanUnwrap())
			{
				return new FlushedNodeHandle(_bundleReader, BundleNodeLocator.FromBlobLocator(locator));
			}
			else
			{
				return new BundleHandle(this, locator);
			}
		}

		/// <inheritdoc/>
		public BundleWriter CreateWriter(string? basePath = null, BundleOptions? options = null) => new BundleWriter(this, _bundleReader, basePath, options);

		/// <inheritdoc/>
		IStorageWriter IStorageClient.CreateWriter(string? basePath) => CreateWriter(basePath);

		#endregion

		#region Aliases

		/// <inheritdoc/>
		public abstract Task AddAliasAsync(string name, BlobHandle handle, int rank, ReadOnlyMemory<byte> data, CancellationToken cancellationToken);

		/// <inheritdoc/>
		public abstract Task RemoveAliasAsync(string name, BlobHandle handle, CancellationToken cancellationToken);

		/// <inheritdoc/>
		public abstract Task<BlobAlias[]> FindAliasesAsync(string name, int? maxLength = null, CancellationToken cancellationToken = default);

		#endregion

		#region Refs

		/// <inheritdoc/>
		public abstract Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public abstract Task<BlobHandle?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public abstract Task WriteRefAsync(RefName name, BlobHandle target, RefOptions? options, CancellationToken cancellationToken);

		#endregion

		/// <inheritdoc/>
		public virtual void GetStats(StorageStats stats) => _bundleReader.GetStats(stats);
	}

	/// <summary>
	/// Base class for an implementation of <see cref="IStorageClient"/>, providing implementations for some common functionality using bundles.
	/// </summary>
	public abstract class BundleStorageClient : BundleStorageClientBase
	{
		readonly IStorageBackend _backend;

		/// <summary>
		/// Backend for this client
		/// </summary>
		public IStorageBackend Backend => _backend;

		/// <summary>
		/// Constructor
		/// </summary>
		protected BundleStorageClient(IStorageBackend backend, BundleReaderCache cache, ILogger logger)
			: base(cache, logger)
		{
			_backend = backend;
		}

		/// <summary>
		/// Overridable dispose method
		/// </summary>
		/// <param name="disposing"></param>
		protected override void Dispose(bool disposing)
		{
			if (disposing)
			{
				_backend.Dispose();
			}

			base.Dispose(disposing);
		}

		/// <inheritdoc/>
		protected override async Task<Stream> OpenBundleAsync(BlobLocator locator, int offset, int? length, CancellationToken cancellationToken)
			=> await _backend.OpenAsync(locator.ToString(), offset, length, cancellationToken);

		/// <inheritdoc/>
		protected override async ValueTask<BlobHandle> WriteBundleAsync(Stream stream, IReadOnlyList<BlobHandle> references, string? basePath = null, CancellationToken cancellationToken = default)
		{
			string path = await _backend.WriteAsync(stream, basePath, cancellationToken);
			return new BundleHandle(this, new BlobLocator(path));
		}

		/// <inheritdoc/>
		public override void GetStats(StorageStats stats)
		{
			_backend.GetStats(stats);
			base.GetStats(stats);
		}
	}

	/// <summary>
	/// Wraps an underlying storage client with functionality that packs nodes into bundles.
	/// </summary>
	public sealed class BundleStorageClientWrapper : BundleStorageClientBase
	{
		readonly IStorageClient _inner;

		/// <summary>
		/// Constructor
		/// </summary>
		public BundleStorageClientWrapper(IStorageClient inner, BundleReaderCache cache, ILogger logger)
			: base(cache, logger)
		{
			_inner = inner;
		}

		/// <summary>
		/// Overridable dispose method
		/// </summary>
		/// <param name="disposing"></param>
		protected override void Dispose(bool disposing)
		{
			if (disposing)
			{
				_inner.Dispose();
			}

			base.Dispose(disposing);
		}

		/// <summary>
		/// Creates a bundle storage client around a memory client backend
		/// </summary>
		public static BundleStorageClientWrapper CreateFromMemory(ILogger logger)
		{
			MemoryStorageClient blobStore = new MemoryStorageClient();
			return new BundleStorageClientWrapper(blobStore, BundleReaderCache.None, logger);
		}

		/// <summary>
		/// Creates a bundle storage client around a directory on the filesystem
		/// </summary>
		public static BundleStorageClientWrapper CreateFromDirectory(DirectoryReference rootDir, BundleReaderCache cache, ILogger logger)
		{
			FileStorageClient fileStorageClient = new FileStorageClient(rootDir, logger);
			return new BundleStorageClientWrapper(fileStorageClient, cache, logger);
		}

		/// <inheritdoc/>
		protected override async Task<Stream> OpenBundleAsync(BlobLocator locator, int offset, int? length, CancellationToken cancellationToken)
			=> await _inner.CreateBlobHandle(locator).OpenAsync(offset, length, cancellationToken);

		/// <inheritdoc/>
		protected override ValueTask<BlobHandle> WriteBundleAsync(Stream stream, IReadOnlyList<BlobHandle> references, string? basePath = null, CancellationToken cancellationToken = default)
			=> _inner.WriteBlobAsync(BundleBlobType, stream, references, basePath, cancellationToken);

		/// <inheritdoc/>
		public override void GetStats(StorageStats stats)
		{
			_inner.GetStats(stats);
			base.GetStats(stats);
		}

		#region Aliases

		/// <inheritdoc/>
		public override Task AddAliasAsync(string name, BlobHandle handle, int rank, ReadOnlyMemory<byte> data, CancellationToken cancellationToken)
			=> _inner.AddAliasAsync(name, handle, rank, data, cancellationToken);

		/// <inheritdoc/>
		public override Task RemoveAliasAsync(string name, BlobHandle handle, CancellationToken cancellationToken)
			=> _inner.RemoveAliasAsync(name, handle, cancellationToken);

		/// <inheritdoc/>
		public override async Task<BlobAlias[]> FindAliasesAsync(string name, int? maxLength = null, CancellationToken cancellationToken = default)
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
		public override Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default)
			=> _inner.DeleteRefAsync(name, cancellationToken);

		/// <inheritdoc/>
		public override async Task<BlobHandle?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			BlobHandle? target = await _inner.TryReadRefAsync(name, cacheTime, cancellationToken);
			if (target != null)
			{
				target = CreateBlobHandle(target.GetLocator());
			}
			return target;
		}

		/// <inheritdoc/>
		public override Task WriteRefAsync(RefName name, BlobHandle target, RefOptions? options, CancellationToken cancellationToken)
			=> _inner.WriteRefAsync(name, target, options, cancellationToken);

		#endregion
	}
}
