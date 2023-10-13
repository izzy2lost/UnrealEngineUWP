// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
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
	/// Extension methods for reading bundles
	/// </summary>
	public static class BundleStorageClientExtensions
	{
		/// <summary>
		/// Reads an entire bundle into memory
		/// </summary>
		/// <param name="handle">Handle to read from</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Bundle that was read</returns>
		public static async Task<Bundle> ReadBundleAsync(this BlobHandle handle, CancellationToken cancellationToken = default)
		{
			using Stream stream = await handle.OpenAsync(cancellationToken: cancellationToken);
			return await Bundle.FromStreamAsync(stream, cancellationToken);
		}

		/// <summary>
		/// Reads an entire bundle into memory
		/// </summary>
		/// <param name="storageClient">Storage client</param>
		/// <param name="locator">Locator for the bundle</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Bundle that was read</returns>
		public static async Task<Bundle> ReadBundleAsync(this IStorageClient storageClient, BlobLocator locator, CancellationToken cancellationToken = default)
		{
			BlobHandle handle = storageClient.CreateBlobHandle(locator);
			using Stream stream = await handle.OpenAsync(cancellationToken: cancellationToken);
			return await Bundle.FromStreamAsync(stream, cancellationToken);
		}

		/// <summary>
		/// Writes an entire bundle to a storage client
		/// </summary>
		/// <param name="storageClient">Storage client</param>
		/// <param name="bundle">Bundle to write</param>
		/// <param name="basePath">Prefix for the uploaded data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Locator for reading the bundle back in</returns>
		public static async Task<BlobHandle> WriteBundleAsync(this IStorageClient storageClient, Bundle bundle, string? basePath = null, CancellationToken cancellationToken = default)
		{
			BlobHandle[] imports = new BlobHandle[bundle.Header.Imports.Count];
			for (int idx = 0; idx < bundle.Header.Imports.Count; idx++)
			{
				imports[idx] = storageClient.CreateBlobHandle(new BlobLocator(bundle.Header.Imports[idx].Path));
			}

			using ReadOnlySequenceStream stream = new ReadOnlySequenceStream(bundle.AsSequence());
			return await storageClient.WriteBlobAsync(BundleStorageClient.BundleBlobType, stream, imports, basePath, cancellationToken);
		}
	}

	/// <summary>
	/// Base class for an implementation of <see cref="IStorageClient"/>, providing implementations for some common functionality using bundles.
	/// </summary>
	public abstract class BundleStorageClient : IStorageClient
	{
		/// <summary>
		/// Blob type for bundles
		/// </summary>
		public static BlobType BundleBlobType { get; } = new BlobType(Guid.Parse("{7C5BA294-2D21-4F92-85BE-852F48CC4C1E}"), 1);

		class BundleHandle : BlobHandle
		{
			readonly BundleStorageClient _storageClient;
			readonly string _path;
			List<BlobHandle>? _refs;

			public BundleHandle(BundleStorageClient storageClient, string path)
			{
				_storageClient = storageClient;
				_path = path;
			}

			public override ValueTask<BlobType> GetTypeAsync(CancellationToken cancellationToken = default) => new ValueTask<BlobType>(BundleBlobType);

			public override async ValueTask<IReadOnlyList<BlobHandle>> GetRefsAsync(CancellationToken cancellationToken = default)
			{
				if (_refs == null)
				{
					List<BlobHandle> refs = new List<BlobHandle>();

					BundleHeader header = await _storageClient.ReadHeaderAsync(new BlobLocator(_path), cancellationToken);
					foreach (BlobLocator import in header.Imports)
					{
						refs.Add(_storageClient.CreateBlobHandle(new BlobLocator(import.Path)));
					}

					_refs = refs;
				}
				return _refs;
			}

			public override Task<Stream> OpenAsync(int offset = 0, int? length = null, CancellationToken cancellationToken = default)
			{
				return _storageClient.OpenAsync(new BlobLocator(_path), offset, length, cancellationToken);
			}

			public override async ValueTask<BlobData> ReadAsync(CancellationToken cancellationToken = default)
			{
				using (Stream stream = await _storageClient.OpenAsync(new BlobLocator(_path), 0, cancellationToken: cancellationToken))
				{
					byte[] data = await stream.ReadAllBytesAsync(cancellationToken);
					return new BlobData(BundleBlobType, data, await GetRefsAsync(cancellationToken));
				}
			}

			public override bool TryGetLocator([NotNullWhen(true)] out BlobLocator locator)
			{
				locator = new BlobLocator(_path);
				return true;
			}
		}

		readonly IStorageBackend _backend;
		readonly BundleReader _bundleReader;

		/// <summary>
		/// Backend for this client
		/// </summary>
		public IStorageBackend Backend => _backend;

		/// <summary>
		/// Constructor
		/// </summary>
		protected BundleStorageClient(IStorageBackend backend, BundleReaderCache cache, ILogger logger)
		{
			_backend = backend;
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
			_backend.Dispose();
		}

		#region Blobs

		/// <inheritdoc/>
		public async Task<Stream> OpenAsync(BlobLocator locator, int offset = 0, int? length = null, CancellationToken cancellationToken = default) => await _backend.OpenAsync(locator.Path.ToString(), offset, length, cancellationToken);

		/// <inheritdoc/>
		public async ValueTask<BlobHandle> WriteBlobAsync(BlobType type, Stream stream, IReadOnlyList<BlobHandle> references, string? basePath = null, CancellationToken cancellationToken = default)
		{
			Debug.Assert(type == BundleBlobType);
			string path = await _backend.WriteAsync(stream, basePath, cancellationToken);
			return new BundleHandle(this, path);
		}

		#endregion

		#region Bundles

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
				return new BundleHandle(this, locator.Path.ToString());
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
		public abstract Task<RefValue?> TryReadRefAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public abstract Task WriteRefAsync(RefName name, BlobHandle target, ReadOnlyMemory<byte> data, RefOptions? options, CancellationToken cancellationToken);

		#endregion

		/// <inheritdoc/>
		public void GetStats(StorageStats stats)
		{
			_backend.GetStats(stats);
			_bundleReader.GetStats(stats);
		}
	}
}
