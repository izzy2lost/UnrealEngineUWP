// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using System.Runtime.CompilerServices;
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
	public abstract class BundleStorageClient : IStorageClient
	{
		readonly IStorageBackend _backend;

		/// <summary>
		/// Reader for node data
		/// </summary>
		public BundleReader BundleReader { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		protected BundleStorageClient(IStorageBackend backend, StorageCache cache, ILogger logger)
		{
			_backend = backend;
			BundleReader = new BundleReader(this, cache, logger);
		}

		#region Blobs

		/// <summary>
		/// Opens a bundle stream for reading
		/// </summary>
		/// <param name="locator">Locator for the bundle</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Stream for reading from the bundle</returns>
		public Task<Stream> OpenAsync(BundleLocator locator, CancellationToken cancellationToken = default) => OpenAsync(locator, 0, null, cancellationToken);

		/// <inheritdoc/>
		public async Task<Stream> OpenAsync(BundleLocator locator, int offset, int? length = null, CancellationToken cancellationToken = default)
		{
			return await _backend.OpenAsync(locator.Path.ToString(), offset, length, cancellationToken);
		}

		/// <summary>
		/// Reads an entire bundle into memory
		/// </summary>
		/// <param name="locator">Locator for the bundle</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Bundle that was read</returns>
		public async Task<Bundle> ReadBundleAsync(BundleLocator locator, CancellationToken cancellationToken = default)
		{
			using Stream stream = await OpenAsync(locator, cancellationToken);
			return await Bundle.FromStreamAsync(stream, cancellationToken);
		}

		/// <inheritdoc/>
		public async Task<BundleLocator> WriteBundleAsync(Bundle bundle, Utf8String prefix = default, CancellationToken cancellationToken = default)
		{
			using ReadOnlySequenceStream stream = new ReadOnlySequenceStream(bundle.AsSequence());
			string path = await _backend.WriteAsync(stream, prefix.IsEmpty? null : prefix.ToString(), cancellationToken);
			return new BundleLocator(path);
		}

		#endregion

		#region Nodes

		/// <summary>
		/// Creates a handle to a node from its locator
		/// </summary>
		public BlobHandle CreateNodeHandle(BundleNodeLocator locator) => new FlushedNodeHandle(BundleReader, locator);

		/// <inheritdoc/>
		public BundleWriter CreateWriter(RefName refName = default, BundleOptions? options = null)
		{
			return new BundleWriter(this, BundleReader, refName, options);
		}

		/// <inheritdoc/>
		IStorageWriter IStorageClient.CreateWriter(RefName refName) => CreateWriter(refName);

		#endregion

		#region Aliases

		/// <inheritdoc/>
		Task IStorageClient.AddAliasAsync(Utf8String name, BlobHandle handle, int rank, CancellationToken cancellationToken) => AddAliasAsync(name, (BundleNodeHandle)handle, rank, cancellationToken);

		/// <inheritdoc/>
		public abstract Task AddAliasAsync(Utf8String name, BundleNodeHandle handle, int rank = 0, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		Task IStorageClient.RemoveAliasAsync(Utf8String name, BlobHandle handle, CancellationToken cancellationToken) => RemoveAliasAsync(name, (BundleNodeHandle)handle, cancellationToken);

		/// <inheritdoc/>
		public abstract Task RemoveAliasAsync(Utf8String name, BundleNodeHandle handle, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		async IAsyncEnumerable<BlobHandle> IStorageClient.FindAliasAsync(Utf8String name, [EnumeratorCancellation] CancellationToken cancellationToken)
		{
			await foreach (BundleNodeHandle handle in FindAliasAsync(name, cancellationToken))
			{
				yield return handle;
			}
		}

		/// <inheritdoc/>
		public abstract IAsyncEnumerable<BundleNodeHandle> FindAliasAsync(Utf8String name, CancellationToken cancellationToken = default);

		#endregion

		#region Refs

		/// <inheritdoc/>
		public abstract Task<bool> DeleteRefAsync(RefName name, CancellationToken cancellationToken = default);

		/// <inheritdoc cref="StorageClientExtensions.ReadRefTargetAsync(IStorageClient, RefName, RefCacheTime, CancellationToken)"/>
		public async Task<BundleNodeHandle> ReadRefTargetAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default)
		{
			BundleNodeHandle? refTarget = await TryReadRefTargetAsync(name, cacheTime, cancellationToken);
			if (refTarget == null)
			{
				throw new RefNameNotFoundException(name);
			}
			return refTarget;
		}

		/// <inheritdoc/>
		public abstract Task<BundleNodeHandle?> TryReadRefTargetAsync(RefName name, RefCacheTime cacheTime = default, CancellationToken cancellationToken = default);

		/// <inheritdoc/>
		async Task<BlobHandle?> IStorageClient.TryReadRefTargetAsync(RefName name, RefCacheTime cacheTime, CancellationToken cancellationToken) => await TryReadRefTargetAsync(name, cacheTime, cancellationToken);

		/// <inheritdoc/>
		public Task WriteRefTargetAsync(RefName name, BlobHandle target, RefOptions? options = null, CancellationToken cancellationToken = default)
		{
			return WriteRefTargetAsync(name, (BundleNodeHandle)target, options, cancellationToken);
		}

		/// <inheritdoc/>
		public abstract Task WriteRefTargetAsync(RefName name, BundleNodeHandle target, RefOptions? options = null, CancellationToken cancellationToken = default);

		#endregion
	}
}
