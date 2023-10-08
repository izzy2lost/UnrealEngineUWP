// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage.Bundles;
using Microsoft.Extensions.Logging;

namespace EpicGames.Horde.Storage.Clients
{
	/// <summary>
	/// Interface for bundle storage clients. Used to implement ref-counted redirect wrappers.
	/// </summary>
	public interface IBundleStorageClient : IStorageClient
	{
		/// <summary>
		/// Backend for this client
		/// </summary>
		IStorageBackend Backend { get; }

		#region Blobs

		/// <inheritdoc/>
		Task<Stream> OpenAsync(BundleLocator locator, int offset, int? length = null, CancellationToken cancellationToken = default);

		#endregion

		#region Bundles

		/// <summary>
		/// Reads the header for a bundle
		/// </summary>
		/// <param name="locator">Locator for the bundle</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task<BundleHeader> ReadHeaderAsync(BundleLocator locator, CancellationToken cancellationToken);

		/// <summary>
		/// Reads a node from a bundle
		/// </summary>
		/// <param name="locator">Locator for the bundle</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task<BlobData> ReadNodeDataAsync(BundleNodeLocator locator, CancellationToken cancellationToken);

		#endregion

		#region Nodes

		/// <summary>
		/// Creates a handle to a node from its locator
		/// </summary>
		/// <param name="locator">Locator for the node</param>
		BundleNodeHandle CreateNodeHandle(BundleNodeLocator locator);

		/// <inheritdoc cref="IStorageClient.CreateWriter(RefName)"/>
		BundleWriter CreateWriter(RefName refName = default, BundleOptions? options = null);

		#endregion

		#region Aliases

		/// <inheritdoc cref="IStorageClient.AddAliasAsync(String, BlobHandle, Int32, ReadOnlyMemory{Byte}, CancellationToken)"/>
		Task AddAliasAsync(string name, BundleNodeLocator locator, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default);

		/// <inheritdoc cref="IStorageClient.RemoveAliasAsync(String, BlobHandle, CancellationToken)"/>
		Task RemoveAliasAsync(string name, BundleNodeLocator locator, CancellationToken cancellationToken = default);

		#endregion

		#region Refs

		/// <inheritdoc/>
		new Task<BundleNodeHandle?> TryReadRefTargetAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		Task WriteRefTargetAsync(RefName name, BundleNodeLocator target, RefOptions? options = null, CancellationToken cancellationToken = default);

		#endregion
	}

	/// <summary>
	/// Extension methods for <see cref="IBundleStorageClient"/>
	/// </summary>
	public static class BundleStorageClientExtensions
	{
		/// <summary>
		/// Opens a bundle stream for reading
		/// </summary>
		/// <param name="storageClient">Storage client</param>
		/// <param name="locator">Locator for the bundle</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Stream for reading from the bundle</returns>
		public static Task<Stream> OpenAsync(this IBundleStorageClient storageClient, BundleLocator locator, CancellationToken cancellationToken = default) => storageClient.OpenAsync(locator, 0, null, cancellationToken);

		/// <summary>
		/// Reads an entire bundle into memory
		/// </summary>
		/// <param name="storageClient">Storage client</param>
		/// <param name="locator">Locator for the bundle</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Bundle that was read</returns>
		public static async Task<Bundle> ReadBundleAsync(this IBundleStorageClient storageClient, BundleLocator locator, CancellationToken cancellationToken = default)
		{
			using Stream stream = await storageClient.OpenAsync(locator, cancellationToken);
			return await Bundle.FromStreamAsync(stream, cancellationToken);
		}

		/// <summary>
		/// Writes an entire bundle to a storage client
		/// </summary>
		/// <param name="storageClient">Storage client</param>
		/// <param name="bundle">Bundle to write</param>
		/// <param name="prefix">Prefix for the uploaded data</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Locator for reading the bundle back in</returns>
		public static async Task<BundleLocator> WriteBundleAsync(this IBundleStorageClient storageClient, Bundle bundle, Utf8String prefix = default, CancellationToken cancellationToken = default)
		{
			using ReadOnlySequenceStream stream = new ReadOnlySequenceStream(bundle.AsSequence());
			string path = await storageClient.Backend.WriteAsync(stream, prefix.IsEmpty ? null : prefix.ToString(), cancellationToken);
			return new BundleLocator(path);
		}

		/// <summary>
		/// Reads a ref from the store, throwing an exception if it does not exist
		/// </summary>
		/// <param name="store">The store instance to read from</param>
		/// <param name="name">Id for the ref</param>
		/// <param name="cacheTime">Minimum coherency of any cached result</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The ref target</returns>
		public static async Task<BundleNodeHandle> ReadRefTargetAsync(this IBundleStorageClient store, RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			BundleNodeHandle? refTarget = await store.TryReadRefTargetAsync(name, cacheTime, cancellationToken);
			return refTarget ?? throw new RefNameNotFoundException(name);
		}
	}

	/// <summary>
	/// Base class for an implementation of <see cref="IStorageClient"/>, providing implementations for some common functionality using bundles.
	/// </summary>
	public abstract class BundleStorageClient : IBundleStorageClient
	{
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
		public async Task<Stream> OpenAsync(BundleLocator locator, int offset, int? length = null, CancellationToken cancellationToken = default) => await _backend.OpenAsync(locator.Path.ToString(), offset, length, cancellationToken);

		#endregion

		#region Bundles

		/// <inheritdoc/>
		public Task<BundleHeader> ReadHeaderAsync(BundleLocator locator, CancellationToken cancellationToken) => _bundleReader.ReadHeaderAsync(locator, cancellationToken);

		/// <inheritdoc/>
		public async Task<BlobData> ReadNodeDataAsync(BundleNodeLocator locator, CancellationToken cancellationToken) => await _bundleReader.ReadNodeDataAsync(locator, cancellationToken);

		#endregion

		#region Nodes

		/// <summary>
		/// Creates a handle to a node from its locator
		/// </summary>
		public BundleNodeHandle CreateNodeHandle(BundleNodeLocator locator) => new FlushedNodeHandle(_bundleReader, locator);

		/// <inheritdoc/>
		public BundleWriter CreateWriter(RefName refName = default, BundleOptions? options = null) => new BundleWriter(this, _bundleReader, refName, options);

		/// <inheritdoc/>
		IStorageWriter IStorageClient.CreateWriter(RefName refName) => CreateWriter(refName);

		#endregion

		#region Aliases

		/// <inheritdoc/>
		Task IStorageClient.AddAliasAsync(string name, BlobHandle handle, int rank, ReadOnlyMemory<byte> data, CancellationToken cancellationToken) => AddAliasAsync(name, ((BundleNodeHandle)handle).GetLocator(), rank, data, cancellationToken);

		/// <inheritdoc/>
		public abstract Task AddAliasAsync(string name, BundleNodeLocator handle, int rank = 0, ReadOnlyMemory<byte> data = default, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		Task IStorageClient.RemoveAliasAsync(string name, BlobHandle handle, CancellationToken cancellationToken) => RemoveAliasAsync(name, ((BundleNodeHandle)handle).GetLocator(), cancellationToken);

		/// <inheritdoc/>
		public abstract Task RemoveAliasAsync(string name, BundleNodeLocator locator, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public abstract Task<BlobAlias[]> FindAliasesAsync(string name, int? maxLength = null, CancellationToken cancellationToken = default);

		#endregion

		#region Refs

		/// <inheritdoc/>
		public abstract Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		public abstract Task<BundleNodeHandle?> TryReadRefTargetAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		async Task<BlobHandle?> IStorageClient.TryReadRefTargetAsync(RefName name, RefCacheTime cacheTime, CancellationToken cancellationToken) => await TryReadRefTargetAsync(name, cacheTime, cancellationToken);

		/// <inheritdoc/>
		async Task IStorageClient.WriteRefTargetAsync(RefName name, BlobHandle target, RefOptions? options, CancellationToken cancellationToken)
		{
			await target.FlushAsync(cancellationToken);
			await WriteRefTargetAsync(name, ((BundleNodeHandle)target.Unwrap()).GetLocator(), options, cancellationToken);
		}

		/// <inheritdoc/>
		public abstract Task WriteRefTargetAsync(RefName name, BundleNodeLocator target, RefOptions? options = null, CancellationToken cancellationToken = default);

		#endregion

		/// <inheritdoc/>
		public void GetStats(StorageStats stats)
		{
			_backend.GetStats(stats);
			_bundleReader.GetStats(stats);
		}
	}
}
